/* port/net/tests/net_test.c: port/net's tests (docs/port/NETWORK.md 8).
 *
 *   net_test unit              BLAKE2s, the wire format, addresses, the simulator, sockets on loopback, and on a
 *                              virtual clock: the handshake with files and loss, join codes, builds, BYE, the
 *                              timeout, a desync, spoofed and malformed packets, the input assignment rules
 *   net_test sim MODEL [TICKS] two peers on a virtual clock through a RUSH_SIM_NET model (10,000 ticks by default):
 *                              the same hash at every tick on both, the delay's trace, stalls, 60.00 ticks a second
 *   net_test fuzz [N]          the parser on random and mutated packets, and sessions fed garbage
 *   net_test threads MODEL [TICKS]  the same over real UDP on 127.0.0.1, two threads, in real time
 *   net_test models            the models' names and specs
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver.h"
#include "net.h"
#include "net_blake2s.h"
#include "net_os.h"
#include "net_proto.h"
#include "net_sim.h"
#include "net_sock.h"
#include "toy.h"
#include "vnet.h"

#ifdef _WIN32
#include <mmsystem.h>
#endif

#define MS 1000000ull
#define PERIOD (1000000000 / 60)

static int g_fail, g_verbose;

#define CHECK(c, ...)                                                                                                  \
    do {                                                                                                              \
        if (!(c)) {                                                                                                   \
            g_fail++;                                                                                                 \
            printf("FAIL %s:%d: %s: ", __FILE__, __LINE__, #c);                                                       \
            printf(__VA_ARGS__);                                                                                      \
            printf("\n");                                                                                             \
            fflush(stdout);                                                                                           \
        }                                                                                                             \
    } while (0)

/* ---------------------------------------------------------------- the models */

typedef struct Model {
    const char *name, *host, *guest;   /* each side's RUSH_SIM_NET (its receive path) */
    const char *what;
    double max_stall_ms;               /* the longest stall allowed after the first seconds */
    int max_stalls;                    /* stalls allowed over 10,000 ticks */
    int rate_check;                    /* 60.00 a second in the steady part */
} Model;

/* Virtual runs are 10,000 ticks (167 s): the changes come at 60 s and 110 s, the pause at 60 s. */
static const Model MODELS[] = {
    {"clean", "", "", "no delay", 0, 0, 1},
    {"lan20", "delay=20", "delay=20", "20 ms each way", 0, 2, 1},
    {"wan80", "delay=80,jitter=20", "delay=80,jitter=20", "80 ms ± 20 each way", 0, 4, 1},
    {"bad150", "delay=150,jitter=40,loss=10,dup=2,reorder=2", "delay=150,jitter=40,loss=10,dup=2,reorder=2",
     "150 ms ± 40, 10% loss, 2% duplicated, 2% reordered", 0, 60, 0},
    {"shift", "delay=10,at=60,delay=200,at=110,delay=30", "delay=10,at=60,delay=200,at=110,delay=30",
     "10 ms, 200 ms from 60 s, 30 ms from 110 s", 0, 6, 0},
    {"pause", "delay=20", "delay=20,pause=60:3", "20 ms; the guest stops for 3 s at 60 s", 0, 4, 0},
    {"burst", "delay=30,jitter=5,burst=5:1:30", "delay=30,jitter=5,burst=5:1:30",
     "30 ms ± 5, and 1 s of 30% loss every 5 s", 0, 60, 0},
};
#define NMODELS (int)(sizeof MODELS / sizeof MODELS[0])

static const Model *find_model(const char *name)
{
    for (int i = 0; i < NMODELS; i++)
        if (!strcmp(MODELS[i].name, name))
            return &MODELS[i];
    return NULL;
}

/* ---------------------------------------------------------------- logs */

typedef struct LogBuf {
    char text[1 << 16];
    int len;
    const char *tag;
} LogBuf;

static void log_to(void *user, const char *line)
{
    LogBuf *b = user;
    if (g_verbose)
        printf("  [%s] %s\n", b->tag, line);
    int n = (int)strlen(line);
    if (b->len + n + 2 < (int)sizeof b->text) {
        memcpy(b->text + b->len, line, (size_t)n);
        b->len += n;
        b->text[b->len++] = '\n';
        b->text[b->len] = 0;
    }
}

/* With the virtual clock the log callback's user is the clock's; the log goes through a wrapper. */
typedef struct VUser {
    VNet *v;
    LogBuf log;
} VUser;

static uint64_t vuser_clock(void *user) { return ((VUser *)user)->v->now; }
static void vuser_log(void *user, const char *line) { log_to(&((VUser *)user)->log, line); }

/* ---------------------------------------------------------------- BLAKE2s */

static void hex(const uint8_t *b, int n, char *out)
{
    for (int i = 0; i < n; i++)
        sprintf(out + 2 * i, "%02x", b[i]);
}

static void test_blake2s(void)
{
    uint8_t out[32], key[32], buf[256];
    char h[65];
    net_blake2s(out, 32, NULL, 0, "abc", 3);   /* RFC 7693, appendix B */
    hex(out, 32, h);
    CHECK(!strcmp(h, "508c5e8c327c14e2e1a72ba34eeb452f37458b209ed63a294d999b4c86675982"), "BLAKE2s-256(abc) = %s", h);
    for (int i = 0; i < 32; i++)
        key[i] = (uint8_t)i;
    net_blake2s(out, 32, key, 32, NULL, 0);   /* the reference KAT: key 00..1f, the empty message */
    hex(out, 32, h);
    CHECK(!strcmp(h, "48a8997da407876b3d79c0d92325ad3b89cbb754d86ab71aee047ad345fd2c49"), "keyed empty = %s", h);
    for (int i = 0; i < 256; i++)
        buf[i] = (uint8_t)i;
    net_blake2s(out, 32, key, 32, buf, 255);   /* the KAT's last entry: 255 bytes 00..fe */
    hex(out, 32, h);
    CHECK(!strcmp(h, "3fb735061abc519dfe979e54c1ee5bfad0a9d858b3315bad34bde999efd724dd"), "keyed 255 = %s", h);
    /* the same in pieces of every size */
    uint8_t whole[32];
    net_blake2s(whole, 32, NULL, 0, buf, 200);
    for (int step = 1; step < 70; step += 3) {
        NetBlake2s c;
        net_blake2s_init(&c, 32, NULL, 0);
        for (int at = 0; at < 200; at += step)
            net_blake2s_update(&c, buf + at, (size_t)(200 - at < step ? 200 - at : step));
        net_blake2s_final(&c, out);
        CHECK(!memcmp(out, whole, 32), "pieces of %d differ", step);
    }
    /* a printed vector for a check against another implementation (Python's hashlib.blake2s) */
    net_blake2s(out, 16, "rush", 4, "2049", 4);
    hex(out, 16, h);
    printf("blake2s: RFC 7693 vector ok; BLAKE2s-128(key=rush, 2049) = %s\n", h);
}

/* ---------------------------------------------------------------- the wire format */

static int build_packet(uint8_t *buf, int type, uint64_t session, const void *payload, int n, const uint8_t *key)
{
    NetWriter w;
    NetHeader h;
    memset(&h, 0, sizeof h);
    h.version = NET_PROTO_VERSION;
    h.type = (uint8_t)type;
    h.flags = key ? NET_FLAG_TAG : 0;
    h.sender = 1;
    h.session = session;
    h.seq = 7;
    h.t_send = 123456789;
    net_w_init(&w, buf, NET_MAX_PACKET - (key ? NET_TAG_SIZE : 0));
    net_w_header(&w, &h);
    net_w_bytes(&w, payload, n);
    if (w.over)
        return -1;
    if (key) {
        net_proto_tag(key, buf, w.len, buf + w.len);
        return w.len + NET_TAG_SIZE;
    }
    return w.len;
}

/* An INPUT payload: count inputs for one port from first, hashes. */
static int input_payload(uint8_t *out, uint32_t cur, uint32_t first, int count, uint8_t ports, uint32_t hfirst,
                         int hcount, int whash)
{
    NetWriter w;
    net_w_init(&w, out, NET_MAX_PACKET);
    net_w_u32(&w, cur);
    net_w_u64(&w, 5000);
    net_w_u32(&w, 0);
    net_w_u32(&w, 0);
    net_w_u32(&w, 0);
    net_w_u32(&w, first);
    net_w_u8(&w, (uint8_t)count);
    net_w_u8(&w, ports);
    net_w_u8(&w, 2);
    net_w_u8(&w, (uint8_t)(whash ? NET_INPUT_WHASH : 0));
    net_w_u32(&w, 0);
    net_w_u64(&w, 0);
    net_w_u32(&w, hfirst);
    net_w_u16(&w, (uint16_t)hcount);
    net_w_u16(&w, 0);
    int np = net_popcount4(ports);
    for (int i = 0; i < count * np; i++)
        net_w_u32(&w, 0x00030001u + (uint32_t)i);
    for (int i = 0; i < count; i++)
        net_w_u16(&w, 10);
    for (int i = 0; i < hcount; i++)
        net_w_u32(&w, 0xABCD0000u + (uint32_t)i);
    if (whash) {
        net_w_u32(&w, 59);
        net_w_u64(&w, 0x1122334455667788ull);
    }
    return w.len;
}

