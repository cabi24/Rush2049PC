#!/usr/bin/env python3
"""A fake home router on 127.0.0.1 for the port-mapping tests (docs/port/NETWORK.md 13): never the real one.

    fake_router.py [--pmp pcp|natpmp|none] [--upnp | --no-upnp] [--external A.B.C.D] [--refuse CODE]
                   [--permanent-only] [--conflict] [--hostile KIND]

It prints the environment that points rush2049pc (or net_test nat) at it and runs until Ctrl-C, logging each
request; the tests import it (FakeRouter) instead:

  - an SSDP responder (UDP): answers an M-SEARCH (unicast, as RUSH_NAT_SSDP sends it) with LOCATION
    http://127.0.0.1:HTTP/rootDesc.xml;
  - its HTTP server: the device description (an InternetGatewayDevice:1 with a WANIPConnection:1 at /ctl/IPConn) and
    the SOAP control: GetExternalIPAddress, AddPortMapping, DeletePortMapping (UPnP IGD's WANIPConnection:1);
  - a PCP (RFC 6887) and NAT-PMP (RFC 6886) server (UDP), as RUSH_NAT_GATEWAY names it: --pmp pcp answers both
    versions, natpmp answers PCP's requests with NAT-PMP's "unsupported version" as an old server does, none listens
    on nothing (the port closed: an ICMP port unreachable);
  - the mappings with their leases: one not renewed expires, as a real router's does (the "expire" event).

Every request and change is an event (time, protocol, what, details), which the tests read. --hostile makes the
UPnP side answer badly: garbage, a LOCATION on another address, oversized fields, a description without a WAN
service, a chunked answer, a truncated one. Written from the same RFCs and specifications as port/net/net_nat.c.
Stdlib only, Python 3.9+.
"""
import argparse
import http.server
import ipaddress
import socket
import socketserver
import struct
import sys
import threading
import time
import xml.etree.ElementTree as ET

DESC = """<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
<specVersion><major>1</major><minor>0</minor></specVersion>
<device>
<deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</deviceType>
<friendlyName>Fake &amp; Test Router</friendlyName>
<manufacturer>Rush2049PC tests</manufacturer>
<modelName>FakeIGD</modelName>
<UDN>uuid:00000000-0000-0000-0000-000000002049</UDN>
<serviceList><service>
<serviceType>urn:schemas-upnp-org:service:Layer3Forwarding:1</serviceType>
<serviceId>urn:upnp-org:serviceId:L3Forwarding1</serviceId>
<controlURL>/ctl/L3F</controlURL><eventSubURL>/evt/L3F</eventSubURL><SCPDURL>/L3F.xml</SCPDURL>
</service></serviceList>
<deviceList><device>
<deviceType>urn:schemas-upnp-org:device:WANDevice:1</deviceType>
<friendlyName>WANDevice</friendlyName>
<deviceList><device>
<deviceType>urn:schemas-upnp-org:device:WANConnectionDevice:1</deviceType>
<friendlyName>WANConnectionDevice</friendlyName>
<serviceList><service>
<serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType>
<serviceId>urn:upnp-org:serviceId:WANIPConn1</serviceId>
<controlURL>/ctl/IPConn</controlURL><eventSubURL>/evt/IPConn</eventSubURL><SCPDURL>/WANIPCn.xml</SCPDURL>
</service></serviceList>
</device></deviceList>
</device></deviceList>
</device>
</root>
"""
WAN = "urn:schemas-upnp-org:service:WANIPConnection:1"
SOAP_NS = "http://schemas.xmlsoap.org/soap/envelope/"
FAULT = """<?xml version="1.0"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"
 s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
<s:Body><s:Fault><faultcode>s:Client</faultcode><faultstring>UPnPError</faultstring><detail>
<UPnPError xmlns="urn:schemas-upnp-org:control-1-0"><errorCode>{code}</errorCode>
<errorDescription>{desc}</errorDescription></UPnPError></detail></s:Fault></s:Body></s:Envelope>
"""
UPNP_ERRORS = {402: "Invalid Args", 501: "Action Failed", 606: "Action not authorized", 714: "NoSuchEntryInArray",
               718: "ConflictInMappingEntry", 725: "OnlyPermanentLeasesSupported"}


