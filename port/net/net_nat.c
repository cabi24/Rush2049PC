/* port/net/net_nat.c: the host's router opens its UDP port (net_nat.h, docs/port/NETWORK.md 13): PCP (RFC 6887),
 * NAT-PMP (RFC 6886) and UPnP IGD (net_upnp.c reads and writes its messages), on a thread of its own, with a lease
 * renewed at half its length and the mapping removed at the end. Written from the RFCs, the UPnP Device Architecture
 * 1.1 and the WANIPConnection service templates, and the POSIX and Windows Sockets documentation.
 *
 * The order: PCP and NAT-PMP first, one datagram each way to the default gateway's port 5351, no discovery, no TCP,
 * no XML: the cheapest. PCP before NAT-PMP: a NAT-PMP-only server answers PCP's request at once with its "unsupported
 * version" (RFC 6887 9), so asking PCP first costs nothing there, and nothing answering on 5351 (most routers that
 * speak only UPnP; an ICMP port unreachable ends it at once) means neither. Then UPnP: SSDP's multicast search on the
 * LAN (an answer within MX = 2 s), the device's description over HTTP, its control URL's SOAP actions.
 *
 * Only this PC's own network is spoken to: the default gateway, and the device that answered the search, whose every
 * URL must be on its own (private or link-local) address; loopback too in the tests' mode. */
#if defined(_WIN32) && (!defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601)
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include "net_nat.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_os.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef int socklen_t_;
static int sock_err(void) { return WSAGetLastError(); }
#define E_AGAIN WSAEWOULDBLOCK
#define E_INPROGRESS WSAEWOULDBLOCK
#define E_INTR WSAEINTR
#define E_REFUSED(e) ((e) == WSAECONNRESET || (e) == WSAECONNREFUSED)
#define SEND_FLAGS 0
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
typedef socklen_t socklen_t_;
static int sock_err(void) { return errno; }
#define E_AGAIN EAGAIN
#define E_INPROGRESS EINPROGRESS
#define E_INTR EINTR
#define E_REFUSED(e) ((e) == ECONNREFUSED || (e) == ECONNRESET)
#ifdef MSG_NOSIGNAL
#define SEND_FLAGS MSG_NOSIGNAL   /* a router that closes early: no SIGPIPE */
#else
#define SEND_FLAGS 0
#endif
#endif

#define MS 1000000ull
#define SLICE_MS 100              /* every wait looks at the stop this often */
#define HTTP_MAX (128 * 1024)     /* a description or an answer: past this, not trusted */
#define FOUND_MAX 4               /* IGDs tried */
#define SERVICES_MAX 4            /* WAN services tried, each */
#define RETRY_MAX_S 30            /* a renewal that failed: again after this at most */
#define UA "Rush2049PC UPnP/1.1 Rush2049PC/1"

static const char *const k_search[] = {
    "urn:schemas-upnp-org:device:InternetGatewayDevice:1",
    "urn:schemas-upnp-org:device:InternetGatewayDevice:2",
    "urn:schemas-upnp-org:service:WANIPConnection:1",
};

struct NetNat {
    NetNatConfig c;
    char desc[64];
    NetMutex m;
    NetCond cv;
    NetThread th;
    int stop, keep, done, removed;
    NetNatStatus st;
    /* the mapping: renewed and removed as it was made */
    int mapped;                  /* a request that may have opened it was sent */
    int proto;
    NetAddr gw;                  /* PCP, NAT-PMP: the server */
    uint8_t nonce[12];           /* PCP's */
    int ext_port;                /* the port asked for, then the one the router gave */
    uint32_t lease;              /* granted (0: permanent) */
    NetIgdService svc;           /* UPnP's */
    char client[20];             /* UPnP's NewInternalClient: this PC's address toward the IGD */
    char detail[128];            /* a refusal's words, kept while the next protocol is tried */
    NetNatWhy refusal;
};

/* ---------------------------------------------------------------- small things */

static void say(NetNat *n, const char *fmt, ...)
{
    char line[400];
    va_list ap;
    if (!n->c.log)
        return;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    n->c.log(n->c.user, line);
}

static int stopping(NetNat *n)
{
    net_mutex_lock(&n->m);
    int s = n->stop;
    net_mutex_unlock(&n->m);
    return s;
}

/* Waits up to ns or until stopped: 1 when stopped. */
static int nap(NetNat *n, uint64_t ns)
{
    uint64_t end = net_os_now_ns() + ns;
    int s;
    net_mutex_lock(&n->m);
    while (!(s = n->stop)) {
        uint64_t now = net_os_now_ns();
        if (now >= end)
            break;
        net_cond_wait_ns(&n->cv, &n->m, end - now);
    }
    net_mutex_unlock(&n->m);
    return s;
}

static void put16(uint8_t *p, unsigned v)
{
    p[0] = (uint8_t)(v >> 8), p[1] = (uint8_t)v;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24), p[1] = (uint8_t)(v >> 16), p[2] = (uint8_t)(v >> 8), p[3] = (uint8_t)v;
}

