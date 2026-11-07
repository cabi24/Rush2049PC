/* port/net/tests/nat_test.c: the router's port mapping's tests (port/net/net_nat.h, docs/port/NETWORK.md 13).
 *
 *   nat_test unit           PCP's and NAT-PMP's messages, SSDP's, the URLs, the XML reader, the device description,
 *                           SOAP's body and faults, HTTP answers (Content-Length, chunked, cut short), the join hint;
 *                           malformed and hostile input each refused: oversized fields, missing tags, garbage
 *   nat_test fuzz [N]       every parser fed N random and mutated inputs (no crash, nothing read or written out of
 *                           bounds: run it under AddressSanitizer)
 *   nat_test run [--seconds S] [--keep] [--crash]
 *                           a NetNat against the fake router RUSH_NAT_SSDP / RUSH_NAT_GATEWAY name
 *                           (tools/port/nat_check.py runs tools/port/fake_router.py): its states as they change,
 *                           then after S seconds stopped (the mapping removed; --keep: left to its lease) or, with
 *                           --crash, the process ends at once as a crash would. It refuses to run without those
 *                           hooks: never the real router.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_nat.h"
#include "net_os.h"
#include "net_sock.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

static int g_fail, g_checks;

#define CHECK(c, ...)                                                                                                  \
    do {                                                                                                              \
        g_checks++;                                                                                                   \
        if (!(c)) {                                                                                                   \
            g_fail++;                                                                                                 \
            printf("FAIL %s:%d: %s: ", __FILE__, __LINE__, #c);                                                       \
            printf(__VA_ARGS__);                                                                                      \
            printf("\n");                                                                                             \
            fflush(stdout);                                                                                           \
        }                                                                                                             \
    } while (0)

static const char DESC[] =
    "<?xml version=\"1.0\"?>\n<root xmlns=\"urn:schemas-upnp-org:device-1-0\">\n"
    "<!-- a comment with <tags> in it -->\n"
    "<device><deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</deviceType>\n"
    "<friendlyName>Fake &amp; Test Router</friendlyName>\n"
    "<serviceList><service><serviceType>urn:schemas-upnp-org:service:Layer3Forwarding:1</serviceType>"
    "<controlURL>/ctl/L3F</controlURL></service></serviceList>\n"
    "<deviceList><device><deviceType>urn:schemas-upnp-org:device:WANDevice:1</deviceType>"
    "<deviceList><device><deviceType>urn:schemas-upnp-org:device:WANConnectionDevice:1</deviceType>"
    "<serviceList>"
    "<service><serviceType>urn:schemas-upnp-org:service:WANPPPConnection:1</serviceType>"
    "<controlURL>/ctl/PPPConn</controlURL></service>"
    "<service><serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType>"
    "<serviceId>urn:upnp-org:serviceId:WANIPConn1</serviceId>\n"
    "<controlURL>\n  /ctl/IPConn  \n</controlURL><eventSubURL>/evt/IPConn</eventSubURL></service>"
    "</serviceList></device></deviceList></device></deviceList></device></root>\n";

static const char EXT_OK[] =
    "<?xml version=\"1.0\"?>\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body>"
    "<u:GetExternalIPAddressResponse xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\">"
    "<NewExternalIPAddress>203.0.113.5</NewExternalIPAddress></u:GetExternalIPAddressResponse></s:Body>"
    "</s:Envelope>\r\n";

static const char FAULT[] =
    "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body><s:Fault>"
    "<faultcode>s:Client</faultcode><faultstring>UPnPError</faultstring><detail>"
    "<UPnPError xmlns=\"urn:schemas-upnp-org:control-1-0\"><errorCode>718</errorCode>"
    "<errorDescription>ConflictInMappingEntry</errorDescription></UPnPError></detail></s:Fault></s:Body>"
    "</s:Envelope>";

static void test_pmp(void)
{
    uint8_t b[64], ext[4];
    int r, ip, ep;
    uint32_t life;
    CHECK(net_pmp_external_req(b, sizeof b) == 2 && b[0] == 0 && b[1] == 0, "the external address request");
    CHECK(net_pmp_map_req(b, sizeof b, 24049, 24050, 600) == 12, "a map request is 12 bytes");
    static const uint8_t map[12] = {0, 1, 0, 0, 0x5D, 0xF1, 0x5D, 0xF2, 0, 0, 0x02, 0x58};
    CHECK(!memcmp(b, map, 12), "its bytes (RFC 6886 3.3): version 0, opcode 1, ports, lifetime 600");
    CHECK(net_pmp_map_req(b, 11, 24049, 0, 600) == -1 && net_pmp_map_req(b, 64, 0, 0, 1) == -1 &&
              net_pmp_map_req(b, 64, 70000, 0, 1) == -1, "no room, or ports that aren't ones");
    static const uint8_t ea[12] = {0, 128, 0, 0, 0, 0, 1, 0, 203, 0, 113, 5};
    CHECK(net_pmp_parse_external(ea, 12, &r, ext) == 0 && r == 0 && ext[0] == 203 && ext[3] == 5, "its answer");
    CHECK(net_pmp_parse_external(ea, 11, &r, ext) == -1, "a success cut short");
    static const uint8_t ee[8] = {0, 128, 0, 3, 0, 0, 1, 0};
    CHECK(net_pmp_parse_external(ee, 8, &r, ext) == 0 && r == 3, "an error answer (network failure) is 8 bytes");
    static const uint8_t ma[16] = {0, 129, 0, 0, 0, 0, 1, 0, 0x5D, 0xF1, 0x5D, 0xF3, 0, 0, 1, 0x2C};
    CHECK(net_pmp_parse_map(ma, 16, &r, &ip, &ep, &life) == 0 && r == 0 && ip == 24049 && ep == 24051 && life == 300,
          "a map answer: the router's own port and lifetime");
    CHECK(net_pmp_parse_map(ma, 15, &r, &ip, &ep, &life) == -1, "cut short");
    CHECK(net_pmp_parse_map(ea, 12, &r, &ip, &ep, &life) == -1, "another opcode's answer");
    uint8_t v1[16];
    memcpy(v1, ma, 16);
    v1[0] = 1;
    CHECK(net_pmp_parse_map(v1, 16, &r, &ip, &ep, &life) == -1, "another version");
    static const uint8_t mr[8] = {0, 129, 0, 2, 0, 0, 0, 9};
    CHECK(net_pmp_parse_map(mr, 8, &r, &ip, &ep, &life) == 0 && r == 2, "refused (2), short");
    CHECK(!strcmp(net_pmp_result_name(2), "not authorized or refused") && !strcmp(net_pmp_result_name(99),
                                                                                   "unknown result"), "names");
}

static void test_pcp(void)
{
    uint8_t b[1200], nonce[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    NetAddr me;
    NetPcpAnswer a;
    memset(&me, 0, sizeof me);
    me.family = NET_AF_INET;
    me.addr[0] = 192, me.addr[1] = 168, me.addr[2] = 1, me.addr[3] = 23;
    CHECK(net_pcp_map_req(b, sizeof b, &me, nonce, 24049, 24049, 600) == 60, "a MAP request is 60 bytes");
    CHECK(b[0] == 2 && b[1] == 1 && b[4] == 0 && b[6] == 2 && b[7] == 0x58, "version 2, MAP, lifetime 600");
    CHECK(b[18] == 0xFF && b[19] == 0xFF && b[20] == 192 && b[23] == 23, "the client as ::ffff:192.168.1.23");
    CHECK(!memcmp(b + 24, nonce, 12) && b[36] == 17 && b[40] == 0x5D && b[41] == 0xF1, "nonce, UDP, the port");
    CHECK(b[54] == 0xFF && b[55] == 0xFF && !b[56] && !b[59], "any IPv4 address outside: ::ffff:0.0.0.0");
    CHECK(net_pcp_map_req(b, 59, &me, nonce, 1, 1, 1) == -1, "no room");
    /* its answer, as RFC 6887 7.2 and 11.1 lay it out */
    uint8_t ans[60];
    memcpy(ans, b, 60);
    ans[1] = 0x81, ans[2] = 0, ans[3] = 0;
    ans[4] = 0, ans[5] = 0, ans[6] = 0x01, ans[7] = 0x2C;   /* 300 s */
    ans[8] = 0, ans[9] = 0, ans[10] = 0x10, ans[11] = 0;    /* epoch */
    memset(ans + 12, 0, 12);
    ans[42] = 0x5D, ans[43] = 0xF2;
    memset(ans + 44, 0, 16);
    ans[54] = ans[55] = 0xFF, ans[56] = 203, ans[57] = 0, ans[58] = 113, ans[59] = 5;
    CHECK(net_pcp_parse(ans, 60, nonce, &a) == 0 && a.version == 2 && a.result == 0 && a.lifetime == 300 &&
              a.external_port == 24050 && a.external.family == NET_AF_INET && a.external.addr[0] == 203 &&
              a.epoch == 4096, "a success: the lifetime, the port and the address outside");
    uint8_t other[12];
    memcpy(other, nonce, 12);
    other[0] ^= 1;
    CHECK(net_pcp_parse(ans, 60, other, &a) == -1, "another request's nonce: not this one's answer");
    CHECK(net_pcp_parse(ans, 59, nonce, &a) == -1 && net_pcp_parse(ans, 58, nonce, &a) == -1, "cut short");
    ans[1] = 0x01;
    CHECK(net_pcp_parse(ans, 60, nonce, &a) == -1, "a request, not an answer");
    ans[1] = 0x81, ans[3] = 2;
    CHECK(net_pcp_parse(ans, 60, nonce, &a) == 0 && a.result == 2 && !strcmp(net_pcp_result_name(2),
                                                                             "NOT_AUTHORIZED"), "refused");
    memcpy(b, ans, 60);
    memset(b + 60, 0, 1100 - 60);
    CHECK(net_pcp_parse(b, 64, nonce, &a) == 0, "options after it: skipped");
    CHECK(net_pcp_parse(b, 62, nonce, &a) == -1 && net_pcp_parse(b, 1104, nonce, &a) == -1,
          "a length that isn't a multiple of 4, or past 1100");
    static const uint8_t pmp_unsupp[8] = {0, 0x81, 0, 1, 0, 0, 0, 7};
    CHECK(net_pcp_parse(pmp_unsupp, 8, nonce, &a) == 0 && a.version == 0 && a.result == 1,
          "a NAT-PMP server's \"unsupported version\": speak NAT-PMP (RFC 6887 9)");
    CHECK(net_pcp_parse(pmp_unsupp, 3, nonce, &a) == -1, "3 bytes");
}

