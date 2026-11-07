#!/usr/bin/env python3
"""The router's port mapping (port/net/net_nat.c, docs/port/NETWORK.md 13) against a fake router on 127.0.0.1
(tools/port/fake_router.py), through `nat_test run`: never the real router (nat_test refuses to run without the test
hooks, and every case sets them).

    nat_check.py --exe NAT_TEST CASE

upnp       UPnP only (nothing on PCP's port): the search, the description, GetExternalIPAddress, AddPortMapping of
           the port with a 4 s lease (RUSH_NAT_LEASE), renewed every 2 s, the external address said; at the end
           DeletePortMapping, and nothing left in the router's table.
pcp        PCP only: a MAP request with the lease, renewals, MAP with lifetime 0 at the end.
natpmp     an old NAT-PMP server: PCP's request answered "unsupported version", then NAT-PMP: the external address,
           the mapping, renewals, its removal.
order      PCP and UPnP both there: PCP first, and UPnP never searched for.
kill       the process killed (it exits at once, as a crash) once mapped: no removal; the router's lease ends the
           mapping (an "expire" within the lease and a margin).
keep       stopped with keep (this PC's game going, to come back): no removal, the lease ends it.
refused    UPnP's 606 and PCP's NOT_AUTHORIZED: the port isn't open, "refused" with the router's words.
cgnat      the router's external address in 100.64.0.0/10: mapped, then taken back at once; "cgnat".
double     a private external address (another router outside): mapped and kept; "double".
norouter   nothing answers (no SSDP, nothing on PCP's port): "no-router", within the search's time.
permanent  an IGD that takes only permanent mappings (725): asked again with lease 0, no renewals, removed at the end.
conflict   the port forwarded to another PC already (718): "conflict".
hostile    the UPnP side answering badly (garbage, a LOCATION or control URL on another address, oversized fields, a
           description without a WAN service, XML garbage, a body cut short): no crash, the port not open, and the
           other address never connected to; a chunked description: mapped.

Exit status: 0 when it holds. Stdlib only, Python 3.9+.
"""
import argparse
import os
import re
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fake_router  # noqa: E402

LEASE = 4


def say(*a):
    print("nat_check:", *a, flush=True)


def run(a, router, seconds=5.0, extra=(), env_extra=None, timeout=60):
    env = dict(os.environ)
    for k in ("RUSH_NAT_SSDP", "RUSH_NAT_GATEWAY", "RUSH_NAT_LEASE"):
        env.pop(k, None)
    env.update(router.env())
    env["RUSH_NAT_LEASE"] = str(LEASE)
    env.update(env_extra or {})
    t0 = time.monotonic()
    p = subprocess.run([a.exe, "run", "--seconds", str(seconds)] + list(extra), env=env, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, timeout=timeout)
    out = p.stdout.decode(errors="replace")
    return p.returncode, out, time.monotonic() - t0


def states(out):
    return [dict(re.findall(r"(\w+)=('[^']*'|\S+)", l)) | {"state": l.split()[1]}
            for l in out.splitlines() if l.startswith("state ")]


def last(out):
    s = states(out)
    return s[-1] if s else {}


def port_of(out):
    m = re.search(r"^port (\d+)", out, re.M)
    return int(m.group(1)) if m else 0


def check(ok, what, out=""):
    if ok:
        say("ok:", what)
        return 0
    say("FAILED:", what)
    if out:
        print(out, flush=True)
    return 1


def mapped_case(a, proto, router, add_proto=None):
    """Mapped by proto, renewed, removed at the end, nothing left."""
    bad = 0
    code, out, took = run(a, router, seconds=5.2)
    port = port_of(out)
    s = last(out)
    first = next((x for x in states(out) if x["state"] == "mapped"), {})
    key = add_proto or proto.lower().replace("-", "")
    bad |= check(code == 0 and first.get("proto") == proto and first.get("external") == f"203.0.113.5:{port}",
                 f"{proto}: mapped, its external address 203.0.113.5:{port} said ({first})", out)
    adds, renews, dels = router.count(key, "add"), router.count(key, "renew"), router.count(key, "delete")
    bad |= check(adds == 1 and renews >= 2 and dels == 1 and router.mapping(key, port) is None,
                 f"{proto}: one add, {renews} renewals in 5 s at a {LEASE} s lease (every {LEASE // 2} s), one "
                 f"removal at the end, nothing left in the router's table", out)
    bad |= check(int(s.get("renewals", "0")) == renews and "stopped: removed=1" in out,
                 f"{proto}: the program counted {s.get('renewals')} renewals and said the removal", out)
    adds_ev = [e for e in router.events if e["what"] == "add" and e["proto"] == key]
    if adds_ev:
        e = adds_ev[0]
        bad |= check(e["internal"] == port and e["lease"] == LEASE and e["client"] == "127.0.0.1",
                     f"{proto}: the mapping is this PC's port {port}, from 127.0.0.1, lease {LEASE} s ({e})")
    return bad, out