static unsigned get16(const uint8_t *p)
{
    return (unsigned)p[0] << 8 | p[1];
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static void ip4_str(const NetAddr *a, char *buf, size_t n)
{
    snprintf(buf, n, "%u.%u.%u.%u", a->addr[0], a->addr[1], a->addr[2], a->addr[3]);
}

const char *net_nat_proto_name(int proto)
{
    return proto == NET_NAT_PCP ? "PCP" : proto == NET_NAT_PMP ? "NAT-PMP" : proto == NET_NAT_UPNP ? "UPnP" : "none";
}

void net_nat_join_hint(const NetAddr *host, int port, char *out, size_t n)
{
    int k = net_addr_kind(host);
    if (!n)
        return;
    out[0] = 0;
    if (k == NET_KIND_PUBLIC && host->family == NET_AF_INET6)
        snprintf(out, n, "That is an internet (IPv6) address: unless both PCs are on Tailscale, the host's "
                 "firewall and router must let UDP port %d in to their PC. Tailscale needs none of that.", port);
    else if (k == NET_KIND_PUBLIC)
        snprintf(out, n, "That is an internet address: unless both PCs are on Tailscale, the host's router must "
                 "forward UDP port %d to their PC. The host turns on \"Open the port on my router (UPnP)\" (U on "
                 "their waiting screen, or F1 > Online > Host a race), or forwards the port by hand in the router's "
                 "settings. Tailscale needs neither.", port);
    else if (k == NET_KIND_TAILSCALE)
        snprintf(out, n, "That is a Tailscale address: are both PCs on Tailscale, signed in to the same tailnet (or "
                 "the host's PC shared with you)?");
}

/* ---------------------------------------------------------------- NAT-PMP and PCP's wire (RFC 6886, 6887) */

int net_pmp_external_req(uint8_t *buf, int cap)
{
    if (cap < 2)
        return -1;
    buf[0] = 0;   /* version 0 */
    buf[1] = 0;   /* opcode 0: the external address */
    return 2;
}

int net_pmp_map_req(uint8_t *buf, int cap, int internal_port, int external_port, uint32_t lifetime)
{
    if (cap < 12 || internal_port < 1 || internal_port > 65535 || external_port < 0 || external_port > 65535)
        return -1;
    buf[0] = 0;
    buf[1] = 1;   /* map UDP */
    put16(buf + 2, 0);
    put16(buf + 4, (unsigned)internal_port);
    put16(buf + 6, (unsigned)external_port);
    put32(buf + 8, lifetime);
    return 12;
}

int net_pmp_parse_external(const uint8_t *buf, int len, int *result, uint8_t ext[4])
{
    if (len < 8 || buf[0] != 0 || buf[1] != 128)
        return -1;
    *result = (int)get16(buf + 2);
    memset(ext, 0, 4);
    if (*result == 0) {
        if (len < 12)
            return -1;
        memcpy(ext, buf + 8, 4);
    }
    return 0;
}

int net_pmp_parse_map(const uint8_t *buf, int len, int *result, int *internal_port, int *external_port,
                      uint32_t *lifetime)
{
    if (len < 8 || buf[0] != 0 || buf[1] != 129)
        return -1;
    *result = (int)get16(buf + 2);
    *internal_port = *external_port = 0;
    *lifetime = 0;
    if (len >= 16) {
        *internal_port = (int)get16(buf + 8);
        *external_port = (int)get16(buf + 10);
        *lifetime = get32(buf + 12);
    } else if (*result == 0) {
        return -1;
    }
    return 0;
}

int net_pcp_map_req(uint8_t *buf, int cap, const NetAddr *client, const uint8_t nonce[12], int internal_port,
                    int external_port, uint32_t lifetime)
{
    if (cap < 60 || internal_port < 1 || internal_port > 65535 || external_port < 0 || external_port > 65535 ||
        (client->family != NET_AF_INET && client->family != NET_AF_INET6))
        return -1;
    memset(buf, 0, 60);
    buf[0] = 2;   /* version */
    buf[1] = 1;   /* a request (R 0), MAP */
    put32(buf + 4, lifetime);
    if (client->family == NET_AF_INET) {   /* ::ffff:a.b.c.d */
        buf[18] = buf[19] = 0xFF;
        memcpy(buf + 20, client->addr, 4);
    } else {
        memcpy(buf + 8, client->addr, 16);
    }
    memcpy(buf + 24, nonce, 12);
    buf[36] = 17;   /* UDP */
    put16(buf + 40, (unsigned)internal_port);
    put16(buf + 42, (unsigned)external_port);
    if (client->family == NET_AF_INET)   /* any IPv4 address outside: ::ffff:0.0.0.0 */
        buf[54] = buf[55] = 0xFF;
    return 60;
}

int net_pcp_parse(const uint8_t *buf, int len, const uint8_t nonce[12], NetPcpAnswer *a)
{
    static const uint8_t mapped[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF};
    memset(a, 0, sizeof *a);
    if (len < 4)
        return -1;
    if (buf[0] == 0) {   /* a NAT-PMP server: its "unsupported version" (RFC 6887 9) */
        if (!(buf[1] & 0x80))
            return -1;
        a->version = 0;
        a->result = (int)get16(buf + 2);
        return 0;
    }
    if (buf[0] != 2 || buf[1] != (0x80 | 1) || len < 60 || len > 1100 || (len & 3))
        return -1;
    if (memcmp(buf + 24, nonce, 12) || buf[36] != 17)
        return -1;   /* not the answer to this request */
    a->version = 2;
    a->result = buf[3];
    a->lifetime = get32(buf + 4);
    a->epoch = get32(buf + 8);
    a->internal_port = (int)get16(buf + 40);
    a->external_port = (int)get16(buf + 42);
    if (!memcmp(buf + 44, mapped, 12)) {
        a->external.family = NET_AF_INET;
        memcpy(a->external.addr, buf + 56, 4);
    } else {
        a->external.family = NET_AF_INET6;
        memcpy(a->external.addr, buf + 44, 16);
    }
    a->external.port = (uint16_t)a->external_port;
    return 0;
}

const char *net_pcp_result_name(int r)
{
    static const char *const names[] = {"SUCCESS", "UNSUPP_VERSION", "NOT_AUTHORIZED", "MALFORMED_REQUEST",
                                        "UNSUPP_OPCODE", "UNSUPP_OPTION", "MALFORMED_OPTION", "NETWORK_FAILURE",
                                        "NO_RESOURCES", "UNSUPP_PROTOCOL", "USER_EX_QUOTA", "CANNOT_PROVIDE_EXTERNAL",
                                        "ADDRESS_MISMATCH", "EXCESSIVE_REMOTE_PEERS"};
    return r >= 0 && r < (int)(sizeof names / sizeof *names) ? names[r] : "unknown result";
}

const char *net_pmp_result_name(int r)
{
    static const char *const names[] = {"success", "unsupported version", "not authorized or refused",
                                        "network failure", "out of resources", "unsupported opcode"};
    return r >= 0 && r < (int)(sizeof names / sizeof *names) ? names[r] : "unknown result";
}

/* ---------------------------------------------------------------- addresses */

/* "A.B.C.D:PORT" (an IPv4 literal: the tests' hooks) */
static int hostport4(const char *s, NetAddr *a)
{
    char url[96];
    NetUrl u;
    if (strlen(s) > 64 || !strchr(s, ':'))
        return -1;
    snprintf(url, sizeof url, "http://%s/", s);
    if (net_url_parse(url, &u))
        return -1;
    *a = u.addr;
    return 0;
}

/* This PC's own network: a private (RFC 1918) or link-local IPv4 address; loopback too in the tests' mode. */
static int on_lan(const NetNat *n, const NetAddr *a)
{
    int k = net_addr_kind(a);
    return a->family == NET_AF_INET && (k == NET_KIND_LAN || k == NET_KIND_LINK_LOCAL ||
                                        (n->c.test && k == NET_KIND_LOOPBACK));
}

static void close_fd(NetSockFd fd)
{
#ifdef _WIN32
    closesocket((SOCKET)fd);
#else
    close(fd);
#endif
}

static int nonblocking(NetSockFd fd)
{
#ifdef _WIN32
    u_long one = 1;
    return ioctlsocket((SOCKET)fd, FIONBIO, &one) == 0 ? 0 : -1;
#else
    int fl = fcntl(fd, F_GETFL, 0);
    return fl >= 0 && fcntl(fd, F_SETFL, fl | O_NONBLOCK) == 0 ? 0 : -1;
#endif
}

static void sin_of(const NetAddr *a, struct sockaddr_in *in)
{
    memset(in, 0, sizeof *in);
    in->sin_family = AF_INET;
    in->sin_port = htons(a->port);
    memcpy(&in->sin_addr, a->addr, 4);
}

/* A UDP socket connected to to (only its datagrams arrive; its ICMP errors are told), and this PC's address it
 * leaves from. NET_SOCK_INVALID when none. */
static NetSockFd udp_to(const NetAddr *to, NetAddr *self)
{
    struct sockaddr_in in, me;
    socklen_t_ ml = sizeof me;
    NetSockFd fd = (NetSockFd)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == NET_SOCK_INVALID)
        return fd;
    sin_of(to, &in);
    if (connect(fd, (struct sockaddr *)&in, sizeof in) != 0 || nonblocking(fd) != 0 ||
        getsockname(fd, (struct sockaddr *)&me, &ml) != 0) {
        close_fd(fd);
        return NET_SOCK_INVALID;
    }
    if (self) {
        memset(self, 0, sizeof *self);
        self->family = NET_AF_INET;
        memcpy(self->addr, &me.sin_addr, 4);
        self->port = ntohs(me.sin_port);
    }
#ifdef _WIN32
    {   /* an ICMP port unreachable: WSAECONNRESET from the next recv, which is what tells "no server" here */
        BOOL yes = TRUE;
        DWORD ret = 0;
        WSAIoctl((SOCKET)fd, _WSAIOW(IOC_VENDOR, 12), &yes, sizeof yes, NULL, 0, &ret, NULL, NULL);
    }
#endif
    return fd;
}