static void test_proto(void)
{
    uint8_t buf[2048], pl[1400], key[32];
    NetMsg m;
    /* HELLO round trip */
    NetHello h;
    memset(&h, 0, sizeof h);
    h.nonce = 0x1111222233334444ull;
    h.ports = 2;
    h.nfiles = 2;
    h.settings.size = 1234;
    h.settings.hash[0] = 9;
    snprintf(h.build, sizeof h.build, "%s", "abc1234");
    h.files[0].size = 32768;
    h.files[1].size = 5;
    h.files[1].hash[31] = 7;
    NetWriter w;
    net_w_init(&w, pl, sizeof pl);
    net_w_hello(&w, &h);
    int n = build_packet(buf, NET_MSG_HELLO, 0, pl, w.len, NULL);
    CHECK(net_proto_parse(buf, n, &m) == 0, "HELLO parses");
    CHECK(m.u.hello.nonce == h.nonce && m.u.hello.ports == 2 && m.u.hello.nfiles == 2 &&
              m.u.hello.settings.size == 1234 && m.u.hello.settings.hash[0] == 9 &&
              !strcmp(m.u.hello.build, "abc1234") && m.u.hello.files[0].size == 32768 &&
              m.u.hello.files[1].hash[31] == 7,
          "HELLO fields");
    printf("proto: HELLO %d bytes\n", n);
    for (int cut = 0; cut < n; cut++)
        CHECK(net_proto_parse(buf, cut, &m) != 0, "HELLO cut to %d parses", cut);
    buf[n] = 0;
    CHECK(net_proto_parse(buf, n + 1, &m) == NET_PARSE_LONG, "HELLO + 1 byte parses");
    /* a build without its NUL, or with a control character */
    {
        uint8_t b2[2048];
        memcpy(b2, buf, (size_t)n);
        memset(b2 + NET_HEADER_SIZE + 56, 'x', NET_BUILD_LEN);
        CHECK(net_proto_parse(b2, n, &m) == NET_PARSE_FIELD, "an unterminated build parses");
        memcpy(b2, buf, (size_t)n);
        b2[NET_HEADER_SIZE + 56] = '\n';
        CHECK(net_proto_parse(b2, n, &m) == NET_PARSE_FIELD, "a build with a newline parses");
    }
    /* the header's checks */
    memcpy(pl, buf, (size_t)n);
    pl[0] = 'X';
    CHECK(net_proto_parse(pl, n, &m) == NET_PARSE_MAGIC, "bad magic");
    memcpy(pl, buf, (size_t)n);
    pl[4] = 2;
    CHECK(net_proto_parse(pl, n, &m) == NET_PARSE_VERSION, "bad version");
    memcpy(pl, buf, (size_t)n);
    pl[5] = 99;
    CHECK(net_proto_parse(pl, n, &m) == NET_PARSE_TYPE, "bad type");
    memcpy(pl, buf, (size_t)n);
    pl[6] = 0x80;
    CHECK(net_proto_parse(pl, n, &m) == NET_PARSE_FIELD, "bad flags");
    CHECK(net_proto_parse(buf, 1300, &m) == NET_PARSE_LONG, "over 1200 bytes");
    /* INPUT round trip, at its largest */
    int pn = input_payload(pl, 1000, 900, NET_WINDOW, 0x2, 990, 94, 1);   /* 1200 bytes exactly */
    n = build_packet(buf, NET_MSG_INPUT, 42, pl, pn, NULL);
    CHECK(n > 0 && n <= NET_MAX_PACKET, "INPUT size %d", n);
    CHECK(net_proto_parse(buf, n, &m) == 0, "INPUT parses");
    CHECK(m.u.input.cur == 1000 && m.u.input.first == 900 && m.u.input.count == NET_WINDOW &&
              m.u.input.hash_count == 94 && net_get32(m.u.input.hashes + 4) == 0xABCD0001u &&
              m.u.input.whash == 0x1122334455667788ull && m.u.input.whash_retrace == 59 && m.h.session == 42,
          "INPUT fields");
    for (int cut = NET_HEADER_SIZE; cut < n; cut++)
        CHECK(net_proto_parse(buf, cut, &m) != 0, "INPUT cut to %d parses", cut);
    pn = input_payload(pl, 1000, 900, NET_WINDOW + 1, 0x2, 0, 0, 0);
    n = build_packet(buf, NET_MSG_INPUT, 42, pl, pn, NULL);
    CHECK(n < 0 || net_proto_parse(buf, n, &m) != 0, "121 inputs parse");
    pn = input_payload(pl, 1000, 0xFFFFFFF0u, 100, 0x2, 0, 0, 0);
    n = build_packet(buf, NET_MSG_INPUT, 42, pl, pn, NULL);
    CHECK(net_proto_parse(buf, n, &m) == NET_PARSE_FIELD, "inputs past retrace 2^32 parse");
    /* the tag */
    net_proto_key("join-me", key);
    pn = input_payload(pl, 10, 5, 3, 0x1, 0, 0, 0);
    n = build_packet(buf, NET_MSG_INPUT, 42, pl, pn, key);
    CHECK(net_proto_parse(buf, n, &m) == 0 && m.has_tag, "tagged INPUT parses");
    CHECK(net_proto_tag_ok(key, buf, n), "its tag checks");
    buf[50] ^= 1;
    CHECK(!net_proto_tag_ok(key, buf, n), "a changed byte fails the tag");
    buf[50] ^= 1;
    uint8_t key2[32];
    net_proto_key("join-me!", key2);
    CHECK(!net_proto_tag_ok(key2, buf, n), "another code's key fails the tag");
    /* BYE, chunk, ack */
    NetBye b;
    memset(&b, 0, sizeof b);
    b.reason = NET_END_DESYNC;
    b.detail = 777;
    snprintf(b.text, sizeof b.text, "%s", "out of sync");
    net_w_init(&w, pl, sizeof pl);
    net_w_bye(&w, &b);
    n = build_packet(buf, NET_MSG_BYE, 42, pl, w.len, NULL);
    CHECK(net_proto_parse(buf, n, &m) == 0 && m.u.bye.reason == NET_END_DESYNC && m.u.bye.detail == 777 &&
              !strcmp(m.u.bye.text, "out of sync"),
          "BYE");
    uint8_t data[NET_CHUNK];
    memset(data, 0x5A, sizeof data);
    NetChunk ch = {3, 31, 32768, NET_CHUNK, data};
    net_w_init(&w, pl, sizeof pl);
    net_w_chunk(&w, &ch);
    n = build_packet(buf, NET_MSG_FILE, 42, pl, w.len, key);
    CHECK(n <= NET_MAX_PACKET, "a tagged FILE chunk is %d bytes", n);
    CHECK(net_proto_parse(buf, n, &m) == 0 && m.u.chunk.index == 3 && m.u.chunk.chunk == 31 &&
              m.u.chunk.len == NET_CHUNK && m.u.chunk.data[100] == 0x5A,
          "FILE");
    printf("proto: INPUT with 120 inputs, 94 hashes and a window hash %d bytes; tagged FILE chunk %d bytes\n",
           NET_HEADER_SIZE + input_payload(pl, 1, 1, NET_WINDOW, 2, 0, 94, 1), n);
    pn = input_payload(pl, 1000, 900, NET_WINDOW, 0x2, 990, 95, 1);
    CHECK(build_packet(buf, NET_MSG_INPUT, 42, pl, pn, NULL) < 0, "a 1204-byte INPUT was built");
}