def case_upnp(a):
    r = fake_router.FakeRouter(pmp="none", upnp=True).start()
    try:
        bad, out = mapped_case(a, "UPnP", r)
        m = [e for e in r.events if e["what"] == "add"]
        bad |= check("name='Fake & Test Router'" in out and r.count("http", "description") == 1 and
                     r.count("upnp", "get-external") == 1,
                     "UPnP: the description read once, its friendlyName, GetExternalIPAddress once", out)
        if r.maps:
            bad |= check(False, f"UPnP: left in the table: {r.maps}")
        # the description given to the mapping: Rush2049PC
        bad |= check(any(True for _ in m), "UPnP: AddPortMapping seen")
        return bad
    finally:
        r.stop()


def case_pcp(a):
    r = fake_router.FakeRouter(pmp="pcp", upnp=False).start()
    try:
        bad, out = mapped_case(a, "PCP", r)
        bad |= check(r.count("ssdp") == 0, "PCP: UPnP not tried (RUSH_NAT_SSDP off)")
        return bad
    finally:
        r.stop()


def case_natpmp(a):
    r = fake_router.FakeRouter(pmp="natpmp", upnp=False).start()
    try:
        bad, out = mapped_case(a, "NAT-PMP", r, add_proto="natpmp")
        bad |= check(r.count("natpmp", "unsupported-version") >= 1 and r.count("natpmp", "get-external") >= 1 and
                     "speaks NAT-PMP, not PCP" in out,
                     "NAT-PMP: PCP's request answered \"unsupported version\", then NAT-PMP's external address", out)
        return bad
    finally:
        r.stop()


def case_order(a):
    r = fake_router.FakeRouter(pmp="pcp", upnp=True).start()
    try:
        code, out, took = run(a, r, seconds=1.0)
        s = next((x for x in states(out) if x["state"] == "mapped"), {})
        return check(s.get("proto") == "PCP" and r.count("ssdp") == 0 and r.count("upnp") == 0 and
                     r.count("pcp", "delete") == 1,
                     f"PCP and UPnP both there: PCP took it, UPnP never searched for ({r.count('ssdp')} searches)",
                     out)
    finally:
        r.stop()


def case_kill(a):
    bad = 0
    for pmp, upnp, key in (("none", True, "upnp"), ("pcp", False, "pcp")):
        r = fake_router.FakeRouter(pmp=pmp, upnp=upnp).start()
        try:
            code, out, took = run(a, r, seconds=1.0, extra=["--crash"])
            port = port_of(out)
            t_exit = time.monotonic() - r.t0
            held = r.mapping(key, port) is not None
            deadline = time.monotonic() + LEASE + 3
            while r.mapping(key, port) is not None and time.monotonic() < deadline:
                time.sleep(0.05)
            exp = [e for e in r.events if e["what"] == "expire" and e["proto"] == key]
            gone_after = exp[0]["t"] - t_exit if exp else None
            bad |= check(code == 9 and held and r.count(key, "delete") == 0 and exp and gone_after <= LEASE + 0.5,
                         f"{key}: the process killed once mapped: no removal; the mapping there after it, then the "
                         f"router's lease ended it {gone_after if gone_after is None else round(gone_after, 2)} s "
                         f"later (lease {LEASE} s)", out)
        finally:
            r.stop()
    return bad


def case_keep(a):
    r = fake_router.FakeRouter(pmp="none", upnp=True).start()
    try:
        code, out, took = run(a, r, seconds=1.0, extra=["--keep"])
        port = port_of(out)
        held = r.mapping("upnp", port) is not None
        time.sleep(LEASE + 1)
        return check(code == 0 and held and r.count("upnp", "delete") == 0 and r.count("upnp", "expire") == 1 and
                     "left to its lease" in out,
                     "stopped with keep: no removal, the mapping left to its lease, which ends it", out)
    finally:
        r.stop()


def case_refused(a):
    bad = 0
    r = fake_router.FakeRouter(pmp="none", upnp=True, refuse=606).start()
    try:
        code, out, took = run(a, r, seconds=1.0)
        s = last(out)
        f = next((x for x in states(out) if x["state"] == "failed"), {})
        bad |= check(f.get("why") == "refused" and "UPnP error 606 (Action not authorized)" in f.get("detail", "") and
                     not r.maps, f"UPnP refuses (606): failed, refused, its words: {f.get('detail')}", out)
    finally:
        r.stop()
    r = fake_router.FakeRouter(pmp="pcp", upnp=False, refuse=2).start()
    try:
        code, out, took = run(a, r, seconds=1.0)
        f = next((x for x in states(out) if x["state"] == "failed"), {})
        bad |= check(f.get("why") == "refused" and "NOT_AUTHORIZED" in f.get("detail", "") and not r.maps,
                     f"PCP refuses (NOT_AUTHORIZED): failed, refused: {f.get('detail')}", out)
    finally:
        r.stop()
    return bad