/* Waits until fd is readable (or writable: write), up to ms: 1 ready, 0 not yet, -1 an error (writable's: the
 * connect failed). */
static int wait_fd(NetSockFd fd, int write, int ms)
{
#ifdef _WIN32
    fd_set rf, wf, ef;
    struct timeval tv = {ms / 1000, (ms % 1000) * 1000};
    FD_ZERO(&rf), FD_ZERO(&wf), FD_ZERO(&ef);
    FD_SET((SOCKET)fd, write ? &wf : &rf);
    FD_SET((SOCKET)fd, &ef);
    int r = select(0, &rf, &wf, &ef, &tv);
    if (r > 0 && FD_ISSET((SOCKET)fd, &ef))
        return -1;
    return r > 0 ? 1 : r == 0 ? 0 : -1;
#else
    struct pollfd p = {fd, (short)(write ? POLLOUT : POLLIN), 0};
    int r = poll(&p, 1, ms);
    if (r < 0 && errno == EINTR)
        return 0;
    return r > 0 ? 1 : r == 0 ? 0 : -1;
#endif
}

/* The default gateway's IPv4 address (port 0): 0, or -1 (none, or not known here). Only the routing table is read. */
#ifdef _WIN32
typedef DWORD(WINAPI *GetBestRouteFn)(DWORD, DWORD, void *);
#endif
static int default_gateway(NetAddr *gw)
{
    memset(gw, 0, sizeof *gw);
#if defined(__linux__)
    char line[256];
    unsigned long best_metric = ~0ul;
    int found = -1;
    FILE *f = fopen("/proc/net/route", "r");
    if (!f)
        return -1;
    if (!fgets(line, sizeof line, f)) {   /* its header */
        fclose(f);
        return -1;
    }
    for (int k = 0; k < 4096 && fgets(line, sizeof line, f); k++) {
        char ifname[64];
        unsigned long dest, via, flags, metric, mask;
        if (sscanf(line, "%63s %lx %lx %lx %*d %*d %lu %lx", ifname, &dest, &via, &flags, &metric, &mask) != 6)
            continue;
        if (dest || mask || (flags & 3) != 3 || !via || metric >= best_metric)
            continue;   /* not a default route through a gateway (RTF_UP | RTF_GATEWAY) */
        uint32_t v = (uint32_t)via;   /* printed as the kernel's own (network-order) word */
        memcpy(gw->addr, &v, 4);
        gw->family = NET_AF_INET;
        best_metric = metric;
        found = 0;
    }
    fclose(f);
    return found;
#elif defined(_WIN32)
    /* GetBestRoute (iphlpapi.dll, loaded at run time: the program imports only what it did) for a documentation
     * address (192.0.2.1, TEST-NET-1): the routing table's answer for the internet; nothing is sent. */
    static GetBestRouteFn fn;
    static int tried;
    DWORD row[14];
    if (!tried) {
        HMODULE h = LoadLibraryA("iphlpapi.dll");
        tried = 1;
        fn = h ? (GetBestRouteFn)(void (*)(void))GetProcAddress(h, "GetBestRoute") : NULL;
    }
    if (!fn)
        return -1;
    memset(row, 0, sizeof row);
    if (fn(htonl(0xC0000201u), 0, row) != 0 || !row[3] || row[3] == htonl(0xC0000201u))
        return -1;   /* row[3]: dwForwardNextHop; none or the address itself: no gateway */
    memcpy(gw->addr, &row[3], 4);
    gw->family = NET_AF_INET;
    return 0;
#else
    return -1;
#endif
}

/* ---------------------------------------------------------------- PCP and NAT-PMP */

/* Sends req on fd at 0, 250 and 750 ms, until want takes an answer or wait_ms passes. The answer's length; 0 none;
 * -1 refused (an ICMP port unreachable: nothing listens); -2 stopped (cancel). */
static int udp_ask(NetNat *n, NetSockFd fd, const uint8_t *req, int reqlen, uint8_t *resp, int cap, int wait_ms,
                   int (*want)(const uint8_t *, int, void *), void *arg, int cancel)
{
    static const int at[] = {0, 250, 750};
    uint64_t t0 = net_os_now_ns();
    int sent = 0;
    for (;;) {
        uint64_t now = net_os_now_ns();
        int64_t el = (int64_t)((now - t0) / MS);
        if (el >= wait_ms)
            return 0;
        if (cancel && stopping(n))
            return -2;
        if (sent < 3 && el >= at[sent]) {
            if (send(fd, (const char *)req, reqlen, SEND_FLAGS) < 0 && E_REFUSED(sock_err()))
                return -1;
            sent++;
        }
        int64_t until = sent < 3 ? at[sent] : wait_ms;
        int w = (int)(until - el);
        if (w > SLICE_MS)
            w = SLICE_MS;
        if (wait_fd(fd, 0, w < 1 ? 1 : w) != 1)
            continue;
        for (int k = 0; k < 16; k++) {
            int r = (int)recv(fd, (char *)resp, cap, 0);
            if (r < 0) {
                int e = sock_err();
                if (E_REFUSED(e))
                    return -1;
                break;   /* nothing more waiting */
            }
            if (r > 0 && want(resp, r, arg))
                return r;
        }
    }
}

static int want_pcp(const uint8_t *b, int len, void *arg)
{
    NetPcpAnswer a;
    return net_pcp_parse(b, len, (const uint8_t *)arg, &a) == 0;
}

static int want_pmp_ext(const uint8_t *b, int len, void *arg)
{
    int r;
    uint8_t e[4];
    (void)arg;
    return net_pmp_parse_external(b, len, &r, e) == 0;
}

static int want_pmp_map(const uint8_t *b, int len, void *arg)
{
    int r, ip, ep;
    uint32_t life;
    if (net_pmp_parse_map(b, len, &r, &ip, &ep, &life))
        return 0;
    return r != 0 || ip == *(const int *)arg;   /* a success is for this PC's port */
}