static void test_addresses(void)
{
    char host[128];
    int port;
    CHECK(net_split_hostport("rocky.tailfe03a6.ts.net", host, sizeof host, &port, 24049) == 0 &&
              !strcmp(host, "rocky.tailfe03a6.ts.net") && port == 24049,
          "a MagicDNS name");
    CHECK(net_split_hostport("100.101.102.103:4000", host, sizeof host, &port, 1) == 0 &&
              !strcmp(host, "100.101.102.103") && port == 4000,
          "an address and port");
    CHECK(net_split_hostport("[fd7a:115c:a1e0::1]:5000", host, sizeof host, &port, 1) == 0 &&
              !strcmp(host, "fd7a:115c:a1e0::1") && port == 5000,
          "[v6]:port");
    CHECK(net_split_hostport("::1", host, sizeof host, &port, 77) == 0 && !strcmp(host, "::1") && port == 77,
          "a bare v6");
    CHECK(net_split_hostport("host:0", host, sizeof host, &port, 1) != 0, "port 0");
    CHECK(net_split_hostport("host:70000", host, sizeof host, &port, 1) != 0, "port 70000");
    CHECK(net_split_hostport("host:12ab", host, sizeof host, &port, 1) != 0, "port 12ab");
    CHECK(net_split_hostport("[::1", host, sizeof host, &port, 1) != 0, "[::1");
    CHECK(net_split_hostport("", host, sizeof host, &port, 1) != 0, "empty");

    /* what an address is (the host's waiting screen), and as a player types it */
    static const struct { int family; uint8_t b[16]; int kind; const char *ip; } kinds[] = {
        {NET_AF_INET, {100, 101, 102, 103}, NET_KIND_TAILSCALE, "100.101.102.103"},
        {NET_AF_INET, {100, 64, 0, 1}, NET_KIND_TAILSCALE, "100.64.0.1"},
        {NET_AF_INET, {100, 128, 0, 1}, NET_KIND_PUBLIC, "100.128.0.1"},
        {NET_AF_INET, {192, 168, 1, 20}, NET_KIND_LAN, "192.168.1.20"},
        {NET_AF_INET, {172, 31, 0, 1}, NET_KIND_LAN, "172.31.0.1"},
        {NET_AF_INET, {172, 32, 0, 1}, NET_KIND_PUBLIC, "172.32.0.1"},
        {NET_AF_INET, {10, 0, 0, 2}, NET_KIND_LAN, "10.0.0.2"},
        {NET_AF_INET, {127, 0, 0, 1}, NET_KIND_LOOPBACK, "127.0.0.1"},
        {NET_AF_INET, {169, 254, 3, 4}, NET_KIND_LINK_LOCAL, "169.254.3.4"},
        {NET_AF_INET, {203, 0, 113, 5}, NET_KIND_PUBLIC, "203.0.113.5"},
        {NET_AF_INET6, {0xfd, 0x7a, 0x11, 0x5c, 0xa1, 0xe0, 0xab, 0x12, [14] = 0x12, [15] = 0x34}, NET_KIND_TAILSCALE,
         "fd7a:115c:a1e0:ab12::1234"},
        {NET_AF_INET6, {0xfe, 0x80, [15] = 1}, NET_KIND_LINK_LOCAL, "fe80::1"},
        {NET_AF_INET6, {[15] = 1}, NET_KIND_LOOPBACK, "::1"},
        {NET_AF_INET6, {0xfd, 0x00, [15] = 9}, NET_KIND_LAN, "fd00::9"},
        {NET_AF_INET6, {0x20, 0x01, 0x0d, 0xb8, [15] = 0x20}, NET_KIND_PUBLIC, "2001:db8::20"},
        {NET_AF_INET6, {0xff, 0x02, [15] = 1}, NET_KIND_OTHER, "ff02::1"},
    };
    for (size_t i = 0; i < sizeof kinds / sizeof kinds[0]; i++) {
        NetAddr a;
        char ip[64];
        memset(&a, 0, sizeof a);
        a.family = (uint16_t)kinds[i].family;
        memcpy(a.addr, kinds[i].b, 16);
        net_addr_ip(&a, ip, sizeof ip);
        CHECK(net_addr_kind(&a) == kinds[i].kind && !strcmp(ip, kinds[i].ip), "%s: kind %d, '%s'", kinds[i].ip,
              net_addr_kind(&a), ip);
    }

    /* this PC's addresses: none of loopback or link-local, each once */
    NetLocalAddr la[32];
    net_sock_startup();
    int nla = net_local_addrs(la, 32);
    CHECK(nla >= 0 && nla <= 32, "%d local addresses", nla);
    for (int i = 0; i < nla; i++) {
        char ip[64];
        int k = net_addr_kind(&la[i].addr);
        net_addr_ip(&la[i].addr, ip, sizeof ip);
        if (g_verbose)
            printf("local address %s (%s), kind %d\n", ip, la[i].ifname, k);
        CHECK((la[i].addr.family == NET_AF_INET || la[i].addr.family == NET_AF_INET6) && la[i].addr.port == 0 &&
                  k != NET_KIND_LOOPBACK && k != NET_KIND_LINK_LOCAL,
              "local address %s (%s)", ip, la[i].ifname);
        for (int j = 0; j < i; j++)
            CHECK(!net_addr_equal(&la[i].addr, &la[j].addr), "%s listed twice", ip);
    }
    net_sock_shutdown();

    /* the DNS PTR question for this PC's MagicDNS name, and an answer with a compression pointer */
    NetAddr ts;
    uint8_t q[128], ans[256];
    char name[128];
    memset(&ts, 0, sizeof ts);
    ts.family = NET_AF_INET;
    ts.addr[0] = 100, ts.addr[1] = 101, ts.addr[2] = 102, ts.addr[3] = 103;
    int qn = net_dns_ptr_query(&ts, 0xBEEF, q, sizeof q);
    static const uint8_t qname[] = "\003103\003102\003101\003100\007in-addr\004arpa";
    CHECK(qn == 12 + (int)sizeof qname + 4 && q[0] == 0xBE && q[1] == 0xEF && q[5] == 1 &&
              !memcmp(q + 12, qname, sizeof qname) && q[qn - 3] == 12 && q[qn - 1] == 1,
          "the question: %d bytes", qn);
    memcpy(ans, q, (size_t)qn);
    ans[2] |= 0x80;   /* an answer */
    ans[7] = 1;       /* one record */
    int an = qn;
    static const uint8_t rr[] = {0xC0, 12, 0, 12, 0, 1, 0, 0, 0, 60, 0, 29,          /* the question's name, PTR, IN */
                                 11, 'd', 'e', 's', 'k', 't', 'o', 'p', '-', 'a', 'b', 'c',
                                 8, 't', 'a', 'i', 'l', '1', '2', '3', '4', 2, 't', 's', 3, 'n', 'e', 't', 0};
    memcpy(ans + an, rr, sizeof rr);
    an += (int)sizeof rr;
    CHECK(net_dns_ptr_answer(ans, an, 0xBEEF, name, sizeof name) == 0 && !strcmp(name, "desktop-abc.tail1234.ts.net"),
          "the answer's name: '%s'", name);
    CHECK(net_dns_ptr_answer(ans, an, 0xBEEE, name, sizeof name) != 0, "another question's answer read");
    CHECK(net_dns_ptr_answer(ans, an - 5, 0xBEEF, name, sizeof name) != 0, "a cut answer read");
    ans[3] |= 3;   /* NXDOMAIN */
    CHECK(net_dns_ptr_answer(ans, an, 0xBEEF, name, sizeof name) != 0, "no such name read");
    ans[3] &= 0xF0;
    ans[qn] = 0xC0, ans[qn + 1] = (uint8_t)qn;   /* a pointer to itself */
    CHECK(net_dns_ptr_answer(ans, an, 0xBEEF, name, sizeof name) != 0, "a pointer loop read");
}

static void test_sim(void)
{
    NetSimModel m;
    char err[200], d[400];
    CHECK(net_sim_parse("delay=150,jitter=40,loss=10,dup=2,reorder=2", &m, err, sizeof err) == 0, "%s", err);
    CHECK(m.active && m.phase[0].delay_ms == 150 && m.phase[0].loss_pct == 10, "values");
    CHECK(net_sim_parse("delay=10,at=20,delay=200,at=40,delay=30,pause=5:3,burst=2:0.5:30,spike=3:0.2:100,seed=9",
                        &m, err, sizeof err) == 0,
          "%s", err);
    CHECK(m.nphases == 3 && m.phase[1].at_s == 20 && m.phase[1].delay_ms == 200 && m.phase[2].delay_ms == 30 &&
              m.pause_len_s == 3 && m.burst_loss_pct == 30 && m.spike_ms == 100 && m.seed == 9,
          "phases");
    net_sim_describe(&m, d, sizeof d);
    printf("sim: %s\n", d);
    CHECK(net_sim_parse("delay=-1", &m, err, sizeof err) != 0, "a negative delay");
    CHECK(net_sim_parse("loss=101", &m, err, sizeof err) != 0, "101%% loss");
    CHECK(net_sim_parse("bogus=1", &m, err, sizeof err) != 0, "an unknown key");
    CHECK(net_sim_parse("delay", &m, err, sizeof err) != 0, "no value");
    CHECK(net_sim_parse("at=5,at=3", &m, err, sizeof err) != 0, "phases out of order");
    CHECK(net_sim_parse("", &m, err, sizeof err) == 0 && !m.active, "empty");
    /* the same seed gives the same fates; the pause holds what arrives */
    net_sim_parse("delay=50,jitter=20,loss=20,dup=5,reorder=5,pause=1:1", &m, err, sizeof err);
    uint64_t sums[2] = {0, 0};
    for (int run = 0; run < 2; run++) {
        NetSim *s = net_sim_new(&m, 77, 0);
        NetAddr a = vnet_addr(1), from;
        uint8_t b[8];
        for (uint64_t t = 0; t < 3000 * MS; t += MS) {
            if (t % (5 * MS) == 0) {
                memcpy(b, &t, 8);
                net_sim_push(s, t, &a, b, 8, 0);
            }
            int n;
            while ((n = net_sim_pop(s, t, &from, b, 8, NULL)) > 0) {
                uint64_t sent;
                memcpy(&sent, b, 8);
                CHECK(!net_sim_frozen(s, t), "released in the pause");
                sums[run] = sums[run] * 31 + sent + t;
                if (sent >= 1000 * MS && sent < 2000 * MS)
                    CHECK(t >= 2000 * MS, "a packet that came in the pause left at %.3f s", t / 1e9);
            }
        }
        NetSimStats st;
        net_sim_stats(s, &st);
        if (run == 0)
            printf("sim: 600 in, %llu dropped, %llu duplicated, %llu reordered\n", (unsigned long long)st.dropped,
                   (unsigned long long)st.duplicated, (unsigned long long)st.reordered);
        CHECK(st.dropped > 80 && st.dropped < 170, "20%% of 600 dropped: %llu", (unsigned long long)st.dropped);
        net_sim_free(s);
    }
    CHECK(sums[0] == sums[1], "the same seed, different fates");
}