static void test_ssdp(void)
{
    char buf[600], loc[256], st[128];
    int n = net_ssdp_search(buf, sizeof buf, "239.255.255.250:1900",
                            "urn:schemas-upnp-org:device:InternetGatewayDevice:1", 2);
    static const char want[] = "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\n"
                               "MX: 2\r\nST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n";
    CHECK(n > 0 && !strncmp(buf, want, strlen(want)) && !strcmp(buf + n - 4, "\r\n\r\n"),
          "the M-SEARCH (UDA 1.1 1.3.2): %s", buf);
    CHECK(net_ssdp_search(buf, 40, "239.255.255.250:1900", "x", 2) == -1, "no room");
    static const char ok[] = "HTTP/1.1 200 OK\r\nCACHE-CONTROL: max-age=120\r\nst: urn:schemas-upnp-org:device:"
                             "InternetGatewayDevice:1\r\nEXT:\r\nlocation:   http://192.168.1.1:5000/rootDesc.xml \r\n"
                             "SERVER: x\r\n\r\n";
    CHECK(net_ssdp_parse(ok, (int)strlen(ok), loc, sizeof loc, st, sizeof st) == 0 &&
              !strcmp(loc, "http://192.168.1.1:5000/rootDesc.xml") && strstr(st, "InternetGatewayDevice:1"),
          "an answer, headers in any case, spaces trimmed: '%s' '%s'", loc, st);
    static const char lf[] = "HTTP/1.1 200 OK\nLOCATION: http://10.0.0.1/d.xml\n\n";
    CHECK(net_ssdp_parse(lf, (int)strlen(lf), loc, sizeof loc, st, sizeof st) == 0 && !st[0], "LF line ends, no ST");
    static const char nf[] = "HTTP/1.1 404 Not Found\r\nLOCATION: http://10.0.0.1/d.xml\r\n\r\n";
    CHECK(net_ssdp_parse(nf, (int)strlen(nf), loc, sizeof loc, st, sizeof st) == -1, "not a 200");
    static const char none[] = "HTTP/1.1 200 OK\r\nST: x\r\n\r\n";
    CHECK(net_ssdp_parse(none, (int)strlen(none), loc, sizeof loc, st, sizeof st) == -1, "no LOCATION");
    static const char two[] = "HTTP/1.1 200 OK\r\nLOCATION: http://10.0.0.1/a\r\nLOCATION: http://10.0.0.2/b\r\n\r\n";
    CHECK(net_ssdp_parse(two, (int)strlen(two), loc, sizeof loc, st, sizeof st) == -1, "two LOCATIONs: not trusted");
    char big[3000];
    n = snprintf(big, sizeof big, "HTTP/1.1 200 OK\r\nLOCATION: http://10.0.0.1/%0*d\r\n\r\n", 2000, 0);
    CHECK(net_ssdp_parse(big, n, loc, sizeof loc, st, sizeof st) == -1, "a LOCATION longer than its room: refused, "
                                                                        "not cut");
    static const char ctl[] = "HTTP/1.1 200 OK\r\nLOCATION: http://10.0.0.1/\x01x\r\n\r\n";
    CHECK(net_ssdp_parse(ctl, (int)strlen(ctl), loc, sizeof loc, st, sizeof st) == -1, "a control character");
    static const char nul[] = "HTTP/1.1 200 OK\r\nLOCATION: http://10.0.0.1/a\0b\r\n\r\n";
    CHECK(net_ssdp_parse(nul, (int)sizeof nul - 1, loc, sizeof loc, st, sizeof st) == -1, "a NUL inside");
    CHECK(net_ssdp_parse("HTTP/1.1", 8, loc, sizeof loc, st, sizeof st) == -1 &&
              net_ssdp_parse("", 0, loc, sizeof loc, st, sizeof st) == -1, "nothing much");
}