/* Where PCP and NAT-PMP go: 0, or -1 (off, none, or not on this PC's network). */
static int pmp_server(NetNat *n, NetAddr *gw)
{
    if (!strcmp(n->c.gateway, "off"))
        return -1;
    if (n->c.gateway[0]) {
        if (hostport4(n->c.gateway, gw)) {
            say(n, "router: RUSH_NAT_GATEWAY '%s' isn't A.B.C.D:PORT: PCP and NAT-PMP not tried", n->c.gateway);
            return -1;
        }
    } else if (default_gateway(gw) == 0) {
        gw->port = NET_NAT_PMP_PORT;
    } else {
        say(n, "router: no default gateway found: PCP and NAT-PMP not tried");
        return -1;
    }
    if (!on_lan(n, gw)) {
        char a[32];
        ip4_str(gw, a, sizeof a);
        say(n, "router: the default gateway %s isn't on a private network: PCP and NAT-PMP not tried", a);
        return -1;
    }
    return 0;
}

/* One PCP or NAT-PMP request for n's mapping with lifetime (0: remove). 1 done (the port and lease the router gave
 * in n), 0 no answer, -1 refused (n->detail), -2 stopped. */
static int pmp_once(NetNat *n, uint32_t lifetime, int cancel, NetAddr *ext)
{
    uint8_t req[64], resp[1100];
    NetAddr self;
    int len, r, wait = n->c.wait_ms;
    NetSockFd fd = udp_to(&n->gw, &self);
    if (fd == NET_SOCK_INVALID) {
        snprintf(n->detail, sizeof n->detail, "no socket to the gateway");
        return 0;
    }
    if (n->proto == NET_NAT_PCP) {
        NetPcpAnswer a;
        len = net_pcp_map_req(req, sizeof req, &self, n->nonce, n->c.internal_port, n->ext_port, lifetime);
        r = udp_ask(n, fd, req, len, resp, sizeof resp, wait, want_pcp, n->nonce, cancel);
        close_fd(fd);
        if (r <= 0)
            return r == -2 ? -2 : 0;
        net_pcp_parse(resp, r, n->nonce, &a);
        if (a.version == 0)
            return 0;   /* only the first request learns this */
        if (a.result) {
            snprintf(n->detail, sizeof n->detail, "PCP %s (%d)", net_pcp_result_name(a.result), a.result);
            n->refusal = a.result == 7 ? NET_NAT_NO_INTERNET : NET_NAT_REFUSED;
            return -1;
        }
        if (lifetime) {
            n->ext_port = a.external_port;
            n->lease = a.lifetime;
            if (ext)
                *ext = a.external;
        }
        return 1;
    }
    {   /* NAT-PMP */
        int res, ip, ep, port = n->c.internal_port;
        uint32_t life;
        len = net_pmp_map_req(req, sizeof req, n->c.internal_port, lifetime ? n->ext_port : 0, lifetime);
        r = udp_ask(n, fd, req, len, resp, sizeof resp, wait, want_pmp_map, &port, cancel);
        close_fd(fd);
        if (r <= 0)
            return r == -2 ? -2 : 0;
        net_pmp_parse_map(resp, r, &res, &ip, &ep, &life);
        if (res) {
            snprintf(n->detail, sizeof n->detail, "NAT-PMP %s (%d)", net_pmp_result_name(res), res);
            n->refusal = res == 3 ? NET_NAT_NO_INTERNET : NET_NAT_REFUSED;
            return -1;
        }
        if (lifetime) {
            n->ext_port = ep;
            n->lease = life;
        }
        return 1;
    }
}

/* PCP, or NAT-PMP when the server speaks only it. 1 mapped (ext: the address outside, when it said), 0 nothing
 * answered, -1 refused, -2 stopped. */
static int try_pmp(NetNat *n, NetAddr *ext)
{
    uint8_t req[64], resp[1100];
    NetAddr self;
    char gw[32];
    int len, r;
    NetPcpAnswer a;
    if (pmp_server(n, &n->gw))
        return 0;
    ip4_str(&n->gw, gw, sizeof gw);
    NetSockFd fd = udp_to(&n->gw, &self);
    if (fd == NET_SOCK_INVALID)
        return 0;
    net_os_random(n->nonce, sizeof n->nonce);
    n->proto = NET_NAT_PCP;
    n->ext_port = n->c.external_port;
    len = net_pcp_map_req(req, sizeof req, &self, n->nonce, n->c.internal_port, n->ext_port, (uint32_t)n->c.lease_s);
    n->mapped = 1;   /* from here a removal is sent at the end, whatever came back */
    r = udp_ask(n, fd, req, len, resp, sizeof resp, n->c.wait_ms, want_pcp, n->nonce, 1);
    if (r == -2) {
        close_fd(fd);
        return -2;
    }
    if (r <= 0) {
        close_fd(fd);
        n->mapped = 0;
        say(n, "router: PCP and NAT-PMP: %s at %s:%d", r < 0 ? "nothing listens" : "no answer", gw, n->gw.port);
        return 0;
    }
    net_pcp_parse(resp, r, n->nonce, &a);
    if (a.version == 2) {
        close_fd(fd);
        if (a.result) {
            n->mapped = 0;
            snprintf(n->detail, sizeof n->detail, "PCP %s (%d)", net_pcp_result_name(a.result), a.result);
            n->refusal = a.result == 7 ? NET_NAT_NO_INTERNET : NET_NAT_REFUSED;
            say(n, "router: PCP at %s refused the mapping: %s", gw, n->detail);
            return -1;
        }
        n->ext_port = a.external_port;
        n->lease = a.lifetime;
        *ext = a.external;
        return 1;
    }
    /* a NAT-PMP server (its "unsupported version"): its external address, then the mapping */
    n->mapped = 0;
    say(n, "router: %s speaks NAT-PMP, not PCP", gw);
    n->proto = NET_NAT_PMP;
    len = net_pmp_external_req(req, sizeof req);
    r = udp_ask(n, fd, req, len, resp, sizeof resp, n->c.wait_ms, want_pmp_ext, NULL, 1);
    close_fd(fd);
    if (r == -2)
        return -2;
    if (r > 0) {
        int res = -1;
        uint8_t e[4];
        net_pmp_parse_external(resp, r, &res, e);
        if (res == 0) {
            ext->family = NET_AF_INET;
            memcpy(ext->addr, e, 4);
        } else if (res == 3) {   /* network failure: no address from its provider yet */
            snprintf(n->detail, sizeof n->detail, "NAT-PMP %s (%d)", net_pmp_result_name(res), res);
            n->refusal = NET_NAT_NO_INTERNET;
            say(n, "router: NAT-PMP at %s has no internet address: %s", gw, n->detail);
            return -1;
        }
    }
    n->mapped = 1;
    r = pmp_once(n, (uint32_t)n->c.lease_s, 1, NULL);
    if (r != 1)
        n->mapped = r == -2;
    if (r == -1)
        say(n, "router: NAT-PMP at %s refused the mapping: %s", gw, n->detail);
    return r;
}

/* ---------------------------------------------------------------- UPnP: SSDP, HTTP, SOAP */

typedef struct {
    NetUrl location;
} Found;