static void test_sockets(void)
{
    NetSock a, b;
    char err[200];
    NetAddr to, from;
    uint8_t buf[2048];
    CHECK(net_sock_startup() == 0, "startup");
    CHECK(net_sock_open(&a, NULL, 0, err, sizeof err) == 0, "dual-stack any: %s", err);
    CHECK(net_sock_open(&b, "127.0.0.1", 0, err, sizeof err) == 0, "127.0.0.1: %s", err);
    int pa = net_sock_port(&a), pb = net_sock_port(&b);
    printf("sockets: any-address socket %s (port %d), 127.0.0.1 port %d\n",
           a.family == NET_AF_INET6 ? (a.dual ? "IPv6 dual-stack" : "IPv6 only") : "IPv4", pa, pb);
    CHECK(net_sock_resolve(&b, "127.0.0.1", pa, &to, err, sizeof err) == 0, "%s", err);
    CHECK(net_sock_wait(&a, 1000) == 0, "nothing waits yet");
    CHECK(net_sock_recv(&a, &from, buf, sizeof buf) == 0, "nothing to read yet");
    CHECK(net_sock_send(&b, &to, "hello", 5) == 0, "send");
    CHECK(net_sock_wait(&a, 500000) == 1, "readable");
    int n = net_sock_recv(&a, &from, buf, sizeof buf);
    CHECK(n == 5 && !memcmp(buf, "hello", 5), "received %d", n);
    char s[80];
    net_addr_str(&from, s, sizeof s);
    CHECK(from.family == NET_AF_INET && from.port == pb, "an IPv4 sender on the dual-stack socket is IPv4: %s", s);
    /* answer to the address received */
    CHECK(net_sock_send(&a, &from, "back", 4) == 0, "send back");
    CHECK(net_sock_wait(&b, 500000) == 1, "readable back");
    n = net_sock_recv(&b, &from, buf, sizeof buf);
    CHECK(n == 4, "received back %d", n);
    /* an oversized datagram is reported as such, not truncated into something parseable */
    memset(buf, 1, 1600);
    net_sock_send(&b, &to, buf, 1600);
    net_sock_wait(&a, 500000);
    n = net_sock_recv(&a, &from, buf, 1500);
    CHECK(n == -2, "an oversized datagram: %d", n);
    /* a send to a closed port, then receive: no error (Windows' WSAECONNRESET is switched off) */
    NetSock c;
    CHECK(net_sock_open(&c, "127.0.0.1", 0, err, sizeof err) == 0, "%s", err);
    NetAddr dead;
    net_sock_resolve(&c, "127.0.0.1", net_sock_port(&c), &dead, err, sizeof err);
    net_sock_close(&c);
    net_sock_send(&b, &dead, "x", 1);
    net_os_sleep_ns(20 * MS);
    CHECK(net_sock_recv(&b, &from, buf, sizeof buf) == 0, "a receive after an ICMP unreachable");
    /* IPv6 loopback, where the host has it */
    if (a.family == NET_AF_INET6 && net_sock_resolve(&a, "::1", pa, &to, err, sizeof err) == 0) {
        NetSock d6;
        if (net_sock_open(&d6, "::1", 0, err, sizeof err) == 0) {
            net_sock_send(&d6, &to, "six", 3);
            CHECK(net_sock_wait(&a, 500000) == 1, "IPv6 readable");
            n = net_sock_recv(&a, &from, buf, sizeof buf);
            CHECK(n == 3 && from.family == NET_AF_INET6, "IPv6 datagram %d family %d", n, from.family);
            net_addr_str(&from, s, sizeof s);
            printf("sockets: IPv6 loopback from %s\n", s);
            net_sock_close(&d6);
        } else {
            printf("sockets: no IPv6 loopback here (%s)\n", err);
        }
    }
    CHECK(net_sock_resolve(&b, "localhost", 1234, &to, err, sizeof err) == 0, "localhost: %s", err);
    if (net_sock_resolve(&b, "no-such-host.invalid", 1234, &to, err, sizeof err) != 0)
        printf("sockets: an unknown name: %s\n", err);
    else
        printf("sockets: this host's resolver answers even for .invalid names\n");
    net_sock_close(&a);
    net_sock_close(&b);
    net_sock_shutdown();
}

/* ---------------------------------------------------------------- virtual sessions */

typedef struct VPair {
    VNet v;
    VUser hu, gu;
    NetSession *h, *g;
    Driver dh, dg;
} VPair;

typedef struct VOpts {
    const char *host_sim, *guest_sim;
    uint32_t ticks;
    int unique;
    int delay_fixed;
    const char *host_code, *guest_code;
    const char *host_build, *guest_build;
    int host_files, guest_files;   /* random files of 32 KiB */
    const char *host_settings, *guest_settings;
    int timeout_ms;
    int manual_accept;
    uint64_t seed;
} VOpts;

static uint8_t g_files[2][NET_FILES_MAX][32768];

static void vconfig(NetConfig *c, VUser *u, VNode *node, NetRole role, const VOpts *o)
{
    net_config_defaults(c);
    c->role = role;
    c->ports = role == NET_HOST ? 0x1 : 0x2;
    c->transport = &node->tr;
    c->peer_addr = vnet_addr(0);
    c->now_ns = vuser_clock;
    c->log = vuser_log;
    c->user = u;
    c->build = role == NET_HOST ? (o->host_build ? o->host_build : "test") : (o->guest_build ? o->guest_build : "test");
    c->sim = role == NET_HOST ? (o->host_sim ? o->host_sim : "") : (o->guest_sim ? o->guest_sim : "");
    c->seed = o->seed + (uint64_t)role + 1;
    c->auto_accept = !o->manual_accept;
    c->delay_fixed = o->delay_fixed;
    c->join_code = role == NET_HOST ? o->host_code : o->guest_code;
    if (o->timeout_ms)
        c->timeout_ms = o->timeout_ms;
    const char *set = role == NET_HOST ? o->host_settings : o->guest_settings;
    if (set) {
        c->settings = set;
        c->settings_size = strlen(set);
    }
    int nf = role == NET_HOST ? o->host_files : o->guest_files;
    for (int i = 0; i < nf; i++) {
        uint8_t *f = g_files[role][i];
        for (int k = 0; k < 32768; k++)
            f[k] = (uint8_t)(k * 7 + i * 13 + role * 101 + (k >> 9));
        c->files[i].data = f;
        c->files[i].size = i == 1 ? 1000 : 32768;   /* one short file: the last chunk's length */
    }
    c->nfiles = nf;
}

static int vpair_open(VPair *p, const VOpts *o)
{
    char err[200];
    NetConfig hc, gc;
    memset(p, 0, sizeof *p);
    vnet_init(&p->v);
    p->hu.v = p->gu.v = &p->v;
    p->hu.log.tag = "host";
    p->gu.log.tag = "guest";
    vconfig(&hc, &p->hu, &p->v.node[0], NET_HOST, o);
    vconfig(&gc, &p->gu, &p->v.node[1], NET_GUEST, o);
    p->h = net_create(&hc, err, sizeof err);
    CHECK(p->h, "host: %s", err);
    p->g = net_create(&gc, err, sizeof err);
    CHECK(p->g, "guest: %s", err);
    if (!p->h || !p->g)
        return -1;
    drv_init(&p->dh, p->h, 0, o->ticks, o->seed ^ 0x1111, PERIOD, 10000 * MS);
    drv_init(&p->dg, p->g, 1, o->ticks, o->seed ^ 0x2222, PERIOD, 10000 * MS);
    p->dh.unique = p->dg.unique = o->unique;
    return 0;
}

static void vpair_close(VPair *p)
{
    net_destroy(p->h);
    net_destroy(p->g);
    drv_free(&p->dh);
    drv_free(&p->dg);
    vnet_free(&p->v);
}

/* Advance the virtual clock by dt in 250 us steps, pumping both and running their loops. */
static void vrun(VPair *p, uint64_t dt, int drive)
{
    for (uint64_t t = 0; t < dt; t += 250000) {
        p->v.now += 250000;
        if (!p->v.node[0].cut)
            net_poll(p->h);
        if (!p->v.node[1].cut)
            net_poll(p->g);
        if (drive) {
            if (!p->v.node[0].cut)
                drv_step(&p->dh, p->v.now);
            if (!p->v.node[1].cut)
                drv_step(&p->dg, p->v.now);
        }
    }
}

/* Run until both drivers are done (or limit). */
static void vrun_ticks(VPair *p, uint64_t limit)
{
    uint64_t t0 = p->v.now;
    while (!(p->dh.done && p->dg.done) && p->v.now - t0 < limit)
        vrun(p, 250000, 1);
}

static uint32_t first_difference(const Driver *a, const Driver *b)
{
    uint32_t n = a->n < b->n ? a->n : b->n;
    for (uint32_t i = 0; i < n; i++)
        if (a->hashes[i] != b->hashes[i])
            return i;
    return NET_NONE;
}