static void test_url(void)
{
    NetUrl u, v;
    CHECK(net_url_parse("http://192.168.1.1:5000/rootDesc.xml", &u) == 0 && u.addr.family == NET_AF_INET &&
              u.addr.addr[0] == 192 && u.addr.addr[3] == 1 && u.addr.port == 5000 && !strcmp(u.path, "/rootDesc.xml"),
          "a description's URL");
    CHECK(net_url_parse("HTTP://10.0.0.138", &u) == 0 && u.addr.port == 80 && !strcmp(u.path, "/"),
          "no port, no path");
    CHECK(net_url_parse("http://10.0.0.1:80/ctl?a=b", &u) == 0 && !strcmp(u.path, "/ctl?a=b"), "a query");
    static const char *const bad[] = {"https://10.0.0.1/", "http://router.local/", "http://user@10.0.0.1/",
                                      "http://[fe80::1]/", "http://10.0.0.1:0/", "http://10.0.0.1:70000/",
                                      "http://10.0.0.256/", "http://10.0.0/", "http://10.0.0.1.5/",
                                      "http://10.0.0.1?x", "http://10.0.0.1/a b", "http://10.0.0.1:/",
                                      "http://0010.0.0.1/", "ftp://10.0.0.1/", "http://10.0.0.1/a\\b", ""};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++)
        CHECK(net_url_parse(bad[i], &u) == -1, "not one: '%s'", bad[i]);
    char longu[700];
    snprintf(longu, sizeof longu, "http://10.0.0.1/%0600d", 0);
    CHECK(net_url_parse(longu, &u) == -1, "too long");
    net_url_parse("http://192.168.1.1:5000/dev/rootDesc.xml", &u);
    CHECK(net_url_resolve(&u, "/ctl/IPConn", &v) == 0 && v.addr.port == 5000 && !strcmp(v.path, "/ctl/IPConn"),
          "an absolute path");
    CHECK(net_url_resolve(&u, "ctl/IPConn", &v) == 0 && !strcmp(v.path, "/dev/ctl/IPConn"), "a relative one");
    CHECK(net_url_resolve(&u, "http://192.168.1.1:49000/x", &v) == 0 && v.addr.port == 49000, "a whole URL");
    static const char *const refs[] = {"//evil.example/x", "javascript:alert(1)", "https://192.168.1.1/x", "",
                                       "http://example.com/x", "a b"};
    for (size_t i = 0; i < sizeof refs / sizeof *refs; i++)
        CHECK(net_url_resolve(&u, refs[i], &v) == -1, "not followed: '%s'", refs[i]);
}