/* SSDP's search: the IGDs that answered on this PC's network, at most max. The count; -2 stopped. */
static int ssdp_find(NetNat *n, Found *out, int max)
{
    NetAddr to;
    char host[64], msg[512], where[64];
    uint8_t buf[2048];
    int multicast = !n->c.ssdp[0], count = 0, sends = 0;
    uint64_t t0 = net_os_now_ns(), first = 0;
    if (multicast) {
        memset(&to, 0, sizeof to);
        to.family = NET_AF_INET;
        to.addr[0] = 239, to.addr[1] = 255, to.addr[2] = 255, to.addr[3] = 250;
        to.port = 1900;
        snprintf(host, sizeof host, "239.255.255.250:1900");
    } else if (hostport4(n->c.ssdp, &to)) {
        say(n, "router: RUSH_NAT_SSDP '%s' isn't A.B.C.D:PORT: UPnP not tried", n->c.ssdp);
        return 0;
    } else {
        snprintf(host, sizeof host, "%s", n->c.ssdp);
    }
    NetSockFd fd = (NetSockFd)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == NET_SOCK_INVALID)
        return 0;
    {
        struct sockaddr_in any;
        memset(&any, 0, sizeof any);
        any.sin_family = AF_INET;
        if (bind(fd, (struct sockaddr *)&any, sizeof any) != 0 || nonblocking(fd) != 0) {
            close_fd(fd);
            return 0;
        }
    }
    if (multicast) {
        int ttl = 2;   /* the LAN only (UDA 1.1 says 2 by default) */
        NetAddr gw, self;
        setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (const char *)&ttl, sizeof ttl);
        if (default_gateway(&gw) == 0) {   /* out of the interface toward the gateway, not a VPN's or a VM's */
            gw.port = 9;
            NetSockFd probe = udp_to(&gw, &self);
            if (probe != NET_SOCK_INVALID) {
                struct in_addr ifa;
                memcpy(&ifa, self.addr, 4);
                setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&ifa, sizeof ifa);
                close_fd(probe);
            }
        }
    }
    struct sockaddr_in dst;
    sin_of(&to, &dst);
    for (;;) {
        uint64_t now = net_os_now_ns();
        int el = (int)((now - t0) / MS);
        if (el >= n->c.search_ms || count == max || (first && now - first >= 300 * MS))
            break;
        if (stopping(n)) {
            close_fd(fd);
            return -2;
        }
        if (sends < 2 && el >= sends * 1000) {   /* the search, twice (UDP may lose it) */
            for (size_t i = 0; i < sizeof k_search / sizeof *k_search; i++) {
                int len = net_ssdp_search(msg, sizeof msg, host, k_search[i], 2);
                sendto(fd, msg, len, 0, (struct sockaddr *)&dst, sizeof dst);
            }
            sends++;
        }
        if (wait_fd(fd, 0, SLICE_MS) != 1)
            continue;
        for (int k = 0; k < 16 && count < max; k++) {
            struct sockaddr_in from;
            socklen_t_ fl = sizeof from;
            char loc[300], st[160];
            NetAddr src;
            NetUrl u;
            int r = (int)recvfrom(fd, (char *)buf, sizeof buf, 0, (struct sockaddr *)&from, &fl);
            if (r < 0)
                break;
            if (r == 0 || r >= (int)sizeof buf || from.sin_family != AF_INET)
                continue;
            memset(&src, 0, sizeof src);
            src.family = NET_AF_INET;
            memcpy(src.addr, &from.sin_addr, 4);
            ip4_str(&src, where, sizeof where);
            if (!on_lan(n, &src))
                continue;   /* not this PC's network */
            if (net_ssdp_parse((const char *)buf, r, loc, sizeof loc, st, sizeof st) || net_url_parse(loc, &u)) {
                say(n, "router: UPnP: an answer from %s that isn't one (or its LOCATION isn't http://A.B.C.D/)", where);
                continue;
            }
            if (memcmp(u.addr.addr, src.addr, 4)) {
                say(n, "router: UPnP: %s answered with a description on another address (%s): ignored", where, loc);
                continue;
            }
            int dup = 0;
            for (int i = 0; i < count; i++)
                dup |= !memcmp(&out[i].location.addr, &u.addr, sizeof u.addr) && !strcmp(out[i].location.path, u.path);
            if (dup)
                continue;
            out[count++].location = u;
            if (!first)
                first = net_os_now_ns();
            say(n, "router: UPnP: %s answered (%s): %s", where, st, loc);
        }
    }
    close_fd(fd);
    return count;
}

/* One HTTP request to u (head: the request line and headers, then body) and its answer into out (cap, one byte kept
 * for net_http_parse's NUL). The answer's length, or -1 (err), -2 stopped. */
static int http_ask(NetNat *n, const NetUrl *u, const char *head, const char *body, size_t body_len, char *out,
                    size_t cap, int cancel, char *err, size_t err_n)
{
    struct sockaddr_in in;
    uint64_t t0 = net_os_now_ns();
    size_t got = 0, hl = strlen(head), sent = 0;
    int r = -1, e;
    NetSockFd fd = (NetSockFd)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == NET_SOCK_INVALID || nonblocking(fd) != 0) {
        snprintf(err, err_n, "no socket");
        if (fd != NET_SOCK_INVALID)
            close_fd(fd);
        return -1;
    }
#define LEFT_MS() ((int)n->c.http_ms - (int)((net_os_now_ns() - t0) / MS))
    sin_of(&u->addr, &in);
    if (connect(fd, (struct sockaddr *)&in, sizeof in) != 0 && (e = sock_err()) != E_INPROGRESS && e != E_AGAIN) {
        snprintf(err, err_n, "connect: error %d", e);
        goto done;
    }
    for (;;) {   /* connected */
        if (LEFT_MS() <= 0) {
            snprintf(err, err_n, "no connection in %d ms", n->c.http_ms);
            goto done;
        }
        if (cancel && stopping(n)) {
            r = -2;
            goto done;
        }
        int w = wait_fd(fd, 1, LEFT_MS() < SLICE_MS ? LEFT_MS() : SLICE_MS);
        if (w == 0)
            continue;
        int soerr = 0;
        socklen_t_ sl = sizeof soerr;
        getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&soerr, &sl);
        if (w < 0 || soerr) {
            snprintf(err, err_n, "connection refused or failed (error %d)", soerr);
            goto done;
        }
        break;
    }
    while (sent < hl + body_len) {   /* the request */
        const char *p = sent < hl ? head + sent : body + (sent - hl);
        size_t k = sent < hl ? hl - sent : body_len - (sent - hl);
        int s = (int)send(fd, p, (int)k, SEND_FLAGS);
        if (s > 0) {
            sent += (size_t)s;
            continue;
        }
        if (s < 0 && ((e = sock_err()) == E_AGAIN || e == E_INTR) && LEFT_MS() > 0 && !(cancel && stopping(n))) {
            wait_fd(fd, 1, SLICE_MS);
            continue;
        }
        snprintf(err, err_n, "sending: error %d", s < 0 ? sock_err() : 0);
        goto done;
    }
    for (;;) {   /* the answer, to the connection's end or its Content-Length */
        if (LEFT_MS() <= 0) {
            snprintf(err, err_n, "no whole answer in %d ms", n->c.http_ms);
            goto done;
        }
        if (cancel && stopping(n)) {
            r = -2;
            goto done;
        }
        if (wait_fd(fd, 0, LEFT_MS() < SLICE_MS ? LEFT_MS() : SLICE_MS) != 1)
            continue;
        if (got + 1 >= cap) {
            snprintf(err, err_n, "an answer longer than %u bytes", (unsigned)cap);
            goto done;
        }
        int k = (int)recv(fd, out + got, (int)(cap - 1 - got), 0);
        if (k == 0)
            break;
        if (k < 0) {
            if ((e = sock_err()) == E_AGAIN || e == E_INTR)
                continue;
            snprintf(err, err_n, "receiving: error %d", e);
            goto done;
        }
        got += (size_t)k;
        if (net_http_complete(out, got))
            break;   /* its Content-Length's body, or its last chunk, is here: a server may keep the connection */
    }
    r = (int)got;