static void test_session_basics(void)
{
    VPair p;
    VOpts o;
    NetStats hs, gs;
    char msg[200];
    uint32_t det;
    /* the handshake with settings and files over a lossy link; the guest's own settings differ */
    memset(&o, 0, sizeof o);
    o.ticks = 600;
    o.host_sim = o.guest_sim = "delay=30,jitter=10,loss=20,dup=5,reorder=5";
    o.host_files = 2;
    o.guest_files = 3;
    o.host_settings = "[game]\nframe_rate = original\ncars = 8\n";
    o.guest_settings = "[game]\nframe_rate = 60\n";
    o.seed = 1;
    vpair_open(&p, &o);
    vrun_ticks(&p, 60000 * MS);
    CHECK(p.dh.n == 600 && p.dg.n == 600, "handshake run: %u and %u ticks", p.dh.n, p.dg.n);
    const void *data;
    size_t size;
    CHECK(net_peer_settings(p.g, &data, &size) && size == strlen(o.host_settings) &&
              !memcmp(data, o.host_settings, size),
          "the guest has the host's settings");
    CHECK(net_settings_differ(p.g), "the guest's own settings differed");
    CHECK(net_peer_files(p.h) == 3 && net_peer_files(p.g) == 2, "file counts");
    for (int i = 0; i < 3; i++)
        CHECK(net_peer_file(p.h, i, &data, &size) && size == (i == 1 ? 1000u : 32768u) &&
                  !memcmp(data, g_files[NET_GUEST][i], size),
              "the host has the guest's file %d", i);
    for (int i = 0; i < 2; i++)
        CHECK(net_peer_file(p.g, i, &data, &size) && !memcmp(data, g_files[NET_HOST][i], size),
              "the guest has the host's file %d", i);
    CHECK(first_difference(&p.dh, &p.dg) == NET_NONE, "hashes differ");
    net_stats(p.h, &hs);
    CHECK(hs.resends > 0, "a lossy handshake resends (%llu)", (unsigned long long)hs.resends);
    printf("session: handshake with settings and 5 files at 20%% loss, then 600 ticks: ok (resends %llu)\n",
           (unsigned long long)hs.resends);
    /* BYE: the host leaves; the guest ends at once */
    net_end(p.h, NET_END_LOCAL);
    uint64_t t0 = p.v.now;
    while (net_state(p.g) != NET_ENDED && p.v.now - t0 < 5000 * MS)
        vrun(&p, MS, 0);
    CHECK(net_end_reason(p.g, &det, msg, sizeof msg) == NET_END_PEER_LEFT, "the guest ended: %s", msg);
    printf("session: BYE reached the guest after %.0f ms (one way %s)\n", (p.v.now - t0) / 1e6, "30 ± 10 ms");
    CHECK(p.v.now - t0 < 200 * MS, "BYE took %.0f ms", (p.v.now - t0) / 1e6);
    vpair_close(&p);

    /* the timeout: the guest disappears mid-run */
    memset(&o, 0, sizeof o);
    o.ticks = 100000;
    o.seed = 2;
    vpair_open(&p, &o);
    vrun(&p, 3000 * MS, 1);
    CHECK(p.dh.n > 100, "running before the cut (%u)", p.dh.n);
    p.v.node[1].cut = 1;
    t0 = p.v.now;
    while (net_state(p.h) != NET_ENDED && p.v.now - t0 < 30000 * MS)
        vrun(&p, 10 * MS, 1);
    CHECK(net_end_reason(p.h, &det, msg, sizeof msg) == NET_END_TIMEOUT, "the host ended: %s", msg);
    double took = (p.v.now - t0) / 1e9;
    CHECK(took > 14.9 && took < 15.2, "the timeout came after %.2f s", took);
    net_stats(p.h, &hs);
    printf("session: the guest vanished; the host ended after %.2f s (TIMEOUT), %llu stall(s)\n", took,
           (unsigned long long)hs.stalls);
    vpair_close(&p);

    /* a deliberate desync at tick 777: both sides name it */
    memset(&o, 0, sizeof o);
    o.ticks = 2000;
    o.host_sim = o.guest_sim = "delay=40,jitter=10";
    o.seed = 3;
    vpair_open(&p, &o);
    p.dg.desync_at = 777;
    vrun_ticks(&p, 120000 * MS);
    vrun(&p, 1000 * MS, 0);
    NetEnd eh = net_end_reason(p.h, &det, msg, sizeof msg);
    CHECK(eh == NET_END_DESYNC && det == 777, "host: %s %u (%s)", net_end_name(eh), det, msg);
    printf("session: desync at 777: host says %s at %u (%s)\n", net_end_name(eh), det, msg);
    NetEnd eg = net_end_reason(p.g, &det, msg, sizeof msg);
    CHECK(eg == NET_END_DESYNC && det == 777, "guest: %s %u (%s)", net_end_name(eg), det, msg);
    printf("session: desync at 777: guest says %s at %u (%s)\n", net_end_name(eg), det, msg);
    CHECK(first_difference(&p.dh, &p.dg) == 777, "the toy's first difference is %u", first_difference(&p.dh, &p.dg));
    vpair_close(&p);

    /* builds that differ: the guest ends with VERSION and both builds named; the host keeps listening */
    memset(&o, 0, sizeof o);
    o.ticks = 10;
    o.host_build = "aaaa111";
    o.guest_build = "bbbb222";
    vpair_open(&p, &o);
    vrun(&p, 1000 * MS, 1);
    CHECK(net_end_reason(p.g, &det, msg, sizeof msg) == NET_END_VERSION && strstr(msg, "aaaa111") &&
              strstr(msg, "bbbb222"),
          "guest: %s", msg);
    printf("session: different builds: %s\n", msg);
    CHECK(net_state(p.h) == NET_CONNECTING, "the host still listens (%s)", net_state_name(net_state(p.h)));
    vpair_close(&p);

    /* join codes: the same code connects; a different code, or none, gets no answer */
    const char *codes[3][2] = {{"blue-fox-42", "blue-fox-42"}, {"blue-fox-42", "red-fox-42"}, {"blue-fox-42", NULL}};
    for (int i = 0; i < 3; i++) {
        memset(&o, 0, sizeof o);
        o.ticks = 120;
        o.host_code = codes[i][0];
        o.guest_code = codes[i][1];
        o.timeout_ms = 3000;
        vpair_open(&p, &o);
        vrun_ticks(&p, 10000 * MS);
        NetEnd e = net_end_reason(p.g, &det, msg, sizeof msg);
        net_stats(p.h, &hs);
        if (i == 0) {
            CHECK(p.dg.n == 120 && first_difference(&p.dh, &p.dg) == NET_NONE && e == NET_END_NONE,
                  "the same code: %u ticks, %s", p.dg.n, msg);
        } else {
            CHECK(e == NET_END_NO_ANSWER && hs.rejected_tag > 0, "code %s against %s: %s (%llu tags rejected)",
                  codes[i][0], codes[i][1] ? codes[i][1] : "none", net_end_name(e),
                  (unsigned long long)hs.rejected_tag);
        }
        printf("session: join code %s / %s: %s\n", codes[i][0], codes[i][1] ? codes[i][1] : "(none)",
               i == 0 ? "connected, 120 ticks" : msg);
        vpair_close(&p);
    }

    /* a stranger replays the guest's (tagged) HELLO ahead of it: the host answers both, and only the guest, which
     * can use its session, becomes the peer */
    for (int coded = 0; coded < 2; coded++) {
        memset(&o, 0, sizeof o);
        o.ticks = 120;
        if (coded)
            o.host_code = o.guest_code = "replay-me";
        vpair_open(&p, &o);
        p.v.now += 250000;
        net_poll(p.g);   /* the guest's first HELLO, now in the host's queue */
        VNode *hq = &p.v.node[0];
        CHECK(hq->n == 1, "one HELLO waiting (%d)", hq->n);
        VPkt hello = hq->q[hq->head];
        hq->n = 0;
        NetAddr stranger = vnet_addr(3), guest = vnet_addr(1);
        vnet_inject(&p.v, 0, &stranger, hello.data, hello.len);
        vnet_inject(&p.v, 0, &guest, hello.data, hello.len);
        vrun_ticks(&p, 20000 * MS);
        CHECK(p.dh.n == 120 && p.dg.n == 120 && first_difference(&p.dh, &p.dg) == NET_NONE,
              "%s: the guest after a replayed HELLO: %u ticks", coded ? "join code" : "no code", p.dg.n);
        CHECK(p.v.node[3].n >= 1, "the stranger got its own HELLO_ACK (%d)", p.v.node[3].n);
        printf("session: a HELLO replayed from elsewhere ahead of the guest's (%s): the guest is the peer\n",
               coded ? "join code" : "no code");
        vpair_close(&p);
    }

    /* the caller's acceptance: both wait in NET_ACCEPT with everything there, then accept (the guest early) */
    memset(&o, 0, sizeof o);
    o.ticks = 120;
    o.manual_accept = 1;
    o.host_settings = "[game]\ncars = 12\n";
    o.guest_files = 1;
    o.host_sim = o.guest_sim = "delay=15,loss=10";
    vpair_open(&p, &o);
    net_accept(p.g);   /* before anything arrived: accepted as it does */
    uint64_t t1 = p.v.now;
    while (net_state(p.h) != NET_ACCEPT && p.v.now - t1 < 5000 * MS)
        vrun(&p, MS, 1);
    CHECK(net_state(p.h) == NET_ACCEPT, "the host waits for its caller (%s)", net_state_name(net_state(p.h)));
    CHECK(net_peer_file(p.h, 0, &data, &size) && size == 32768, "the guest's file, to check, in NET_ACCEPT");
    vrun(&p, 500 * MS, 1);
    CHECK(net_state(p.h) == NET_ACCEPT && p.dh.n == 0, "nothing starts before the host accepts");
    net_accept(p.h);
    vrun_ticks(&p, 20000 * MS);
    CHECK(p.dh.n == 120 && p.dg.n == 120 && first_difference(&p.dh, &p.dg) == NET_NONE, "after accepting: %u %u",
          p.dh.n, p.dg.n);
    CHECK(net_peer_settings(p.g, &data, &size) && size == strlen(o.host_settings), "the guest has the settings");
    printf("session: manual acceptance: the host waits in accept with the guest's file, then the run\n");
    vpair_close(&p);

    /* fixed delay */
    memset(&o, 0, sizeof o);
    o.ticks = 300;
    o.delay_fixed = 7;
    o.host_sim = o.guest_sim = "delay=50";
    vpair_open(&p, &o);
    vrun_ticks(&p, 30000 * MS);
    net_stats(p.h, &hs);
    net_stats(p.g, &gs);
    CHECK(hs.delay == 7 && gs.delay == 7 && hs.delay_changes == 0 && gs.delay_changes == 0, "fixed delay %d %d",
          hs.delay, gs.delay);
    CHECK(first_difference(&p.dh, &p.dg) == NET_NONE && p.dh.n == 300, "fixed-delay run");
    printf("session: --net-delay 7 at 50 ms: delay stays 7, %llu stall(s)\n", (unsigned long long)hs.stalls);
    vpair_close(&p);
}