static void test_xml(void)
{
    char out[64];
    size_t from = 0, s, e;
    static const char doc[] = "<a><u:Name x=\"1>2\">  A &amp; B &lt;&#65;&#x42;&gt; </u:Name><Empty/>"
                              "<c><![CDATA[x<y]]></c><bad>&xxe;</bad><n><Name>inner</Name></n></a>";
    size_t len = strlen(doc);
    CHECK(net_xml_text(doc, len, "Name", &from, out, sizeof out) == 0 && !strcmp(out, "A & B <AB>"),
          "a prefix ignored, a '>' in an attribute, entities decoded, spaces trimmed: '%s'", out);
    from = 0;
    CHECK(net_xml_text(doc, len, "Empty", &from, out, sizeof out) == 0 && !out[0], "<Empty/>");
    from = 0;
    CHECK(net_xml_text(doc, len, "c", &from, out, sizeof out) == 0 && !strcmp(out, "x<y"), "CDATA as it is");
    from = 0;
    CHECK(net_xml_text(doc, len, "bad", &from, out, sizeof out) == -1, "an entity a DTD would define: refused");
    from = 0;
    CHECK(net_xml_text(doc, len, "n", &from, out, sizeof out) == -1, "an element inside: not a text");
    from = 0;
    CHECK(net_xml_text(doc, len, "Name", &from, out, sizeof out) == 0 &&
              net_xml_text(doc, len, "Name", &from, out, sizeof out) == 0 && !strcmp(out, "inner"), "the next one");
    from = 0;
    CHECK(net_xml_text(doc, len, "Name", &from, out, 8) == -1, "longer than its room: refused, not cut");
    from = 0;
    CHECK(net_xml_text(doc, len, "Missing", &from, out, sizeof out) == -1, "a missing tag");
    static const char nest[] = "<d><d>x</d><d>y</d></d>";
    from = 0;
    CHECK(net_xml_span(nest, strlen(nest), "d", &from, &s, &e) == 0 && s == 3 && e == strlen(nest) - 4,
          "nested elements of one name: the outer one's span");
    static const char *const broken[] = {"<a>text", "<a>x</b>", "<a", "<a><b>x</a>", "<1a>x</1a>", "<a>x</a",
                                         "<!-- open", "<a b=\">x</a>", "<a>\x80</a>", "<a>&#0;</a>", "<a>&#x;</a>",
                                         "<a>&#99999999999;</a>", "<a>&amp</a>"};
    for (size_t i = 0; i < sizeof broken / sizeof *broken; i++) {
        from = 0;
        CHECK(net_xml_text(broken[i], strlen(broken[i]), "a", &from, out, sizeof out) == -1, "malformed: '%s'",
              broken[i]);
    }
    static char lots[30000];
    memset(lots, '<', sizeof lots - 1);
    from = 0;
    CHECK(net_xml_text(lots, strlen(lots), "a", &from, out, sizeof out) == -1, "30,000 '<'");
    char deep[40000];
    size_t o = 0;
    for (int i = 0; i < 6000; i++)
        o += (size_t)snprintf(deep + o, sizeof deep - o, "<x>");
    from = 0;
    CHECK(net_xml_span(deep, o, "x", &from, &s, &e) == -1, "6,000 elements opened, none closed");
}