done:
#undef LEFT_MS
    close_fd(fd);
    return r;
}

/* A SOAP action on svc. 0 done (body: its answer's), a UPnP error's code (desc), -1 (err), -2 stopped. */
static int soap(NetNat *n, const NetIgdService *svc, const char *action, const char *const *names,
                const char *const *values, int k, char *resp, size_t cap, char **body, size_t *body_len, char *err,
                size_t err_n, int cancel)
{
    char req[2048], head[1024], a[32];
    int len = net_soap_body(req, sizeof req, svc->type, action, names, values, k), status, r;
    if (len < 0) {
        snprintf(err, err_n, "the request doesn't fit");
        return -1;
    }
    ip4_str(&svc->control.addr, a, sizeof a);
    snprintf(head, sizeof head,
             "POST %s HTTP/1.1\r\nHost: %s:%u\r\nUser-Agent: " UA "\r\nContent-Type: text/xml; charset=\"utf-8\"\r\n"
             "SOAPAction: \"%s#%s\"\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
             svc->control.path, a, svc->control.addr.port, svc->type, action, len);
    r = http_ask(n, &svc->control, head, req, (size_t)len, resp, cap, cancel, err, err_n);
    if (r < 0)
        return r;
    if (net_http_parse(resp, (size_t)r, &status, body, body_len)) {
        snprintf(err, err_n, "an answer that isn't HTTP");
        return -1;
    }
    if (status == 200)
        return 0;
    int code;
    char desc[96];
    if (net_soap_fault(*body, *body_len, &code, desc, sizeof desc) == 0 && code > 0) {
        snprintf(err, err_n, "UPnP error %d%s%s%s", code, desc[0] ? " (" : "", desc, desc[0] ? ")" : "");
        return code;
    }
    snprintf(err, err_n, "HTTP %d", status);
    return -1;
}

static int add_mapping(NetNat *n, uint32_t lease, int cancel, char *err, size_t err_n)
{
    char ext[8], in[8], le[16], resp[8192], *body;
    size_t bl;
    const char *names[] = {"NewRemoteHost", "NewExternalPort", "NewProtocol", "NewInternalPort", "NewInternalClient",
                           "NewEnabled", "NewPortMappingDescription", "NewLeaseDuration"};
    snprintf(ext, sizeof ext, "%d", n->ext_port);
    snprintf(in, sizeof in, "%d", n->c.internal_port);
    snprintf(le, sizeof le, "%u", lease);
    const char *values[] = {"", ext, "UDP", in, n->client, "1", n->desc, le};
    return soap(n, &n->svc, "AddPortMapping", names, values, 8, resp, sizeof resp, &body, &bl, err, err_n, cancel);
}

static int delete_mapping(NetNat *n, char *err, size_t err_n)
{
    char ext[8], resp[8192], *body;
    size_t bl;
    const char *names[] = {"NewRemoteHost", "NewExternalPort", "NewProtocol"};
    snprintf(ext, sizeof ext, "%d", n->ext_port);
    const char *values[] = {"", ext, "UDP"};
    return soap(n, &n->svc, "DeletePortMapping", names, values, 3, resp, sizeof resp, &body, &bl, err, err_n, 0);
}

/* UPnP IGD: the search, the description, GetExternalIPAddress on each WAN service until one answers, then
 * AddPortMapping. 1 mapped, 0 no IGD, -1 refused or failed (n->detail, n->refusal), -2 stopped. */
static int try_upnp(NetNat *n, NetAddr *ext)
{
    Found f[FOUND_MAX];
    char *buf = NULL, err[160], a[32];
    int found, chosen = 0, refused = 0, r;
    if (!strcmp(n->c.ssdp, "off"))
        return 0;
    found = ssdp_find(n, f, FOUND_MAX);
    if (found <= 0) {
        if (found == 0)
            say(n, "router: UPnP: no answer to the search in %.1f s", n->c.search_ms / 1000.0);
        return found;
    }
    buf = malloc(HTTP_MAX);
    if (!buf)
        return 0;
    for (int i = 0; i < found && !chosen; i++) {
        char head[512], name[64];
        NetIgdService svc[SERVICES_MAX];
        char *body;
        size_t bl;
        int status, k;
        ip4_str(&f[i].location.addr, a, sizeof a);
        snprintf(head, sizeof head, "GET %s HTTP/1.1\r\nHost: %s:%u\r\nUser-Agent: " UA "\r\nConnection: close\r\n\r\n",
                 f[i].location.path, a, f[i].location.addr.port);
        r = http_ask(n, &f[i].location, head, "", 0, buf, HTTP_MAX, 1, err, sizeof err);
        if (r == -2)
            goto stopped;
        if (r < 0 || net_http_parse(buf, (size_t)r, &status, &body, &bl) || status != 200) {
            say(n, "router: UPnP: the description at %s:%u%s: %s", a, f[i].location.addr.port, f[i].location.path,
                r < 0 ? err : "not an HTTP 200 answer");
            continue;
        }
        k = net_igd_parse(body, bl, &f[i].location, svc, SERVICES_MAX, name, sizeof name);
        if (!k) {
            say(n, "router: UPnP: %s (%s) has no WANIPConnection or WANPPPConnection service", a,
                name[0] ? name : "no name");
            continue;
        }
        for (int j = 0; j < k && !chosen; j++) {
            char *b2;
            size_t bl2;
            size_t from = 0;
            char ip[64];
            r = soap(n, &svc[j], "GetExternalIPAddress", NULL, NULL, 0, buf, HTTP_MAX, &b2, &bl2, err, sizeof err, 1);
            if (r == -2)
                goto stopped;
            if (r != 0) {
                say(n, "router: UPnP: %s's %s: GetExternalIPAddress: %s", a, svc[j].type + 29, err);
                snprintf(n->detail, sizeof n->detail, "%.127s", err);
                n->refusal = NET_NAT_REFUSED;
                refused = 1;
                continue;
            }
            n->svc = svc[j];
            net_mutex_lock(&n->m);
            snprintf(n->st.name, sizeof n->st.name, "%s", name);
            net_mutex_unlock(&n->m);
            chosen = 1;
            memset(ext, 0, sizeof *ext);
            if (net_xml_text(b2, bl2, "NewExternalIPAddress", &from, ip, sizeof ip) == 0) {
                NetUrl u;
                char url[96];
                snprintf(url, sizeof url, "http://%s/", ip);
                if (ip[0] && net_url_parse(url, &u) == 0) {
                    *ext = u.addr;
                    ext->port = 0;
                } else if (ip[0]) {
                    say(n, "router: UPnP: its external address '%s' isn't an IPv4 one", ip);
                }
            }
            say(n, "router: UPnP: using %s%s%s at %s:%u%s (%s), its external address %s", name[0] ? "'" : "", name,
                name[0] ? "' " : "", a, svc[j].control.addr.port, svc[j].control.path, svc[j].type + 29,
                ext->family ? ip : "not said");
        }
    }
    if (!chosen) {   /* refused by a WAN service; or no IGD that can open a port (none, or none usable) */
        free(buf);
        return refused ? -1 : 0;
    }
    {   /* this PC's address toward the IGD: the mapping's internal client */
        NetAddr self;
        NetSockFd fd = udp_to(&n->svc.control.addr, &self);
        if (fd == NET_SOCK_INVALID) {
            free(buf);
            snprintf(n->detail, sizeof n->detail, "no route to the router");
            return -1;
        }
        close_fd(fd);
        ip4_str(&self, n->client, sizeof n->client);
    }
    n->proto = NET_NAT_UPNP;
    n->ext_port = n->c.external_port;
    n->lease = (uint32_t)n->c.lease_s;
    n->mapped = 1;
    r = add_mapping(n, n->lease, 1, err, sizeof err);
    if (r == 725) {   /* OnlyPermanentLeasesSupported: an old IGD; removed at the end all the same */
        say(n, "router: UPnP: the router takes only permanent mappings (725): asking for one, removed at the end");
        n->lease = 0;
        r = add_mapping(n, 0, 1, err, sizeof err);
    }
    free(buf);
    if (r == 0)
        return 1;
    if (r == -2)
        return -2;
    n->mapped = r == -1;   /* no answer to it (not a refusal): the router may have made it; removed at the end */
    snprintf(n->detail, sizeof n->detail, "%.127s", err);
    n->refusal = r == 718 ? NET_NAT_CONFLICT : NET_NAT_REFUSED;
    say(n, "router: UPnP: AddPortMapping of port %d to %s:%d: %s", n->ext_port, n->client, n->c.internal_port, err);
    return -1;
stopped:
    free(buf);
    return -2;
}