/* Spoofed, hostile and malformed packets into a running session: rejected, and the session runs on. */
static void test_hostile(void)
{
    VPair p;
    VOpts o;
    NetStats hs0, hs, gs;
    uint8_t pl[1400], buf[2048];
    memset(&o, 0, sizeof o);
    o.ticks = 1200;
    o.seed = 5;
    o.host_sim = o.guest_sim = "delay=20";
    vpair_open(&p, &o);
    vrun(&p, 3000 * MS, 1);
    CHECK(p.dh.n > 50, "running (%u)", p.dh.n);
    net_stats(p.h, &hs0);
    /* the session id, from a packet of the guest's on its way to the host */
    uint64_t session = 0;
    for (int k = 0; k < 400 && !session; k++) {
        vrun(&p, 250000, 1);
        for (int i = 0; i < p.v.node[0].n && !session; i++) {
            VPkt *q = &p.v.node[0].q[(p.v.node[0].head + i) % p.v.node[0].cap];
            if (q->len >= NET_HEADER_SIZE)
                session = net_get64(q->data + 8);
        }
    }
    CHECK(session != 0, "found the session id");
    NetAddr guest = vnet_addr(1), stranger = vnet_addr(3);
    int rejected = 0;
    uint32_t cur = p.dg.n;
    /* 1. a spoofed source: a valid INPUT with another value for a retrace, from another address */
    int n = input_payload(pl, cur, cur + 1, 5, 0x2, 0, 0, 0);
    int len = build_packet(buf, NET_MSG_INPUT, session, pl, n, NULL);
    net_test_inject(p.h, buf, len, &stranger);
    rejected++;
    /* 2. the wrong session, from the guest's address */
    len = build_packet(buf, NET_MSG_INPUT, session ^ 1, pl, n, NULL);
    net_test_inject(p.h, buf, len, &guest);
    rejected++;
    /* 3. absurd retrace numbers, from the guest's address with the session */
    uint32_t absurd[] = {0xFFFFFF00u, cur + 5000, cur + 100000};
    for (int i = 0; i < 3; i++) {
        n = input_payload(pl, absurd[i], absurd[i], 3, 0x2, 0, 0, 0);
        len = build_packet(buf, NET_MSG_INPUT, session, pl, n, NULL);
        net_test_inject(p.h, buf, len, &guest);
        rejected++;
    }
    /* hashes for retraces not yet run */
    n = input_payload(pl, cur, cur, 0, 0x2, cur + 50, 10, 0);
    len = build_packet(buf, NET_MSG_INPUT, session, pl, n, NULL);
    net_test_inject(p.h, buf, len, &guest);
    rejected++;
    /* the wrong ports */
    n = input_payload(pl, cur, cur, 2, 0x1, 0, 0, 0);
    len = build_packet(buf, NET_MSG_INPUT, session, pl, n, NULL);
    net_test_inject(p.h, buf, len, &guest);
    rejected++;
    /* 4. random bytes, truncations, a header alone, a huge length */
    uint64_t r = 99;
    for (int i = 0; i < 2000; i++) {
        r = r * 6364136223846793005ull + 1442695040888963407ull;
        int l = (int)(r >> 40) % 1300;
        for (int k = 0; k < l; k++) {
            r = r * 6364136223846793005ull + 1442695040888963407ull;
            buf[k] = (uint8_t)(r >> 56);
        }
        if (i % 3 == 0 && l >= 8)
            memcpy(buf, "R2NP\1\10\0\1", 8);   /* past the magic */
        net_test_inject(p.h, buf, l, &guest);
        net_test_inject(p.g, buf, l, &stranger);
    }
    /* 5. a HELLO storm from strangers: rate-limited, ignored */
    NetHello h;
    memset(&h, 0, sizeof h);
    h.ports = 2;
    snprintf(h.build, sizeof h.build, "test");
    NetWriter w;
    net_w_init(&w, pl, sizeof pl);
    net_w_hello(&w, &h);
    for (int i = 0; i < 100; i++) {
        len = build_packet(buf, NET_MSG_HELLO, 0, pl, w.len, NULL);
        NetAddr s2 = vnet_addr(2 + (i & 1));
        net_test_inject(p.h, buf, len, &s2);
    }
    net_stats(p.h, &hs);
    CHECK(hs.rejected >= hs0.rejected + (uint64_t)rejected, "rejected %llu -> %llu", (unsigned long long)hs0.rejected,
          (unsigned long long)hs.rejected);
    CHECK(hs.rejected_source > hs0.rejected_source, "spoofed sources counted");
    CHECK(net_state(p.h) == NET_RUNNING && net_state(p.g) == NET_RUNNING, "still running: %s %s",
          net_state_name(net_state(p.h)), net_state_name(net_state(p.g)));
    vrun_ticks(&p, 60000 * MS);
    CHECK(p.dh.n == 1200 && p.dg.n == 1200 && first_difference(&p.dh, &p.dg) == NET_NONE,
          "the run completes the same after the garbage: %u %u", p.dh.n, p.dg.n);
    net_stats(p.g, &gs);
    printf("hostile: %llu packets rejected by the host (%llu spoofed sources), %llu by the guest; the run completes\n",
           (unsigned long long)(hs.rejected - hs0.rejected), (unsigned long long)hs.rejected_source,
           (unsigned long long)gs.rejected);
    /* 6. a different value for an input already known, from the peer: the protocol error ends it */
    uint32_t known = p.dh.n - 1;
    n = input_payload(pl, known, known, 1, 0x2, 0, 0, 0);
    len = build_packet(buf, NET_MSG_INPUT, session, pl, n, NULL);
    net_test_inject(p.h, buf, len, &guest);
    uint32_t det;
    char msg[200];
    NetEnd e = net_end_reason(p.h, &det, msg, sizeof msg);
    CHECK(e == NET_END_PROTOCOL, "a conflicting input: %s (%s)", net_end_name(e), msg);
    printf("hostile: a conflicting input from the peer's address: %s\n", msg);
    vpair_close(&p);

    /* with a join code, the same conflicting packet without the tag is just dropped */
    memset(&o, 0, sizeof o);
    o.ticks = 400;
    o.host_code = o.guest_code = "code-123";
    vpair_open(&p, &o);
    vrun(&p, 3000 * MS, 1);
    known = p.dh.n > 2 ? p.dh.n - 2 : 0;
    n = input_payload(pl, known, known, 1, 0x2, 0, 0, 0);
    uint8_t wrongkey[32];
    net_proto_key("guess", wrongkey);
    for (int i = 0; i < 2; i++) {
        len = build_packet(buf, NET_MSG_INPUT, 0, pl, n, i ? wrongkey : NULL);
        net_test_inject(p.h, buf, len, &guest);
    }
    vrun_ticks(&p, 30000 * MS);
    net_stats(p.h, &hs);
    CHECK(net_state(p.h) == NET_RUNNING && p.dh.n == 400 && first_difference(&p.dh, &p.dg) == NET_NONE &&
              hs.rejected_tag >= 2,
          "with a join code, forged packets are dropped (%s, %u ticks, %llu tags)", net_state_name(net_state(p.h)),
          p.dh.n, (unsigned long long)hs.rejected_tag);
    printf("hostile: with a join code, untagged and wrongly tagged INPUTs dropped (%llu); the run completes\n",
           (unsigned long long)hs.rejected_tag);
    vpair_close(&p);
}

/* The assignment rules across delay changes: each side's samples reach its port in order, repeated or dropped
 * only where its delay grew or shrank. */