static void test_igd(void)
{
    NetUrl loc;
    NetIgdService svc[4];
    char name[64], doc[4096];
    net_url_parse("http://192.168.1.1:5000/rootDesc.xml", &loc);
    int n = net_igd_parse(DESC, strlen(DESC), &loc, svc, 4, name, sizeof name);
    CHECK(n == 2 && !strcmp(svc[0].type, "urn:schemas-upnp-org:service:WANIPConnection:1") &&
              !strcmp(svc[0].control.path, "/ctl/IPConn") && svc[0].control.addr.port == 5000 &&
              !strcmp(svc[1].type, "urn:schemas-upnp-org:service:WANPPPConnection:1"),
          "the WAN services, IP's before PPP's, Layer3Forwarding not one: %d '%s'", n, n ? svc[0].control.path : "");
    CHECK(!strcmp(name, "Fake & Test Router"), "its friendlyName: '%s'", name);
    /* every URL on the device that answered */
    snprintf(doc, sizeof doc, "<root><URLBase>http://10.9.9.9:80/</URLBase><service><serviceType>urn:schemas-upnp-org:"
             "service:WANIPConnection:2</serviceType><controlURL>ctl</controlURL></service><service><serviceType>urn:"
             "schemas-upnp-org:service:WANIPConnection:1</serviceType><controlURL>http://10.9.9.9/c</controlURL>"
             "</service></root>");
    n = net_igd_parse(doc, strlen(doc), &loc, svc, 4, name, sizeof name);
    CHECK(n == 1 && !strcmp(svc[0].type, "urn:schemas-upnp-org:service:WANIPConnection:2") &&
              svc[0].control.addr.addr[0] == 192 && !strcmp(svc[0].control.path, "/ctl"),
          "a URLBase and a control URL on another address: not followed (%d)", n);
    char huge[8000];
    snprintf(huge, sizeof huge, "<service><serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType>"
             "<controlURL>/%05000d</controlURL></service>", 0);
    CHECK(net_igd_parse(huge, strlen(huge), &loc, svc, 4, name, sizeof name) == 0, "an oversized control URL");
    CHECK(net_igd_parse("<root/>", 7, &loc, svc, 4, name, sizeof name) == 0 && !name[0], "no service");
    CHECK(net_igd_parse("\x01\x02garbage<<<>>>", 15, &loc, svc, 4, name, sizeof name) == 0, "garbage");
    static const char noctl[] = "<service><serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType>"
                                "</service>";
    CHECK(net_igd_parse(noctl, strlen(noctl), &loc, svc, 4, name, sizeof name) == 0, "no controlURL");
    static const char utf[] = "<friendlyName>Fritz\xC2\xA0" "Box</friendlyName>";
    net_igd_parse(utf, strlen(utf), &loc, svc, 4, name, sizeof name);
    CHECK(!strcmp(name, "Fritz??Box"), "a name with UTF-8 in it, shown as ASCII: '%s'", name);
}