/* ---------------------------------------------------------------- the thread */

static void set_state(NetNat *n, NetNatState s, NetNatWhy why)
{
    net_mutex_lock(&n->m);
    n->st.state = s;
    n->st.why = why;
    n->st.proto = n->proto;
    n->st.lease_s = n->lease;
    snprintf(n->st.detail, sizeof n->st.detail, "%s", n->detail);
    net_mutex_unlock(&n->m);
}

/* Renews n's mapping as it was made: 1 done, 0 or -1 not (n->detail), -2 stopped. */
static int renew(NetNat *n)
{
    char err[160];
    if (n->proto == NET_NAT_UPNP) {
        int r = add_mapping(n, n->lease, 1, err, sizeof err);
        if (r)
            snprintf(n->detail, sizeof n->detail, "%.127s", err);
        return r == 0 ? 1 : r == -2 ? -2 : -1;
    }
    int port = n->ext_port, r = pmp_once(n, (uint32_t)n->c.lease_s, 1, NULL);
    if (r == 0)
        snprintf(n->detail, sizeof n->detail, "no answer from the router");
    if (r == 1 && n->ext_port != port)
        say(n, "router: %s moved the port outside from %d to %d", net_nat_proto_name(n->proto), port, n->ext_port);
    return r;
}

static int remove_mapping(NetNat *n)
{
    char err[160];
    int r;
    if (n->proto == NET_NAT_UPNP) {
        r = delete_mapping(n, err, sizeof err);
        if (r == 714)   /* NoSuchEntryInArray: it is gone already */
            r = 0;
        if (r)
            say(n, "router: UPnP: DeletePortMapping of port %d: %s (the lease ends it)", n->ext_port, err);
        return r == 0;
    }
    r = pmp_once(n, 0, 0, NULL);
    if (r != 1)
        say(n, "router: %s: the removal: %s (the lease ends it)", net_nat_proto_name(n->proto),
            r == 0 ? "no answer" : n->detail);
    return r == 1;
}