static void test_assignment(void)
{
    VPair p;
    VOpts o;
    NetStats hs, gs;
    char why[200];
    memset(&o, 0, sizeof o);
    o.ticks = 4000;
    o.unique = 1;
    o.seed = 6;
    o.host_sim = o.guest_sim = "delay=10,at=15,delay=150,jitter=30,at=35,delay=20";
    vpair_open(&p, &o);
    vrun_ticks(&p, 200000 * MS);
    CHECK(p.dh.n == 4000 && p.dg.n == 4000 && first_difference(&p.dh, &p.dg) == NET_NONE, "assignment run");
    net_stats(p.h, &hs);
    net_stats(p.g, &gs);
    for (int side = 0; side < 2; side++) {
        const Driver *d = side ? &p.dg : &p.dh;
        const NetStats *st = side ? &gs : &hs;
        uint64_t rep, drop;
        int ok = drv_check_assignment(d, &rep, &drop, why, sizeof why) == 0;
        CHECK(ok, "%s: %s", side ? "guest" : "host", why);
        CHECK(drop == st->samples_dropped && rep <= st->samples_repeated && st->delay_changes >= 2,
              "%s: %llu drops (stats %llu), %llu repeats (stats %llu), %d changes", side ? "guest" : "host",
              (unsigned long long)drop, (unsigned long long)st->samples_dropped, (unsigned long long)rep,
              (unsigned long long)st->samples_repeated, st->delay_changes);
        printf("assignment: %s: samples in order, %llu repeated (delay grew), %llu dropped (shrank), delay %d-%d\n",
               side ? "guest" : "host", (unsigned long long)rep, (unsigned long long)drop, st->delay_min_seen,
               st->delay_max_seen);
    }
    /* and the other side sees exactly what this side assigned: the merged pads agree */
    int same = 1;
    for (uint32_t t = 0; t < 4000; t++)
        if (memcmp(&p.dh.own[t], &p.dg.other[t], sizeof(NetPad)) || memcmp(&p.dg.own[t], &p.dh.other[t], sizeof(NetPad)))
            same = 0;
    CHECK(same, "the merged pads differ between the sides");
    vpair_close(&p);
}

static int cmd_unit(void)
{
    test_blake2s();
    test_proto();
    test_addresses();
    test_sim();
    test_sockets();
    test_session_basics();
    test_hostile();
    test_assignment();
    printf("%s (%d failure(s))\n", g_fail ? "FAILED" : "unit: all passed", g_fail);
    return g_fail ? 1 : 0;
}

/* ---------------------------------------------------------------- a model, 10,000 ticks on a virtual clock */

static void print_stats(const char *who, NetSession *s)
{
    NetStats st;
    char line[600];
    net_stats(s, &st);
    net_stats_line(&st, line, sizeof line);
    printf("  %s: %s\n", who, line);
}

static int cmd_sim(const char *name, uint32_t ticks)
{
    const Model *m = find_model(name);
    if (!m) {
        printf("no model %s\n", name);
        return 2;
    }
    VPair p;
    VOpts o;
    memset(&o, 0, sizeof o);
    o.ticks = ticks;
    o.host_sim = m->host;
    o.guest_sim = m->guest;
    o.seed = 0x2049;
    printf("model %s: %s; %u ticks on a virtual clock\n", m->name, m->what, ticks);
    vpair_open(&p, &o);
    vrun_ticks(&p, (uint64_t)ticks * 3 * PERIOD + 60000 * MS);
    vrun(&p, 2000 * MS, 0);   /* the last hashes across */
    NetStats hs, gs;
    net_stats(p.h, &hs);
    net_stats(p.g, &gs);
    uint32_t diff = first_difference(&p.dh, &p.dg);
    CHECK(p.dh.n == ticks && p.dg.n == ticks, "ticks run: host %u guest %u", p.dh.n, p.dg.n);
    CHECK(diff == NET_NONE, "the hashes differ from tick %u", diff);
    CHECK(hs.end == NET_END_NONE && gs.end == NET_END_NONE, "ended: %s %s", net_end_name(hs.end),
          net_end_name(gs.end));
    CHECK(hs.hash_checks >= ticks - 2 && gs.hash_checks >= ticks - 2, "hash checks %llu %llu",
          (unsigned long long)hs.hash_checks, (unsigned long long)gs.hash_checks);
    CHECK((int)(hs.stalls + gs.stalls) <= m->max_stalls * (int)(ticks / 10000 + 1), "stalls %llu + %llu over %d",
          (unsigned long long)hs.stalls, (unsigned long long)gs.stalls, m->max_stalls);
    char th[400], tg[400];
    drv_trace_str(&p.dh, th, sizeof th);
    drv_trace_str(&p.dg, tg, sizeof tg);
    double rate = drv_rate(&p.dh, ticks / 4, ticks - 1), grate = drv_rate(&p.dg, ticks / 4, ticks - 1);
    printf("  same hash at every tick on both: %s (%llu + %llu checks, %llu + %llu window checks)\n",
           diff == NET_NONE ? "yes" : "NO", (unsigned long long)hs.hash_checks, (unsigned long long)gs.hash_checks,
           (unsigned long long)hs.window_hash_checks, (unsigned long long)gs.window_hash_checks);
    printf("  delay every 10 s (delay@index): host %s; guest %s\n", th, tg);
    printf("  stalls: host %llu (%.0f ms, max %.0f), guest %llu (%.0f ms, max %.0f)\n", (unsigned long long)hs.stalls,
           hs.stall_ms_total, hs.stall_ms_max, (unsigned long long)gs.stalls, gs.stall_ms_total, gs.stall_ms_max);
    printf("  ticks a second over the last 3/4: host %.3f, guest %.3f; time sync %llu + %llu corrections (%.1f + %.1f "
           "ms)\n",
           rate, grate, (unsigned long long)hs.sync_corrections, (unsigned long long)gs.sync_corrections,
           hs.sync_ms_total, gs.sync_ms_total);
    if (m->rate_check)
        CHECK(rate > 59.99 && grate > 59.99, "rate %.3f %.3f", rate, grate);
    print_stats("host", p.h);
    print_stats("guest", p.g);
    vpair_close(&p);
    printf("%s\n", g_fail ? "FAILED" : "ok");
    return g_fail ? 1 : 0;
}

/* ---------------------------------------------------------------- fuzz */

static uint64_t g_rng = 1;
static uint64_t rnd(void)
{
    g_rng ^= g_rng >> 12;
    g_rng ^= g_rng << 25;
    g_rng ^= g_rng >> 27;
    return g_rng * 0x2545F4914F6CDD1Dull;
}

static int cmd_fuzz(long iters)
{
    uint8_t buf[2048], pl[1400];
    NetMsg m;
    long ok = 0;
    /* 1. the parser: random bytes, and valid packets of every type mutated */
    for (long i = 0; i < iters; i++) {
        int len, kind = (int)(rnd() % 4);
        if (kind == 0) {
            len = (int)(rnd() % 1400);
            for (int k = 0; k < len; k++)
                buf[k] = (uint8_t)rnd();
            if (len >= 6 && rnd() % 2) {
                memcpy(buf, "R2NP\1", 5);
                buf[5] = (uint8_t)(1 + rnd() % 12);
            }
        } else {
            int type = 1 + (int)(rnd() % 12), n = 0;
            NetWriter w;
            net_w_init(&w, pl, sizeof pl);
            switch (type) {
            case NET_MSG_HELLO:
            case NET_MSG_HELLO_ACK: {
                NetHello h;
                memset(&h, 0, sizeof h);
                h.ports = (uint8_t)(1 + rnd() % 15);
                h.nfiles = (uint8_t)(rnd() % 9);
                snprintf(h.build, sizeof h.build, "b%llu", (unsigned long long)(rnd() % 1000));
                net_w_hello(&w, &h);
                n = w.len;
                break;
            }
            case NET_MSG_INPUT:
            case NET_MSG_HASH: {   /* one draw a statement: the same packets on every compiler */
                uint32_t cur = (uint32_t)(rnd() % 5000), first = (uint32_t)(rnd() % 5000);
                int count = type == NET_MSG_HASH ? 0 : (int)(rnd() % 121);
                uint8_t ports = (uint8_t)(1 + rnd() % 15);
                uint32_t hfirst = (uint32_t)(rnd() % 5000);
                int hcount = (int)(rnd() % 40), wh = (int)(rnd() % 2);
                n = input_payload(pl, cur, first, count, ports, hfirst, hcount, wh);
                break;
            }
            default:
                n = (int)(rnd() % 1100);
                for (int k = 0; k < n; k++)
                    pl[k] = (uint8_t)rnd();
            }
            len = build_packet(buf, type, rnd(), pl, n, NULL);
            if (len < 0)
                continue;
            int muts = (int)(rnd() % 6);
            for (int k = 0; k < muts; k++) {
                uint64_t at = rnd() % (uint64_t)len;
                buf[at] ^= (uint8_t)(1u << (rnd() % 8));
            }
            if (kind == 2)
                len = (int)(rnd() % (uint64_t)(len + 1));   /* truncated */
            if (kind == 3 && rnd() % 2)
                len += (int)(rnd() % 40);   /* extended with garbage */
        }
        if (net_proto_parse(buf, len, &m) == 0)
            ok++;
    }
    printf("fuzz: the parser took %ld packets, %ld of them well-formed\n", iters, ok);

    /* 2. running sessions fed garbage from the peer's own address and from strangers, and mutated copies of their
     *    real traffic; with a join code every mutation must be dropped and the run must complete the same */
    for (int round = 0; round < 6; round++) {
        VPair p;
        VOpts o;
        memset(&o, 0, sizeof o);
        o.ticks = 900;
        o.seed = 100 + (uint64_t)round;
        o.host_sim = o.guest_sim = round % 2 ? "delay=40,jitter=20,loss=5,dup=5" : "delay=10";
        int coded = round >= 3;
        if (coded)
            o.host_code = o.guest_code = "fuzz-code";
        vpair_open(&p, &o);
        uint8_t keep[8][1500];
        int keeplen[8] = {0}, nkeep = 0;
        uint64_t injected = 0;
        while (!(p.dh.done && p.dg.done) && p.v.now < 120000 * MS) {
            for (int nd = 0; nd < 2; nd++) {   /* copy real datagrams in flight */
                VNode *q = &p.v.node[nd];
                if (q->n && rnd() % 4 == 0) {
                    VPkt *x = &q->q[q->head];
                    int slot = nkeep < 8 ? nkeep++ : (int)(rnd() % 8);
                    memcpy(keep[slot], x->data, (size_t)x->len);
                    keeplen[slot] = x->len;
                }
            }
            if (nkeep && rnd() % 3 == 0) {
                int k = (int)(rnd() % (uint64_t)nkeep), len = keeplen[k];
                memcpy(buf, keep[k], (size_t)len);
                int muts = 1 + (int)(rnd() % 4);
                for (int j = 0; j < muts; j++) {
                    uint64_t at = NET_HEADER_SIZE + rnd() % (uint64_t)(len > NET_HEADER_SIZE ? len - NET_HEADER_SIZE : 1);
                    buf[at] ^= (uint8_t)(1u << (rnd() % 8));
                }
                if (rnd() % 4 == 0)
                    len = (int)(rnd() % (uint64_t)len);
                int to = (int)(rnd() % 2);
                /* without a code only strangers send garbage (the peer's address would be believed); with a code,
                 * the peer's own address too */
                NetAddr from = coded && rnd() % 2 ? vnet_addr(1 - to) : vnet_addr(2 + (int)(rnd() % 2));
                vnet_inject(&p.v, to, &from, buf, len);
                injected++;
            }
            if (rnd() % 5 == 0) {
                int len = (int)(rnd() % 1300);
                for (int k = 0; k < len; k++)
                    buf[k] = (uint8_t)rnd();
                if (len > 8 && rnd() % 2)
                    memcpy(buf, "R2NP\1\10\0\1", 8);
                int to = (int)(rnd() % 2);
                NetAddr from = rnd() % 2 ? vnet_addr(1 - to) : vnet_addr(3);
                vnet_inject(&p.v, to, &from, buf, len);
                injected++;
            }
            vrun(&p, 250000, 1);
        }
        NetStats hs, gs;
        net_stats(p.h, &hs);
        net_stats(p.g, &gs);
        int same = first_difference(&p.dh, &p.dg) == NET_NONE;
        CHECK(same, "round %d: hashes differ", round);
        /* garbage from the peer's address without a code is believed when it parses (by design: Tailscale's or the
         * join code's authentication is what keeps it out); random bytes almost never parse, so expect a run */
        CHECK(p.dh.n == 900 && p.dg.n == 900, "round %d (%s): %u and %u ticks (%s/%s)", round,
              coded ? "join code" : "no code", p.dh.n, p.dg.n, net_end_name(hs.end), net_end_name(gs.end));
        printf("fuzz: session round %d (%s, %s): %llu injected, %llu + %llu rejected, %u ticks, hashes %s\n", round,
               coded ? "join code" : "no code", o.host_sim, (unsigned long long)injected,
               (unsigned long long)hs.rejected, (unsigned long long)gs.rejected, p.dh.n, same ? "the same" : "DIFFER");
        vpair_close(&p);
    }
    printf("%s\n", g_fail ? "FAILED" : "fuzz: ok");
    return g_fail ? 1 : 0;
}