class FakeRouter:
    def __init__(self, pmp="pcp", upnp=True, external="203.0.113.5", refuse=None, permanent_only=False,
                 conflict=False, hostile=None, quiet=True):
        self.pmp, self.upnp, self.external = pmp, upnp, external
        self.refuse, self.permanent_only, self.conflict, self.hostile = refuse, permanent_only, conflict, hostile
        self.quiet = quiet
        self.t0 = time.monotonic()
        self.epoch0 = int(time.time()) - 1000
        self.lock = threading.Lock()
        self.events = []
        self.maps = {}   # (proto, external port) -> dict(client, internal, lease, expires, desc)
        self.stopping = False
        self.threads = []
        # PCP / NAT-PMP: a UDP port; "none" takes a port and closes it (nothing listens: port unreachable)
        self.pmp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.pmp_sock.bind(("127.0.0.1", 0))
        self.pmp_port = self.pmp_sock.getsockname()[1]
        if pmp == "none":
            self.pmp_sock.close()
            self.pmp_sock = None
        # SSDP and HTTP
        self.ssdp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.ssdp_sock.bind(("127.0.0.1", 0))
        self.ssdp_port = self.ssdp_sock.getsockname()[1]
        router = self

        class Handler(http.server.BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def log_message(self, *a):
                pass

            def do_GET(self):
                router.http_get(self)

            def do_POST(self):
                router.http_post(self)

        class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
            daemon_threads = True
            allow_reuse_address = True

        self.http = Server(("127.0.0.1", 0), Handler)
        self.http_port = self.http.server_address[1]
        if not upnp:
            self.ssdp_sock.close()
            self.ssdp_sock = None

    # ------------------------------------------------------------ events

    def event(self, proto, what, **kw):
        with self.lock:
            e = dict(t=time.monotonic() - self.t0, proto=proto, what=what, **kw)
            self.events.append(e)
        if not self.quiet:
            print(f"fake_router: {e['t']:7.2f} {proto} {what} {kw}", flush=True)

    def count(self, proto=None, what=None):
        with self.lock:
            return sum(1 for e in self.events if (proto is None or e["proto"] == proto) and
                       (what is None or e["what"] == what))

    def mapping(self, proto, port):
        with self.lock:
            return dict(self.maps[(proto, port)]) if (proto, port) in self.maps else None

    def env(self):
        e = {"RUSH_NAT_SSDP": f"127.0.0.1:{self.ssdp_port}" if self.upnp else "off",
             "RUSH_NAT_GATEWAY": f"127.0.0.1:{self.pmp_port}"}
        return e

    # ------------------------------------------------------------ the mappings

    def add(self, proto, ext, client, internal, lease, desc=""):
        now = time.monotonic()
        with self.lock:
            key = (proto, ext)
            renew = key in self.maps
            self.maps[key] = dict(client=client, internal=internal, lease=lease, desc=desc,
                                  expires=now + lease if lease else None)
        self.event(proto, "renew" if renew else "add", port=ext, client=client, internal=internal, lease=lease)

    def delete(self, proto, ext):
        with self.lock:
            had = self.maps.pop((proto, ext), None)
        self.event(proto, "delete" if had else "delete-none", port=ext)
        return had is not None

    def expirer(self):
        while not self.stopping:
            now = time.monotonic()
            gone = []
            with self.lock:
                for k, m in list(self.maps.items()):
                    if m["expires"] is not None and now >= m["expires"]:
                        gone.append(k)
                        del self.maps[k]
            for proto, port in gone:
                self.event(proto, "expire", port=port)
            time.sleep(0.05)

    # ------------------------------------------------------------ SSDP

    def ssdp_loop(self):
        s = self.ssdp_sock
        s.settimeout(0.2)
        while not self.stopping:
            try:
                data, src = s.recvfrom(4096)
            except socket.timeout:
                continue
            except OSError:
                break
            text = data.decode(errors="replace")
            if not text.startswith("M-SEARCH * HTTP/1.1"):
                continue
            st = ""
            for line in text.split("\r\n"):
                if line.lower().startswith("st:"):
                    st = line[3:].strip()
            self.event("ssdp", "search", st=st)
            if "InternetGatewayDevice:1" not in st:
                continue   # one answer a search round: to the IGD:1 search
            loc = f"http://127.0.0.1:{self.http_port}/rootDesc.xml"
            h = self.hostile
            if h == "garbage":
                s.sendto(bytes(range(256)) * 4, src)
                continue
            if h == "elsewhere":
                loc = f"http://10.255.255.1:{self.http_port}/rootDesc.xml"
            if h == "long-location":
                loc = "http://127.0.0.1:%d/%s" % (self.http_port, "a" * 2000)
            reply = ("HTTP/1.1 200 OK\r\nCACHE-CONTROL: max-age=120\r\nST: %s\r\nUSN: uuid:00000000-0000-0000-0000-"
                     "000000002049::%s\r\nEXT:\r\nSERVER: FakeOS/1 UPnP/1.1 FakeIGD/1\r\nLOCATION: %s\r\n\r\n"
                     % (st, st, loc))
            s.sendto(reply.encode(), src)

    # ------------------------------------------------------------ HTTP: the description and SOAP

    def send(self, h, code, body, ctype="text/xml; charset=\"utf-8\"", chunked=False, cut=False):
        data = body.encode() if isinstance(body, str) else body
        h.send_response(code)
        h.send_header("Content-Type", ctype)
        h.send_header("Connection", "close")
        if chunked:
            h.send_header("Transfer-Encoding", "chunked")
            h.end_headers()
            for i in range(0, len(data), 97):
                part = data[i:i + 97]
                h.wfile.write(b"%x\r\n%s\r\n" % (len(part), part))
            h.wfile.write(b"0\r\n\r\n")
        else:
            h.send_header("Content-Length", str(len(data) + (500 if cut else 0)))
            h.end_headers()
            h.wfile.write(data)
        h.close_connection = True

    def http_get(self, h):
        if h.path != "/rootDesc.xml":
            self.send(h, 404, "no")
            return
        self.event("http", "description")
        desc = DESC
        if self.hostile == "no-wan":
            desc = desc.replace(WAN, "urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1")
        elif self.hostile == "long-control":
            desc = desc.replace("/ctl/IPConn", "/" + "c" * 5000)
        elif self.hostile == "control-elsewhere":
            desc = desc.replace("<controlURL>/ctl/IPConn", "<controlURL>http://10.255.255.1:80/ctl/IPConn")
        elif self.hostile == "xml-garbage":
            desc = "<root><device><serviceList><service><serviceType>" + "<" * 3000
        self.send(h, 200, desc, chunked=self.hostile == "chunked", cut=self.hostile == "cut")

    def soap_args(self, body):
        root = ET.fromstring(body)
        b = root.find(f"{{{SOAP_NS}}}Body")
        action = list(b)[0]
        args = {child.tag.split("}")[-1]: (child.text or "") for child in action}
        return action.tag.split("}")[-1], args

    def fault(self, h, code):
        self.send(h, 500, FAULT.format(code=code, desc=UPNP_ERRORS.get(code, "Error")))

    def http_post(self, h):
        n = int(h.headers.get("Content-Length", "0"))
        body = h.rfile.read(n).decode(errors="replace")
        soapaction = h.headers.get("SOAPAction", "").strip('"')
        if h.path != "/ctl/IPConn" or not soapaction.startswith(WAN + "#"):
            self.send(h, 404, "no")
            return
        try:
            action, args = self.soap_args(body)
        except Exception as e:   # noqa: BLE001 (a test fake: anything malformed is a 402)
            self.event("upnp", "malformed", error=str(e))
            self.fault(h, 402)
            return
        if action != soapaction.split("#")[1]:
            self.fault(h, 402)
            return
        if action == "GetExternalIPAddress":
            self.event("upnp", "get-external")
            if self.hostile == "long-external":
                ext = "9" * 4000
            else:
                ext = self.external
            self.send(h, 200, f'<?xml version="1.0"?>\n<s:Envelope xmlns:s="{SOAP_NS}"><s:Body>'
                              f'<u:GetExternalIPAddressResponse xmlns:u="{WAN}"><NewExternalIPAddress>{ext}'
                              f'</NewExternalIPAddress></u:GetExternalIPAddressResponse></s:Body></s:Envelope>\r\n')
            return
        if action == "AddPortMapping":
            ext, lease = int(args.get("NewExternalPort", "0")), int(args.get("NewLeaseDuration", "0"))
            if args.get("NewProtocol") != "UDP" or not ext:
                self.fault(h, 402)
                return
            if self.refuse:
                self.event("upnp", "refused", code=self.refuse)
                self.fault(h, self.refuse)
                return
            if self.conflict:
                self.event("upnp", "refused", code=718)
                self.fault(h, 718)
                return
            if self.permanent_only and lease:
                self.event("upnp", "refused", code=725)
                self.fault(h, 725)
                return
            self.add("upnp", ext, args.get("NewInternalClient"), int(args.get("NewInternalPort", "0")), lease,
                     args.get("NewPortMappingDescription", ""))
            self.send(h, 200, f'<?xml version="1.0"?>\n<s:Envelope xmlns:s="{SOAP_NS}"><s:Body>'
                              f'<u:AddPortMappingResponse xmlns:u="{WAN}"/></s:Body></s:Envelope>\r\n')
            return
        if action == "DeletePortMapping":
            if not self.delete("upnp", int(args.get("NewExternalPort", "0"))):
                self.fault(h, 714)
                return
            self.send(h, 200, f'<?xml version="1.0"?>\n<s:Envelope xmlns:s="{SOAP_NS}"><s:Body>'
                              f'<u:DeletePortMappingResponse xmlns:u="{WAN}"/></s:Body></s:Envelope>\r\n')
            return
        self.fault(h, 401)

    # ------------------------------------------------------------ PCP and NAT-PMP

    def epoch(self):
        return int(time.time()) - self.epoch0

    def pmp_loop(self):
        s = self.pmp_sock
        s.settimeout(0.2)
        while not self.stopping:
            try:
                data, src = s.recvfrom(2048)
            except socket.timeout:
                continue
            except OSError:
                break
            if not data:
                continue
            if data[0] == 0:
                reply = self.natpmp(data, src)
            elif data[0] == 2:
                reply = self.pcp(data, src)
            else:
                reply = None
            if reply:
                s.sendto(reply, src)

    def natpmp(self, d, src):
        if len(d) < 2:
            return None
        op = d[1]
        if op == 0 and len(d) >= 2:
            self.event("natpmp", "get-external")
            return struct.pack("!BBHI4s", 0, 128, 0, self.epoch(), socket.inet_aton(self.external))
        if op == 1 and len(d) >= 12:
            _, _, _, internal, ext, life = struct.unpack("!BBHHHI", d[:12])
            if self.refuse:
                self.event("natpmp", "refused", code=2)
                return struct.pack("!BBHIHHI", 0, 129, 2, self.epoch(), internal, 0, 0)
            if life == 0:
                gone = [k for k, m in self.maps.items() if k[0] == "natpmp" and m["internal"] == internal]
                for k in gone:
                    self.delete("natpmp", k[1])
                if not gone:
                    self.event("natpmp", "delete-none", internal=internal)
                return struct.pack("!BBHIHHI", 0, 129, 0, self.epoch(), internal, 0, 0)
            ext = ext or internal
            self.add("natpmp", ext, src[0], internal, life)
            return struct.pack("!BBHIHHI", 0, 129, 0, self.epoch(), internal, ext, life)
        return struct.pack("!BBHI", 0, 128 + op, 5, self.epoch())   # unsupported opcode

    def pcp(self, d, src):
        if self.pmp == "natpmp":   # an old NAT-PMP server: PCP's version is "unsupported"
            self.event("natpmp", "unsupported-version")
            return struct.pack("!BBHI", 0, 128 | (d[1] & 0x7F) if len(d) > 1 else 128, 1, self.epoch())
        if len(d) < 60 or d[1] != 1:
            return None
        life = struct.unpack("!I", d[4:8])[0]
        client = ipaddress.IPv6Address(d[8:24])
        nonce, proto = d[24:36], d[36]
        internal, ext = struct.unpack("!HH", d[40:44])
        result = 0
        mapped = client.ipv4_mapped
        if mapped is None or str(mapped) != src[0]:
            result = 12   # ADDRESS_MISMATCH
        elif proto != 17:
            result = 9
        elif self.refuse:
            result = 2    # NOT_AUTHORIZED
        out_ext = ext or internal
        if result == 0:
            if life == 0:
                if not self.delete("pcp", out_ext):
                    pass
            else:
                self.add("pcp", out_ext, src[0], internal, life)
        else:
            self.event("pcp", "refused", code=result)
        ext_ip = ipaddress.IPv6Address("::ffff:" + self.external).packed
        head = struct.pack("!BBBBII", 2, 0x81, 0, result, life if result == 0 else 30, self.epoch()) + bytes(12)
        return head + nonce + bytes([17, 0, 0, 0]) + struct.pack("!HH", internal, out_ext if result == 0 else 0) + \
            (ext_ip if result == 0 else bytes(16))

    # ------------------------------------------------------------ running

    def start(self):
        for fn in (self.expirer, self.http.serve_forever) + ((self.ssdp_loop,) if self.ssdp_sock else ()) + \
                ((self.pmp_loop,) if self.pmp_sock else ()):
            t = threading.Thread(target=fn, daemon=True)
            t.start()
            self.threads.append(t)
        return self

    def stop(self):
        self.stopping = True
        self.http.shutdown()
        self.http.server_close()
        for s in (self.ssdp_sock, self.pmp_sock):
            if s:
                s.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--pmp", choices=["pcp", "natpmp", "none"], default="pcp")
    ap.add_argument("--no-upnp", action="store_true")
    ap.add_argument("--external", default="203.0.113.5")
    ap.add_argument("--refuse", type=int)
    ap.add_argument("--permanent-only", action="store_true")
    ap.add_argument("--conflict", action="store_true")
    ap.add_argument("--hostile")
    a = ap.parse_args()
    r = FakeRouter(a.pmp, not a.no_upnp, a.external, a.refuse, a.permanent_only, a.conflict, a.hostile,
                   quiet=False).start()
    print(" ".join(f"{k}={v}" for k, v in r.env().items()), flush=True)
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        r.stop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