static void test_soap(void)
{
    char buf[1200], desc[64];
    const char *names[] = {"NewExternalPort", "NewPortMappingDescription"}, *values[] = {"24049", "R&D <test>"};
    int n = net_soap_body(buf, sizeof buf, "urn:schemas-upnp-org:service:WANIPConnection:1", "AddPortMapping", names,
                          values, 2);
    CHECK(n > 0 && strstr(buf, "<u:AddPortMapping xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\">"
                               "<NewExternalPort>24049</NewExternalPort><NewPortMappingDescription>R&amp;D &lt;test&gt;"
                               "</NewPortMappingDescription></u:AddPortMapping></s:Body></s:Envelope>"),
          "the body, values escaped: %s", buf);
    CHECK(net_soap_body(buf, 100, "urn:x", "A", names, values, 2) == -1, "no room");
    CHECK(net_soap_body(buf, sizeof buf, "urn:x\"><evil", "A", NULL, NULL, 0) == -1, "a type that isn't a token");
    int code = 0;
    CHECK(net_soap_fault(FAULT, strlen(FAULT), &code, desc, sizeof desc) == 0 && code == 718 &&
              !strcmp(desc, "ConflictInMappingEntry"), "a fault's UPnPError: %d '%s'", code, desc);
    CHECK(net_soap_fault(EXT_OK, strlen(EXT_OK), &code, desc, sizeof desc) == -1, "not a fault");
    static const char nan[] = "<UPnPError><errorCode>7a</errorCode></UPnPError>";
    CHECK(net_soap_fault(nan, strlen(nan), &code, desc, sizeof desc) == -1, "a code that isn't a number");
    static const char big[] = "<UPnPError><errorCode>1234567</errorCode></UPnPError>";
    CHECK(net_soap_fault(big, strlen(big), &code, desc, sizeof desc) == -1, "a code too long");
    size_t from = 0;
    char ip[64];
    CHECK(net_xml_text(EXT_OK, strlen(EXT_OK), "NewExternalIPAddress", &from, ip, sizeof ip) == 0 &&
              !strcmp(ip, "203.0.113.5"), "GetExternalIPAddress's answer");
}

static int parse(const char *text, int *status, char *body, size_t body_n)
{
    char buf[4096], *b;
    size_t n = strlen(text), bl;
    memcpy(buf, text, n + 1);
    if (net_http_parse(buf, n, status, &b, &bl))
        return -1;
    snprintf(body, body_n, "%.*s", (int)bl, b);
    return (int)bl;
}

static void test_http(void)
{
    char body[512];
    int st;
    CHECK(parse("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello and more", &st, body, sizeof body) == 5 &&
              st == 200 && !strcmp(body, "hello"), "Content-Length");
    CHECK(parse("HTTP/1.0 500 Internal Server Error\r\nServer: x\r\n\r\n<fault/>", &st, body, sizeof body) == 8 &&
              st == 500, "to the end, HTTP/1.0");
    CHECK(parse("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5;ext=1\r\nhello\r\n6\r\n world\r\n0\r\n\r\n",
                &st, body, sizeof body) == 11 && !strcmp(body, "hello world"), "chunked, an extension: '%s'", body);
    CHECK(parse("HTTP/1.1 200 OK\nContent-Length: 2\n\nok", &st, body, sizeof body) == 2, "LF line ends");
    static const char *const bad[] = {
        "HTTP/1.1 200 OK\r\nContent-Length: 50\r\n\r\nshort",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nFFFFFFFFFF\r\nx\r\n0\r\n\r\n",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n10\r\nhello\r\n0\r\n\r\n",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\n",
        "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Length: 3\r\n\r\nabc",
        "HTTP/1.1 200 OK\r\nContent-Length: -1\r\n\r\nabc",
        "HTTP/1.1 200 OK\r\nContent-Length: 99999999999\r\n\r\nabc",
        "HTTP/1.1 200 OK\r\nno end of the headers",
        "HTTP/2 200\r\n\r\n",
        "SIP/2.0 200 OK\r\n\r\n",
        "HTTP/1.1 2x0 OK\r\n\r\n",
        ""};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++)
        CHECK(parse(bad[i], &st, body, sizeof body) == -1, "refused: '%.40s'", bad[i]);
    static const char done[] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    CHECK(net_http_complete(done, strlen(done)) == 1 && net_http_complete(done, strlen(done) - 1) == 0,
          "whole by Content-Length");
    static const char ch[] = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nok\r\n0\r\n\r\n";
    CHECK(net_http_complete(ch, strlen(ch)) == 1 && net_http_complete(ch, strlen(ch) - 5) == 0,
          "whole by its last chunk");
    static const char to_end[] = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nok";
    CHECK(net_http_complete(to_end, strlen(to_end)) == 0, "no length: to the connection's end");
}