/* ---------------------------------------------------------------- two threads over UDP, in real time */

typedef struct ThreadArg {
    Driver *d;
} ThreadArg;

static void *peer_thread(void *p)
{
    Driver *d = p;
    while (drv_tick(d) > 0) {
    }
    return NULL;
}

static int cmd_threads(const char *name, uint32_t ticks)
{
    const Model *m = find_model(name);
    if (!m) {
        printf("no model %s\n", name);
        return 2;
    }
    char err[200], peer[64];
    LogBuf hl, gl;
    memset(&hl, 0, sizeof hl);
    memset(&gl, 0, sizeof gl);
    hl.tag = "host";
    gl.tag = "guest";
    NetConfig hc, gc;
    net_config_defaults(&hc);
    hc.role = NET_HOST;
    hc.bind = "127.0.0.1";
    hc.port = 0;
    hc.build = "test";
    hc.auto_accept = 1;
    hc.sim = m->host;
    hc.log = log_to;
    hc.user = &hl;
    NetSession *h = net_create(&hc, err, sizeof err);
    if (!h) {
        printf("host: %s\n", err);
        return 1;
    }
    snprintf(peer, sizeof peer, "127.0.0.1:%d", net_local_port(h));
    gc = hc;
    gc.role = NET_GUEST;
    gc.ports = 0x2;
    gc.peer = peer;
    gc.bind = "127.0.0.1";
    gc.sim = m->guest;
    gc.user = &gl;
    NetSession *g = net_create(&gc, err, sizeof err);
    if (!g) {
        printf("guest: %s\n", err);
        return 1;
    }
    net_start_thread(h);
    net_start_thread(g);
    Driver dh, dg;
    drv_init(&dh, h, 0, ticks, 0x1111, PERIOD, 1000 * MS);
    drv_init(&dg, g, 1, ticks, 0x2222, PERIOD, 1000 * MS);
    printf("threads: model %s (%s), %u ticks over 127.0.0.1 port %d, in real time\n", m->name, m->what, ticks,
           net_local_port(h));
    NetThread th, tg;
    uint64_t t0 = net_os_now_ns();
    net_thread_start(&th, peer_thread, &dh);
    net_thread_start(&tg, peer_thread, &dg);
    net_thread_join(th);
    net_thread_join(tg);
    double secs = (net_os_now_ns() - t0) / 1e9;
    net_os_sleep_ns(300 * MS);   /* the last hashes across */
    NetStats hs, gs;
    net_stats(h, &hs);
    net_stats(g, &gs);
    uint32_t diff = first_difference(&dh, &dg);
    CHECK(dh.n == ticks && dg.n == ticks, "ticks %u %u", dh.n, dg.n);
    CHECK(diff == NET_NONE, "hashes differ from %u", diff);
    CHECK(hs.end == NET_END_NONE && gs.end == NET_END_NONE, "ended %s %s", net_end_name(hs.end), net_end_name(gs.end));
    double rate = drv_rate(&dh, ticks / 3, ticks - 1);
    char th_[400], tg_[400];
    drv_trace_str(&dh, th_, sizeof th_);
    drv_trace_str(&dg, tg_, sizeof tg_);
    printf("  %.1f s; same hashes: %s; ticks a second after the first third: %.3f\n", secs,
           diff == NET_NONE ? "yes" : "NO", rate);
    printf("  delay each second: host %s; guest %s\n", th_, tg_);
    print_stats("host", h);
    print_stats("guest", g);
    if (m->rate_check)
        CHECK(rate > 59.9, "rate %.3f", rate);
    net_end(h, NET_END_LOCAL);
    uint64_t t1 = net_os_now_ns();
    while (net_state(g) != NET_ENDED && net_os_now_ns() - t1 < 2000 * MS)
        net_os_sleep_ns(MS);
    CHECK(net_end_reason(g, NULL, NULL, 0) == NET_END_PEER_LEFT, "the guest after BYE: %s",
          net_end_name(net_end_reason(g, NULL, NULL, 0)));
    printf("  BYE: the guest ended %.1f ms later\n", (net_os_now_ns() - t1) / 1e6);
    net_destroy(h);
    net_destroy(g);
    drv_free(&dh);
    drv_free(&dg);
    if (g_fail) {
        printf("host log:\n%s\nguest log:\n%s\n", hl.text, gl.text);
        printf("FAILED\n");
    } else {
        printf("ok\n");
    }
    return g_fail ? 1 : 0;
}

int main(int argc, char **argv)
{
#ifdef _WIN32
    setvbuf(stdout, NULL, _IONBF, 0);   /* msvcrt's _IOLBF is full buffering: net_check.py reads lines as they come */
    timeBeginPeriod(1);   /* the service threads' select() timeouts, as the game asks for (docs/port/WINDOWS.md 5) */
#else
    setvbuf(stdout, NULL, _IOLBF, 0);
#endif
    if (getenv("NET_TEST_VERBOSE"))
        g_verbose = 1;
    if (argc >= 2 && !strcmp(argv[1], "unit"))
        return cmd_unit();
    if (argc >= 3 && !strcmp(argv[1], "sim"))
        return cmd_sim(argv[2], argc >= 4 ? (uint32_t)atol(argv[3]) : 10000);
    if (argc >= 2 && !strcmp(argv[1], "fuzz"))
        return cmd_fuzz(argc >= 3 ? atol(argv[2]) : 200000);
    if (argc >= 3 && !strcmp(argv[1], "threads"))
        return cmd_threads(argv[2], argc >= 4 ? (uint32_t)atol(argv[3]) : 600);
    if (argc >= 2 && !strcmp(argv[1], "models")) {
        for (int i = 0; i < NMODELS; i++)
            printf("%-8s host '%s' guest '%s': %s\n", MODELS[i].name, MODELS[i].host, MODELS[i].guest, MODELS[i].what);
        return 0;
    }
    fprintf(stderr, "usage: net_test unit | sim MODEL [TICKS] | fuzz [N] | threads MODEL [TICKS] | models\n");
    return 2;
}