def case_cgnat(a):
    bad = 0
    for pmp, upnp, key in (("none", True, "upnp"), ("pcp", False, "pcp")):
        r = fake_router.FakeRouter(pmp=pmp, upnp=upnp, external="100.72.1.5").start()
        try:
            code, out, took = run(a, r, seconds=1.0)
            f = next((x for x in states(out) if x["state"] == "failed"), {})
            bad |= check(f.get("why") == "cgnat" and r.count(key, "add") == 1 and r.count(key, "delete") == 1 and
                         not r.maps and "carrier-grade NAT" in out,
                         f"{key}: an external address in 100.64/10: mapped, taken back at once, said cgnat", out)
        finally:
            r.stop()
    r = fake_router.FakeRouter(pmp="none", upnp=True, external="0.0.0.0").start()
    try:
        code, out, took = run(a, r, seconds=1.0)
        f = next((x for x in states(out) if x["state"] == "failed"), {})
        bad |= check(f.get("why") == "no-internet" and not r.maps,
                     "an external address of 0.0.0.0: no internet, taken back", out)
    finally:
        r.stop()
    return bad


def case_double(a):
    r = fake_router.FakeRouter(pmp="none", upnp=True, external="192.168.0.5").start()
    try:
        code, out, took = run(a, r, seconds=1.0)
        s = next((x for x in states(out) if x["state"] == "mapped"), {})
        return check(s.get("why") == "double" and r.count("upnp", "delete") == 1 and "double NAT" in out,
                     "a private external address: mapped and kept (said double NAT), removed at the end", out)
    finally:
        r.stop()


def case_norouter(a):
    r = fake_router.FakeRouter(pmp="none", upnp=True).start()
    try:
        env = {"RUSH_NAT_SSDP": f"127.0.0.1:{fake_router_free_port()}"}   # nobody answers the search there
        code, out, took = run(a, r, seconds=5.0, env_extra=env)
        f = next((x for x in states(out) if x["state"] == "failed"), {})
        return check(f.get("why") == "no-router" and float(f.get("t", "99")) < 2.5 + 1.5 + 1 and
                     "nothing listens" in out and r.count("ssdp") == 0,
                     f"no router: PCP's port closed (nothing listens: at once), no answer to the search: no-router "
                     f"after {f.get('t')} s", out)
    finally:
        r.stop()


def fake_router_free_port():
    import socket
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def case_permanent(a):
    r = fake_router.FakeRouter(pmp="none", upnp=True, permanent_only=True).start()
    try:
        code, out, took = run(a, r, seconds=5.0)
        s = next((x for x in states(out) if x["state"] == "mapped"), {})
        adds = [e for e in r.events if e["what"] == "add"]
        return check(s.get("lease") == "0" and adds and adds[0]["lease"] == 0 and r.count("upnp", "renew") == 0 and
                     r.count("upnp", "delete") == 1 and not r.maps,
                     "an IGD with only permanent mappings (725): asked again with lease 0, not renewed, removed at "
                     "the end", out)
    finally:
        r.stop()


def case_conflict(a):
    r = fake_router.FakeRouter(pmp="none", upnp=True, conflict=True).start()
    try:
        code, out, took = run(a, r, seconds=1.0)
        f = next((x for x in states(out) if x["state"] == "failed"), {})
        return check(f.get("why") == "conflict" and "718" in f.get("detail", ""),
                     f"the port forwarded to another PC already (718): conflict: {f.get('detail')}", out)
    finally:
        r.stop()


def case_hostile(a):
    bad = 0
    # (kind, the end wanted, a line the log must have)
    for kind, want, line in (("garbage", "no-router", "isn't one"),
                             ("elsewhere", "no-router", "a description on another address"),
                             ("long-location", "no-router", "no answer to the search"),
                             ("no-wan", "no-router", "has no WANIPConnection or WANPPPConnection service"),
                             ("long-control", "no-router", "has no WANIPConnection or WANPPPConnection service"),
                             ("control-elsewhere", "no-router", "has no WANIPConnection or WANPPPConnection service"),
                             ("xml-garbage", "no-router", "has no WANIPConnection or WANPPPConnection service"),
                             ("cut", "no-router", "not an HTTP 200 answer"),
                             ("long-external", "mapped", "external=none"),
                             ("chunked", "mapped", "external=203.0.113.5")):
        r = fake_router.FakeRouter(pmp="none", upnp=True, hostile=kind).start()
        try:
            code, out, took = run(a, r, seconds=3.5)
            ss = states(out)
            end = ss[-1] if ss else {}
            got = "mapped" if end.get("state") == "mapped" else end.get("why")
            ok = code == 0 and got == want and "stopped:" in out and line in out and not r.maps
            bad |= check(ok, f"hostile {kind}: {got} (want {want}), no crash, '{line}'", out)
        finally:
            r.stop()
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("case", choices=["upnp", "pcp", "natpmp", "order", "kill", "keep", "refused", "cgnat", "double",
                                     "norouter", "permanent", "conflict", "hostile"])
    a = ap.parse_args()
    a.exe = str(Path(a.exe).resolve())
    bad = globals()["case_" + a.case](a)
    say("all held" if not bad else "FAILED")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