static void *run(void *arg)
{
    NetNat *n = arg;
    NetAddr ext;
    char ea[48] = "";
    int r = 0, rp = 0;
    memset(&ext, 0, sizeof ext);
    say(n, "router: opening UDP port %d (PCP, NAT-PMP, then UPnP; a %d s lease)%s", n->c.internal_port, n->c.lease_s,
        n->c.test ? " [test endpoints]" : "");
    rp = try_pmp(n, &ext);
    r = rp;
    if (r != 1 && r != -2) {
        n->proto = NET_NAT_NONE;
        r = try_upnp(n, &ext);
        if (r == 0 && rp == -1)
            r = -1;   /* PCP or NAT-PMP's refusal is the answer */
    }
    if (r == 1) {   /* mapped: what its internet address says */
        int k = net_addr_kind(&ext), zero = ext.family == NET_AF_INET && !(ext.addr[0] | ext.addr[1] | ext.addr[2] |
                                                                          ext.addr[3]);
        NetNatWhy why = NET_NAT_OK;
        ext.port = (uint16_t)n->ext_port;
        if (ext.family)
            net_addr_ip(&ext, ea, sizeof ea);
        net_mutex_lock(&n->m);
        n->st.external = ext;
        if (n->proto == NET_NAT_UPNP || n->proto == NET_NAT_PCP || n->proto == NET_NAT_PMP)
            n->st.router = n->proto == NET_NAT_UPNP ? n->svc.control.addr : n->gw;
        net_mutex_unlock(&n->m);
        if (ext.family == NET_AF_INET && (zero || k == NET_KIND_TAILSCALE)) {
            /* no internet address, or carriers' shared one: the mapping can't help; taken back at once */
            snprintf(n->detail, sizeof n->detail, "the router's internet address is %s", ea);
            say(n, "router: %s opened port %d, but the router's internet address is %s: %s; removing the mapping",
                net_nat_proto_name(n->proto), n->ext_port, ea, zero ? "it has none now" :
                "a shared one of the provider's (carrier-grade NAT), so nobody outside can reach it");
            remove_mapping(n);
            n->mapped = 0;
            set_state(n, NET_NAT_FAILED, zero ? NET_NAT_NO_INTERNET : NET_NAT_CGNAT);
        } else {
            if (ext.family == NET_AF_INET && k != NET_KIND_PUBLIC)
                why = NET_NAT_DOUBLE;
            n->detail[0] = 0;
            if (n->lease)
                say(n, "router: %s: UDP port %d outside forwarded to this PC's port %d, a %u s lease renewed "
                    "every %u s%s", net_nat_proto_name(n->proto), n->ext_port, n->c.internal_port, n->lease,
                    n->lease / 2 ? n->lease / 2 : 1, ext.family ? "" : "; the router didn't say its internet address");
            else
                say(n, "router: %s: UDP port %d outside forwarded to this PC's port %d, permanent until removed%s",
                    net_nat_proto_name(n->proto), n->ext_port, n->c.internal_port,
                    ext.family ? "" : "; the router didn't say its internet address");
            if (ext.family)
                say(n, "router: its internet address %s: %s", ea, why == NET_NAT_DOUBLE ?
                    "a private one: another router is outside it (double NAT)" : "a friend joins at it");
            set_state(n, NET_NAT_MAPPED, why);
        }
    } else if (r != -2) {
        NetNatWhy why = r == -1 ? n->refusal : NET_NAT_NO_ROUTER;
        if (why == NET_NAT_OK)
            why = NET_NAT_REFUSED;
        if (r == 0)
            snprintf(n->detail, sizeof n->detail, "no router answered (PCP, NAT-PMP, UPnP)");
        say(n, "router: the port isn't open: %s", n->detail);
        set_state(n, NET_NAT_FAILED, why);
    }
    /* renewals until the end */
    uint64_t mapped_at = net_os_now_ns(), next = 0;
    int lost = 0;
    if (r == 1 && n->mapped && n->lease)
        next = mapped_at + (uint64_t)(n->lease / 2 ? n->lease / 2 : 1) * 1000 * MS;
    for (;;) {
        uint64_t now = net_os_now_ns();
        if (!next) {
            if (nap(n, 3600ull * 1000 * MS))
                break;
            continue;
        }
        if (now < next) {
            if (nap(n, next - now))
                break;
            continue;
        }
        int k = renew(n);
        if (k == -2)
            break;
        now = net_os_now_ns();
        net_mutex_lock(&n->m);
        if (k == 1) {
            n->st.renewals++;
            n->st.external.port = (uint16_t)n->ext_port;
        } else {
            n->st.renew_failures++;
        }
        net_mutex_unlock(&n->m);
        if (k == 1) {
            mapped_at = now;
            next = now + (uint64_t)(n->lease / 2 ? n->lease / 2 : 1) * 1000 * MS;
            if (lost) {
                lost = 0;
                n->detail[0] = 0;
                set_state(n, NET_NAT_MAPPED, NET_NAT_OK);
                say(n, "router: the mapping is back");
            }
            if (!n->lease)
                next = 0;
        } else {
            uint64_t retry = (uint64_t)(n->lease / 8 ? n->lease / 8 : 1);
            if (retry > RETRY_MAX_S)
                retry = RETRY_MAX_S;
            next = now + retry * 1000 * MS;
            say(n, "router: renewing the mapping: %s; again in %u s", n->detail, (unsigned)retry);
            if (!lost && now >= mapped_at + (uint64_t)n->lease * 1000 * MS) {
                lost = 1;
                set_state(n, NET_NAT_FAILED, NET_NAT_LOST);
                say(n, "router: the lease ran out without a renewal: the port may be closed");
            }
        }
    }
    net_mutex_lock(&n->m);
    int keep = n->keep;
    net_mutex_unlock(&n->m);
    int removed = 1;
    if (n->mapped && !keep) {
        removed = remove_mapping(n);
        if (removed)
            say(n, "router: removed the mapping of port %d (%s)", n->ext_port, net_nat_proto_name(n->proto));
    } else if (n->mapped) {
        say(n, "router: the mapping of port %d left to its lease (%u s)", n->ext_port, n->lease);
        removed = 0;
    }
    net_mutex_lock(&n->m);
    n->removed = removed;
    n->st.state = NET_NAT_OFF;
    n->done = 1;
    net_cond_broadcast(&n->cv);
    net_mutex_unlock(&n->m);
    return NULL;
}

/* ---------------------------------------------------------------- the API */

void net_nat_config_defaults(NetNatConfig *c)
{
    const char *ssdp = getenv("RUSH_NAT_SSDP"), *gw = getenv("RUSH_NAT_GATEWAY"), *lease = getenv("RUSH_NAT_LEASE");
    memset(c, 0, sizeof *c);
    c->lease_s = NET_NAT_LEASE_S;
    c->description = "Rush2049PC";
    c->wait_ms = 1500;
    c->search_ms = 2500;
    c->http_ms = 3000;
    if ((ssdp && *ssdp) || (gw && *gw)) {   /* the tests' fakes: nothing else, the real router never */
        c->test = 1;
        snprintf(c->ssdp, sizeof c->ssdp, "%s", ssdp && *ssdp ? ssdp : "off");
        snprintf(c->gateway, sizeof c->gateway, "%s", gw && *gw ? gw : "off");
    }
    if (lease && atoi(lease) > 0)
        c->lease_s = atoi(lease);
}

NetNat *net_nat_start(const NetNatConfig *c)
{
    NetNat *n = calloc(1, sizeof *n);
    if (!n)
        return NULL;
    n->c = *c;
    if (n->c.lease_s < 2)
        n->c.lease_s = 2;
    if (!n->c.external_port)
        n->c.external_port = n->c.internal_port;
    if (n->c.wait_ms <= 0)
        n->c.wait_ms = 1500;
    if (n->c.search_ms <= 0)
        n->c.search_ms = 2500;
    if (n->c.http_ms <= 0)
        n->c.http_ms = 3000;
    snprintf(n->desc, sizeof n->desc, "%s", c->description && *c->description ? c->description : "Rush2049PC");
    n->c.description = n->desc;
    n->st.state = NET_NAT_WORKING;
    net_mutex_init(&n->m);
    net_cond_init(&n->cv);
    if (net_sock_startup() != 0 || net_thread_start(&n->th, run, n) != 0) {
        net_cond_destroy(&n->cv);
        net_mutex_destroy(&n->m);
        free(n);
        return NULL;
    }
    return n;
}

void net_nat_status(NetNat *n, NetNatStatus *st)
{
    if (!n) {
        memset(st, 0, sizeof *st);
        return;
    }
    net_mutex_lock(&n->m);
    *st = n->st;
    net_mutex_unlock(&n->m);
}

int net_nat_stop(NetNat *n, int keep, int timeout_ms)
{
    uint64_t end = net_os_now_ns() + (uint64_t)(timeout_ms > 0 ? timeout_ms : 0) * MS;
    int done, removed;
    if (!n)
        return 1;
    net_mutex_lock(&n->m);
    n->stop = 1;
    n->keep = keep;
    net_cond_broadcast(&n->cv);
    while (!n->done) {
        uint64_t now = net_os_now_ns();
        if (now >= end)
            break;
        net_cond_wait_ns(&n->cv, &n->m, end - now);
    }
    done = n->done;
    removed = n->removed;
    net_mutex_unlock(&n->m);
    if (!done)
        return 0;   /* the router is slow: its thread ends by itself; the lease covers the rest */
    net_thread_join(n->th);
    net_cond_destroy(&n->cv);
    net_mutex_destroy(&n->m);
    free(n);
    net_sock_shutdown();
    return removed;
}