static void test_hint(void)
{
    NetAddr a;
    char out[400];
    memset(&a, 0, sizeof a);
    a.family = NET_AF_INET;
    a.addr[0] = 203, a.addr[1] = 0, a.addr[2] = 113, a.addr[3] = 5;
    net_nat_join_hint(&a, 24049, out, sizeof out);
    CHECK(strstr(out, "router must forward UDP port 24049") && strstr(out, "Open the port on my router (UPnP)") &&
              strstr(out, "Tailscale"), "an internet address: forwarding, UPnP, Tailscale: '%s'", out);
    a.addr[0] = 100, a.addr[1] = 101;
    net_nat_join_hint(&a, 24049, out, sizeof out);
    CHECK(strstr(out, "Tailscale address"), "100.x: Tailscale's: '%s'", out);
    a.addr[0] = 192, a.addr[1] = 168;
    net_nat_join_hint(&a, 24049, out, sizeof out);
    CHECK(!out[0], "the local network: nothing more");
}

static void test_config(void)
{
    NetNatConfig c;
    net_nat_config_defaults(&c);
    CHECK(c.lease_s == NET_NAT_LEASE_S && !strcmp(c.description, "Rush2049PC"), "defaults");
}

static int cmd_unit(void)
{
    test_pmp();
    test_pcp();
    test_ssdp();
    test_url();
    test_xml();
    test_igd();
    test_soap();
    test_http();
    test_hint();
    test_config();
    printf("%s: %d checks, %d failure(s)\n", g_fail ? "FAILED" : "nat unit: all passed", g_checks, g_fail);
    return g_fail ? 1 : 0;
}

/* ---------------------------------------------------------------- fuzz */

static uint64_t g_rng = 0x2049;

static uint64_t rnd(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return g_rng;
}

static int cmd_fuzz(long iters)
{
    static const char *const seeds[] = {
        DESC, EXT_OK, FAULT,
        "HTTP/1.1 200 OK\r\nST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\nLOCATION: "
        "http://192.168.1.1:5000/rootDesc.xml\r\n\r\n",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5;e\r\nhello\r\n6\r\n world\r\n0\r\n\r\n",
        "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 20\r\n\r\n<UPnPError></UPnPError>",
        "http://192.168.1.1:5000/a/b.xml",
    };
    static const char *const names[] = {"service", "controlURL", "friendlyName", "NewExternalIPAddress", "UPnPError",
                                        "errorCode", "URLBase", "serviceType"};
    uint8_t nonce[12] = {0};
    char *buf = malloc(70000), out[300], st[200];
    long well = 0;
    if (!buf)
        return 1;
    for (long i = 0; i < iters; i++) {
        size_t len;
        int kind = (int)(rnd() % 3), which = (int)(rnd() % (sizeof seeds / sizeof *seeds));
        if (kind == 0) {   /* random bytes, printable often (the parsers look for text) */
            len = (size_t)(rnd() % 3000);
            for (size_t k = 0; k < len; k++)
                buf[k] = (char)(rnd() % 3 ? "<>/&;:= \r\nabcdefHTTP0123456789#x\"'![]-"[rnd() % 38] : (int)rnd());
        } else {   /* a seed, mutated: bytes flipped, inserted, cut, repeated */
            len = strlen(seeds[which]);
            memcpy(buf, seeds[which], len);
            int muts = 1 + (int)(rnd() % 8);
            for (int m = 0; m < muts && len; m++) {
                size_t at = (size_t)(rnd() % len);
                switch (rnd() % 4) {
                case 0:
                    buf[at] ^= (char)(1u << (rnd() % 8));
                    break;
                case 1:
                    buf[at] = "<>/&;:\r\n\"'"[rnd() % 10];
                    break;
                case 2:
                    len = at;   /* cut */
                    break;
                default:   /* a stretch repeated (oversized fields) */
                    if (len < 30000) {
                        size_t k = 1 + (size_t)(rnd() % 64), times = (size_t)(rnd() % 200);
                        if (at + k > len)
                            k = len - at;
                        for (size_t t = 0; t < times && len + k < 60000; t++) {
                            memmove(buf + at + k, buf + at, len - at);
                            len += k;
                        }
                    }
                }
            }
        }
        /* each parser on its own copy: exactly len bytes, so nothing past them is there to read (ASan) */
        char *c = malloc(len + 1);
        if (!c)
            break;
        memcpy(c, buf, len);
        c[len] = 0;
        size_t from = 0, s, e;
        NetUrl loc, u;
        NetIgdService svc[4];
        NetPcpAnswer pa;
        int status, r, ip, ep, code;
        uint32_t life;
        uint8_t ext[4];
        char *body;
        size_t bl;
        net_url_parse("http://192.168.1.1:5000/rootDesc.xml", &loc);
        well += net_ssdp_parse(c, (int)len, out, sizeof out, st, sizeof st) == 0;
        well += net_xml_text(c, len, names[rnd() % 8], &from, out, sizeof out) == 0;
        from = 0;
        well += net_xml_span(c, len, names[rnd() % 8], &from, &s, &e) == 0;
        well += net_igd_parse(c, len, &loc, svc, 4, out, sizeof out) > 0;
        well += net_soap_fault(c, len, &code, out, sizeof out) == 0;
        well += net_http_complete(c, len);
        if (len < sizeof out) {
            well += net_url_parse(c, &u) == 0;
            well += net_url_resolve(&loc, c, &u) == 0;
        }
        well += net_pcp_parse((const uint8_t *)c, (int)len, nonce, &pa) == 0;
        well += net_pmp_parse_map((const uint8_t *)c, (int)len, &r, &ip, &ep, &life) == 0;
        well += net_pmp_parse_external((const uint8_t *)c, (int)len, &r, ext) == 0;
        well += net_http_parse(c, len, &status, &body, &bl) == 0;   /* last: it writes in place */
        free(c);
    }
    free(buf);
    printf("nat fuzz: %ld inputs through every parser, %ld parses well-formed: ok\n", iters, well);
    return 0;
}

/* ---------------------------------------------------------------- run: against the fake router */

static const char *state_name(int s)
{
    static const char *const n[] = {"off", "working", "mapped", "failed"};
    return s >= 0 && s < 4 ? n[s] : "?";
}

static const char *why_name(int w)
{
    static const char *const n[] = {"ok", "no-router", "refused", "conflict", "no-internet", "cgnat", "double", "lost"};
    return w >= 0 && w < 8 ? n[w] : "?";
}

static void log_line(void *user, const char *line)
{
    (void)user;
    printf("nat: %s\n", line);
    fflush(stdout);
}

static int cmd_run(int argc, char **argv)
{
    double seconds = 5;
    int keep = 0, crash = 0, port = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--seconds") && i + 1 < argc)
            seconds = atof(argv[++i]);
        else if (!strcmp(argv[i], "--keep"))
            keep = 1;
        else if (!strcmp(argv[i], "--crash"))
            crash = 1;
        else if (!strcmp(argv[i], "--port") && i + 1 < argc)
            port = atoi(argv[++i]);
    }
    NetNatConfig c;
    net_nat_config_defaults(&c);
    if (!c.test) {
        fprintf(stderr, "nat_test run: only against a fake router: set RUSH_NAT_SSDP and/or RUSH_NAT_GATEWAY "
                        "(tools/port/fake_router.py); the real one is never asked\n");
        return 2;
    }
    NetSock s;
    char err[200];
    net_sock_startup();
    if (net_sock_open(&s, "127.0.0.1", port, err, sizeof err)) {   /* the session's port, as a host has it */
        fprintf(stderr, "nat_test run: %s\n", err);
        return 2;
    }
    c.internal_port = net_sock_port(&s);
    c.log = log_line;
    printf("port %d\n", c.internal_port);
    fflush(stdout);
    NetNat *n = net_nat_start(&c);
    if (!n)
        return 1;
    uint64_t t0 = net_os_now_ns();
    NetNatStatus last;
    memset(&last, 0xFF, sizeof last);
    for (;;) {
        NetNatStatus st;
        net_nat_status(n, &st);
        if (st.state != last.state || st.why != last.why || st.renewals != last.renewals ||
            st.external.port != last.external.port) {
            char a[64] = "none";
            if (st.external.family)
                net_addr_str(&st.external, a, sizeof a);
            printf("state %s why=%s proto=%s external=%s lease=%u renewals=%u failures=%u detail='%s' name='%s' "
                   "t=%.2f\n", state_name(st.state), why_name(st.why), net_nat_proto_name(st.proto), a, st.lease_s,
                   st.renewals, st.renew_failures, st.detail, st.name, (net_os_now_ns() - t0) / 1e9);
            fflush(stdout);
            last = st;
        }
        if ((net_os_now_ns() - t0) / 1e9 >= seconds)
            break;
        net_os_sleep_ns(20000000);
    }
    if (crash) {   /* as a crash: nothing removed; the router's lease ends it */
        printf("crash\n");
        fflush(stdout);
        _exit(9);
    }
    uint64_t t1 = net_os_now_ns();
    int removed = net_nat_stop(n, keep, 3000);
    printf("stopped: %s=%d in %.2f s\n", keep ? "kept" : "removed", keep ? 1 : removed, (net_os_now_ns() - t1) / 1e9);
    net_sock_close(&s);
    net_sock_shutdown();
    return 0;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc >= 2 && !strcmp(argv[1], "unit"))
        return cmd_unit();
    if (argc >= 2 && !strcmp(argv[1], "fuzz"))
        return cmd_fuzz(argc >= 3 ? atol(argv[2]) : 100000);
    if (argc >= 2 && !strcmp(argv[1], "run"))
        return cmd_run(argc, argv);
    fprintf(stderr, "usage: nat_test unit | fuzz [N] | run [--seconds S] [--keep] [--crash] [--port P]\n");
    return 2;
}
