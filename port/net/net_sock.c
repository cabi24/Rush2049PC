/* port/net/net_sock.c: UDP sockets for port/net, POSIX and Winsock 2 (net_sock.h). Written from the POSIX and
 * Windows Sockets documentation. */
#if defined(_WIN32) && (!defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601)
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601   /* inet_ntop, getaddrinfo */
#endif
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE   /* ppoll */
#endif
#include "net_sock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)   /* mstcpip.h */
#endif
typedef int socklen_t_;
#define SOCK(s) ((SOCKET)(s)->fd)
static int last_error(void) { return WSAGetLastError(); }
#define E_WOULDBLOCK WSAEWOULDBLOCK
#define E_INTR WSAEINTR
#define E_CONNRESET WSAECONNRESET
#define E_CONNREFUSED WSAECONNREFUSED
#define E_MSGSIZE WSAEMSGSIZE
#define E_ADDRINUSE WSAEADDRINUSE
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
typedef socklen_t socklen_t_;
#define SOCK(s) ((s)->fd)
static int last_error(void) { return errno; }
#define E_WOULDBLOCK EAGAIN
#define E_INTR EINTR
#define E_CONNRESET ECONNRESET
#define E_CONNREFUSED ECONNREFUSED
#define E_MSGSIZE EMSGSIZE
#define E_ADDRINUSE EADDRINUSE
#endif

int net_addr_equal(const NetAddr *a, const NetAddr *b)
{
    if (a->family != b->family || a->port != b->port)
        return 0;
    switch (a->family) {
    case NET_AF_INET:
        return memcmp(a->addr, b->addr, 4) == 0;
    case NET_AF_INET6:
        return memcmp(a->addr, b->addr, 16) == 0 && a->scope == b->scope;
    case NET_AF_VIRTUAL:
        return a->addr[0] == b->addr[0];
    default:
        return 0;
    }
}

void net_addr_str(const NetAddr *a, char *buf, size_t n)
{
    char ip[64] = "?";
    if (!n)
        return;
    if (a->family == NET_AF_INET) {
        snprintf(buf, n, "%u.%u.%u.%u:%u", a->addr[0], a->addr[1], a->addr[2], a->addr[3], a->port);
    } else if (a->family == NET_AF_INET6) {
        inet_ntop(AF_INET6, (void *)a->addr, ip, sizeof ip);
        snprintf(buf, n, "[%s]:%u", ip, a->port);
    } else if (a->family == NET_AF_VIRTUAL) {
        snprintf(buf, n, "virtual:%u", a->addr[0]);
    } else {
        snprintf(buf, n, "(none)");
    }
}

void net_addr_ip(const NetAddr *a, char *buf, size_t n)
{
    char ip[64] = "?";
    if (!n)
        return;
    if (a->family == NET_AF_INET) {
        snprintf(buf, n, "%u.%u.%u.%u", a->addr[0], a->addr[1], a->addr[2], a->addr[3]);
    } else if (a->family == NET_AF_INET6) {
        inet_ntop(AF_INET6, (void *)a->addr, ip, sizeof ip);
        snprintf(buf, n, "%s", ip);
    } else {
        net_addr_str(a, buf, n);
    }
}

int net_addr_kind(const NetAddr *a)
{
    const uint8_t *b = a->addr;
    if (a->family == NET_AF_INET) {
        if (b[0] == 127)
            return NET_KIND_LOOPBACK;
        if (b[0] == 169 && b[1] == 254)
            return NET_KIND_LINK_LOCAL;
        if (b[0] == 100 && (b[1] & 0xC0) == 64)   /* 100.64.0.0/10 */
            return NET_KIND_TAILSCALE;
        if (b[0] == 10 || (b[0] == 172 && (b[1] & 0xF0) == 16) || (b[0] == 192 && b[1] == 168))
            return NET_KIND_LAN;
        if (b[0] == 0 || b[0] >= 224)   /* "this network", multicast, reserved */
            return NET_KIND_OTHER;
        return NET_KIND_PUBLIC;
    }
    if (a->family == NET_AF_INET6) {
        static const uint8_t loop[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
        static const uint8_t ts[6] = {0xfd, 0x7a, 0x11, 0x5c, 0xa1, 0xe0};
        if (!memcmp(b, loop, 16))
            return NET_KIND_LOOPBACK;
        if (b[0] == 0xfe && (b[1] & 0xC0) == 0x80)
            return NET_KIND_LINK_LOCAL;
        if (!memcmp(b, ts, 6))
            return NET_KIND_TAILSCALE;
        if ((b[0] & 0xFE) == 0xfc)
            return NET_KIND_LAN;
        if ((b[0] & 0xE0) == 0x20)
            return NET_KIND_PUBLIC;
    }
    return NET_KIND_OTHER;
}

int net_split_hostport(const char *s, char *host, size_t hostcap, int *port, int default_port)
{
    const char *colon, *end;
    size_t hl;
    if (!s || !*s || !hostcap)
        return -1;
    *port = default_port;
    if (s[0] == '[') {   /* [v6]:port */
        end = strchr(s, ']');
        if (!end)
            return -1;
        hl = (size_t)(end - s - 1);
        if (hl + 1 > hostcap || hl == 0)
            return -1;
        memcpy(host, s + 1, hl);
        host[hl] = 0;
        if (end[1] == ':') {
            char *e;
            long p = strtol(end + 2, &e, 10);
            if (*e || p <= 0 || p > 65535)
                return -1;
            *port = (int)p;
        } else if (end[1]) {
            return -1;
        }
        return 0;
    }
    colon = strchr(s, ':');
    if (colon && strchr(colon + 1, ':'))   /* two colons: a bare IPv6 address */
        colon = NULL;
    hl = colon ? (size_t)(colon - s) : strlen(s);
    if (hl + 1 > hostcap || hl == 0)
        return -1;
    memcpy(host, s, hl);
    host[hl] = 0;
    if (colon) {
        char *e;
        long p = strtol(colon + 1, &e, 10);
        if (*e || p <= 0 || p > 65535)
            return -1;
        *port = (int)p;
    }
    return 0;
}

static void from_sockaddr(const struct sockaddr *sa, NetAddr *a)
{
    memset(a, 0, sizeof *a);
    if (sa->sa_family == AF_INET) {
        const struct sockaddr_in *in = (const struct sockaddr_in *)sa;
        a->family = NET_AF_INET;
        a->port = ntohs(in->sin_port);
        memcpy(a->addr, &in->sin_addr, 4);
    } else if (sa->sa_family == AF_INET6) {
        const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *)sa;
        static const uint8_t mapped[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF};
        const uint8_t *b = (const uint8_t *)&in6->sin6_addr;
        a->port = ntohs(in6->sin6_port);
        if (memcmp(b, mapped, 12) == 0) {   /* an IPv4 peer on the dual-stack socket */
            a->family = NET_AF_INET;
            memcpy(a->addr, b + 12, 4);
        } else {
            a->family = NET_AF_INET6;
            memcpy(a->addr, b, 16);
            a->scope = in6->sin6_scope_id;
        }
    }
}

/* The sockaddr to send to a on socket s: IPv4 as ::ffff:a.b.c.d on a dual-stack socket. Its length, 0 if none. */
static int to_sockaddr(const NetSock *s, const NetAddr *a, struct sockaddr_storage *ss)
{
    memset(ss, 0, sizeof *ss);
    if (s->family == NET_AF_INET) {
        struct sockaddr_in *in = (struct sockaddr_in *)ss;
        if (a->family != NET_AF_INET)
            return 0;
        in->sin_family = AF_INET;
        in->sin_port = htons(a->port);
        memcpy(&in->sin_addr, a->addr, 4);
        return (int)sizeof *in;
    }
    struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)ss;
    uint8_t *b = (uint8_t *)&in6->sin6_addr;
    in6->sin6_family = AF_INET6;
    in6->sin6_port = htons(a->port);
    if (a->family == NET_AF_INET) {
        if (!s->dual)
            return 0;
        b[10] = b[11] = 0xFF;
        memcpy(b + 12, a->addr, 4);
    } else if (a->family == NET_AF_INET6) {
        memcpy(b, a->addr, 16);
        in6->sin6_scope_id = a->scope;
    } else {
        return 0;
    }
    return (int)sizeof *in6;
}

static int g_started;

int net_sock_startup(void)
{
#ifdef _WIN32
    if (g_started++ == 0) {
        WSADATA wd;
        if (WSAStartup(MAKEWORD(2, 2), &wd) != 0) {
            g_started = 0;
            return -1;
        }
    }
#else
    g_started++;
#endif
    return 0;
}

void net_sock_shutdown(void)
{
    if (g_started > 0 && --g_started == 0) {
#ifdef _WIN32
        WSACleanup();
#endif
    }
}

static void close_fd(NetSockFd fd)
{
#ifdef _WIN32
    closesocket((SOCKET)fd);
#else
    close(fd);
#endif
}

static int set_nonblocking(NetSockFd fd)
{
#ifdef _WIN32
    u_long one = 1;
    return ioctlsocket((SOCKET)fd, FIONBIO, &one) == 0 ? 0 : -1;
#else
    int fl = fcntl(fd, F_GETFL, 0);
    return fl >= 0 && fcntl(fd, F_SETFL, fl | O_NONBLOCK) == 0 ? 0 : -1;
#endif
}

/* A socket of family af bound to sa; dual-stack when af is IPv6 and sa is the unspecified address. */
static int open_bound(NetSock *s, int af, const struct sockaddr *sa, int salen, int want_dual, char *err, size_t n)
{
    NetSockFd fd = (NetSockFd)socket(af, SOCK_DGRAM, IPPROTO_UDP);
    if (fd == NET_SOCK_INVALID) {
        snprintf(err, n, "socket: error %d", last_error());
        return -1;
    }
    s->dual = 0;
    if (af == AF_INET6) {
        int off = 0;
        if (want_dual && setsockopt((NetSockFd)fd, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&off, sizeof off) == 0)
            s->dual = 1;
    }
    int buf = 1 << 18;   /* 256 KiB: a stall's worth of packets */
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const char *)&buf, sizeof buf);
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (const char *)&buf, sizeof buf);
#ifdef _WIN32
    {   /* without this an ICMP port unreachable makes the next recvfrom fail with WSAECONNRESET */
        BOOL no = FALSE;
        DWORD ret = 0;
        WSAIoctl((SOCKET)fd, SIO_UDP_CONNRESET, &no, sizeof no, NULL, 0, &ret, NULL, NULL);
    }
#endif
    if (bind(fd, sa, (socklen_t_)salen) != 0) {
        int e = last_error();
        snprintf(err, n, "bind: error %d%s", e, e ==
#ifdef _WIN32
                 WSAEADDRINUSE
#else
                 EADDRINUSE
#endif
                 ? " (the port is in use)" : "");
        close_fd(fd);
        return -1;
    }
    if (set_nonblocking(fd) != 0) {
        snprintf(err, n, "non-blocking: error %d", last_error());
        close_fd(fd);
        return -1;
    }
    s->fd = fd;
    s->family = af == AF_INET6 ? NET_AF_INET6 : NET_AF_INET;
    return 0;
}

int net_sock_open(NetSock *s, const char *bind_host, int port, char *err, size_t errlen)
{
    char dummy[8];
    if (!err) {
        err = dummy;
        errlen = sizeof dummy;
    }
    s->fd = NET_SOCK_INVALID;
    s->family = NET_AF_NONE;
    s->dual = 0;
    if (port < 0 || port > 65535) {
        snprintf(err, errlen, "port %d out of range", port);
        return -1;
    }
    if (bind_host && *bind_host) {
        struct addrinfo hints, *res = NULL;
        char ps[16];
        memset(&hints, 0, sizeof hints);
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE;
        snprintf(ps, sizeof ps, "%d", port);
        int r = getaddrinfo(bind_host, ps, &hints, &res);
        if (r != 0 || !res) {
            snprintf(err, errlen, "%s: %s", bind_host, gai_strerror(r));
            return -1;
        }
        int any6 = 0;
        if (res->ai_family == AF_INET6) {
            static const uint8_t zero[16];
            any6 = memcmp(&((struct sockaddr_in6 *)res->ai_addr)->sin6_addr, zero, 16) == 0;
        }
        r = open_bound(s, res->ai_family, res->ai_addr, (int)res->ai_addrlen, any6, err, errlen);
        freeaddrinfo(res);
        return r;
    }
    struct sockaddr_in6 in6;
    memset(&in6, 0, sizeof in6);
    in6.sin6_family = AF_INET6;
    in6.sin6_addr = in6addr_any;
    in6.sin6_port = htons((uint16_t)port);
    if (open_bound(s, AF_INET6, (struct sockaddr *)&in6, sizeof in6, 1, err, errlen) == 0) {
        if (s->dual)
            return 0;
        net_sock_close(s);   /* IPv6 only: IPv4 peers couldn't reach it; take IPv4 instead */
    }
    struct sockaddr_in in;
    memset(&in, 0, sizeof in);
    in.sin_family = AF_INET;
    in.sin_addr.s_addr = htonl(INADDR_ANY);
    in.sin_port = htons((uint16_t)port);
    return open_bound(s, AF_INET, (struct sockaddr *)&in, sizeof in, 0, err, errlen);
}

void net_sock_close(NetSock *s)
{
    if (s->fd != NET_SOCK_INVALID)
        close_fd(s->fd);
    s->fd = NET_SOCK_INVALID;
}

int net_sock_port(const NetSock *s)
{
    struct sockaddr_storage ss;
    socklen_t_ len = sizeof ss;
    NetAddr a;
    if (getsockname(SOCK(s), (struct sockaddr *)&ss, &len) != 0)
        return -1;
    from_sockaddr((struct sockaddr *)&ss, &a);
    return a.port;
}

int net_sock_resolve(const NetSock *s, const char *host, int port, NetAddr *out, char *err, size_t errlen)
{
    struct addrinfo hints, *res = NULL, *pick = NULL;
    char ps[16], dummy[8];
    if (!err) {
        err = dummy;
        errlen = sizeof dummy;
    }
    memset(&hints, 0, sizeof hints);
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family = s->family == NET_AF_INET ? AF_INET : AF_UNSPEC;
    snprintf(ps, sizeof ps, "%d", port);
    int r = getaddrinfo(host, ps, &hints, &res);
    if (r != 0 || !res) {
        snprintf(err, errlen, "%s: %s", host, gai_strerror(r));
        return -1;
    }
    for (struct addrinfo *a = res; a; a = a->ai_next)   /* IPv4 first: forwarded ports are usually IPv4 */
        if (a->ai_family == AF_INET && (s->family == NET_AF_INET || s->dual)) {
            pick = a;
            break;
        }
    if (!pick)
        for (struct addrinfo *a = res; a; a = a->ai_next)
            if (a->ai_family == AF_INET6 && s->family == NET_AF_INET6) {
                pick = a;
                break;
            }
    if (!pick) {
        snprintf(err, errlen, "%s: no address this socket can reach", host);
        freeaddrinfo(res);
        return -1;
    }
    from_sockaddr(pick->ai_addr, out);
    freeaddrinfo(res);
    return 0;
}

int net_sock_send(NetSock *s, const NetAddr *to, const void *buf, int len)
{
    struct sockaddr_storage ss;
    int sl = to_sockaddr(s, to, &ss);
    if (!sl)
        return -1;
    for (;;) {
        if (sendto(SOCK(s), (const char *)buf, len, 0, (struct sockaddr *)&ss, (socklen_t_)sl) >= 0)
            return 0;
        int e = last_error();
        if (e == E_INTR)
            continue;
        return 0;   /* full buffers, unreachable networks: UDP loses it; the protocol resends */
    }
}

int net_sock_recv(NetSock *s, NetAddr *from, void *buf, int cap)
{
    for (int tries = 0; tries < 64; tries++) {
        struct sockaddr_storage ss;
        socklen_t_ sl = sizeof ss;
#if defined(__linux__)
        int r = (int)recvfrom(SOCK(s), buf, (size_t)cap, MSG_TRUNC, (struct sockaddr *)&ss, &sl);
        if (r > cap)
            return -2;   /* MSG_TRUNC gives a longer datagram's real length */
#else
        int r = (int)recvfrom(SOCK(s), (char *)buf, cap, 0, (struct sockaddr *)&ss, &sl);
#endif
        if (r >= 0) {
            from_sockaddr((struct sockaddr *)&ss, from);
            if (from->family == NET_AF_NONE)
                continue;
            return r == 0 ? -2 : r;   /* an empty datagram: malformed, like an oversized one */
        }
        int e = last_error();
        if (e == E_WOULDBLOCK
#if !defined(_WIN32) && EWOULDBLOCK != EAGAIN
            || e == EWOULDBLOCK
#endif
        )
            return 0;
        if (e == E_INTR || e == E_CONNRESET || e == E_CONNREFUSED)
            continue;   /* an ICMP error from an earlier send: not this socket's problem */
        if (e == E_MSGSIZE)
            return -2;
        return -1;
    }
    return 0;
}

int net_sock_wait(NetSock *s, int timeout_us)
{
    if (timeout_us < 0)
        timeout_us = 0;
#ifdef _WIN32
    fd_set rf;
    struct timeval tv = {timeout_us / 1000000, timeout_us % 1000000};
    FD_ZERO(&rf);
    FD_SET(SOCK(s), &rf);
    int r = select(0, &rf, NULL, NULL, &tv);
    return r > 0 ? 1 : r == 0 ? 0 : -1;
#else
    struct pollfd p = {s->fd, POLLIN, 0};
#ifdef __linux__
    struct timespec ts = {timeout_us / 1000000, (long)(timeout_us % 1000000) * 1000};
    int r = ppoll(&p, 1, &ts, NULL);
#else
    int r = poll(&p, 1, (timeout_us + 999) / 1000);
#endif
    if (r < 0 && errno == EINTR)
        return 0;
    return r > 0 ? 1 : r == 0 ? 0 : -1;
#endif
}

/* ---------------------------------------------------------------- this PC's addresses and names */

/* a's sockaddr, port 0 (no socket to suit): its length, 0 for another family */
static int plain_sockaddr(const NetAddr *a, struct sockaddr_storage *ss)
{
    memset(ss, 0, sizeof *ss);
    if (a->family == NET_AF_INET) {
        struct sockaddr_in *in = (struct sockaddr_in *)ss;
        in->sin_family = AF_INET;
        memcpy(&in->sin_addr, a->addr, 4);
        return (int)sizeof *in;
    }
    if (a->family == NET_AF_INET6) {
        struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)ss;
        in6->sin6_family = AF_INET6;
        memcpy(&in6->sin6_addr, a->addr, 16);
        in6->sin6_scope_id = a->scope;
        return (int)sizeof *in6;
    }
    return 0;
}

/* One address for net_local_addrs: kept unless loopback, link-local or listed already. The count. */
static int add_local(NetLocalAddr *out, int n, int max, const struct sockaddr *sa, const char *ifname)
{
    NetLocalAddr a;
    int k;
    if (n >= max || !sa || (sa->sa_family != AF_INET && sa->sa_family != AF_INET6))
        return n;
    memset(&a, 0, sizeof a);
    from_sockaddr(sa, &a.addr);
    a.addr.port = 0;
    a.addr.scope = 0;
    k = net_addr_kind(&a.addr);
    if (a.addr.family == NET_AF_NONE || k == NET_KIND_LOOPBACK || k == NET_KIND_LINK_LOCAL)
        return n;
    for (int i = 0; i < n; i++)
        if (net_addr_equal(&out[i].addr, &a.addr))
            return n;
    snprintf(a.ifname, sizeof a.ifname, "%s", ifname ? ifname : "");
    out[n] = a;
    return n + 1;
}

int net_local_addrs(NetLocalAddr *out, int max)
{
    int n = 0;
#ifdef _WIN32
    char name[256];
    struct addrinfo hints, *res = NULL;
    if (gethostname(name, (int)sizeof name) != 0)
        return 0;
    name[sizeof name - 1] = 0;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(name, NULL, &hints, &res) != 0)
        return 0;
    for (struct addrinfo *a = res; a; a = a->ai_next)
        n = add_local(out, n, max, a->ai_addr, NULL);
    freeaddrinfo(res);
#else
    struct ifaddrs *list = NULL;
    if (getifaddrs(&list) != 0)
        return 0;
    for (struct ifaddrs *i = list; i; i = i->ifa_next)
        if ((i->ifa_flags & IFF_UP) && !(i->ifa_flags & IFF_LOOPBACK))
            n = add_local(out, n, max, i->ifa_addr, i->ifa_name);
    freeifaddrs(list);
#endif
    return n;
}

int net_addr_name(const NetAddr *a, char *out, size_t n)
{
    struct sockaddr_storage ss;
    char host[1025];
    int sl = plain_sockaddr(a, &ss);
    if (!sl || !n)
        return -1;
    if (getnameinfo((struct sockaddr *)&ss, (socklen_t_)sl, host, sizeof host, NULL, 0, NI_NAMEREQD) != 0)
        return -1;
    snprintf(out, n, "%s", host);
    return 0;
}

int net_host_names(char *name, size_t n, char *canon, size_t canon_n)
{
    char h[256];
    struct addrinfo hints, *res = NULL;
    if (canon_n)
        canon[0] = 0;
    if (gethostname(h, (int)sizeof h) != 0)
        return -1;
    h[sizeof h - 1] = 0;
    if (n)
        snprintf(name, n, "%s", h);
    memset(&hints, 0, sizeof hints);
    hints.ai_flags = AI_CANONNAME;
    hints.ai_socktype = SOCK_DGRAM;
    if (canon_n && getaddrinfo(h, NULL, &hints, &res) == 0) {
        if (res && res->ai_canonname)
            snprintf(canon, canon_n, "%s", res->ai_canonname);
        freeaddrinfo(res);
    }
    return 0;
}

/* ---------------------------------------------------------------- a DNS PTR question (RFC 1035) */

int net_dns_ptr_query(const NetAddr *a, uint16_t id, uint8_t *buf, int cap)
{
    char name[64];
    int n = 12;
    if (a->family != NET_AF_INET || cap < 64)
        return -1;
    memset(buf, 0, 12);
    buf[0] = (uint8_t)(id >> 8);
    buf[1] = (uint8_t)id;
    buf[2] = 0x01;   /* recursion desired */
    buf[5] = 1;      /* one question */
    snprintf(name, sizeof name, "%u.%u.%u.%u.in-addr.arpa", a->addr[3], a->addr[2], a->addr[1], a->addr[0]);
    for (const char *l = name; *l;) {   /* labels: a length byte, then the label */
        const char *dot = strchr(l, '.');
        int k = dot ? (int)(dot - l) : (int)strlen(l);
        buf[n++] = (uint8_t)k;
        memcpy(buf + n, l, (size_t)k);
        n += k;
        l += k + (dot != NULL);
    }
    buf[n++] = 0;
    buf[n++] = 0, buf[n++] = 12;   /* PTR */
    buf[n++] = 0, buf[n++] = 1;    /* IN */
    return n;
}

/* The name at *pos of message m (labels and compression pointers), dotted, into out; *pos past it (past the first
 * pointer, when it has one). 0, or -1 when it is malformed or doesn't fit. */
static int dns_name(const uint8_t *m, int len, int *pos, char *out, size_t n)
{
    int p = *pos, jumped = 0, hops = 0;
    size_t o = 0;
    if (!n)
        return -1;
    for (;;) {
        if (p >= len)
            return -1;
        int c = m[p];
        if (c == 0) {
            if (!jumped)
                *pos = p + 1;
            break;
        }
        if ((c & 0xC0) == 0xC0) {   /* a pointer to a name earlier in the message */
            if (p + 1 >= len || ++hops > 32)
                return -1;
            if (!jumped)
                *pos = p + 2;
            jumped = 1;
            p = (c & 0x3F) << 8 | m[p + 1];
            continue;
        }
        if ((c & 0xC0) || p + 1 + c > len || o + (o > 0) + (size_t)c + 1 > n)
            return -1;
        if (o)
            out[o++] = '.';
        for (int i = 0; i < c; i++) {
            int ch = m[p + 1 + i];
            out[o++] = (char)(ch > 32 && ch < 127 ? ch : '?');
        }
        p += 1 + c;
    }
    out[o] = 0;
    return 0;
}

int net_dns_ptr_answer(const uint8_t *m, int len, uint16_t id, char *out, size_t n)
{
    char name[256];
    if (len < 12 || (m[0] << 8 | m[1]) != id || !(m[2] & 0x80) || (m[3] & 0x0F) != 0)
        return -1;   /* not the answer to it, or an error (no such name...) */
    int qd = m[4] << 8 | m[5], an = m[6] << 8 | m[7], pos = 12;
    for (int i = 0; i < qd; i++) {
        if (dns_name(m, len, &pos, name, sizeof name) != 0 || pos + 4 > len)
            return -1;
        pos += 4;
    }
    for (int i = 0; i < an; i++) {
        if (dns_name(m, len, &pos, name, sizeof name) != 0 || pos + 10 > len)
            return -1;
        int type = m[pos] << 8 | m[pos + 1], cls = m[pos + 2] << 8 | m[pos + 3], rdlen = m[pos + 8] << 8 | m[pos + 9];
        pos += 10;
        if (pos + rdlen > len)
            return -1;
        if (type == 12 && cls == 1) {
            int p = pos;
            if (dns_name(m, len, &p, name, sizeof name) != 0 || !name[0])
                return -1;
            snprintf(out, n, "%s", name);
            return 0;
        }
        pos += rdlen;
    }
    return -1;
}

int net_dns_ptr(const char *server_ip, const NetAddr *a, int timeout_ms, char *out, size_t n)
{
    struct sockaddr_in to;
    uint8_t q[128], r[1500];
    uint16_t id = (uint16_t)((uintptr_t)&to >> 4 ^ (uintptr_t)time(NULL) ^ (uintptr_t)clock());
    int qn, ok = -1;
    memset(&to, 0, sizeof to);
    to.sin_family = AF_INET;
    to.sin_port = htons(53);
    if (inet_pton(AF_INET, server_ip, &to.sin_addr) != 1 || (qn = net_dns_ptr_query(a, id, q, sizeof q)) < 0)
        return -1;
    NetSock s = {(NetSockFd)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP), NET_AF_INET, 0};
    if (s.fd == NET_SOCK_INVALID)
        return -1;
    for (int attempt = 0, answered = 0; attempt < 2 && !answered; attempt++) {   /* a lost question: once more */
        if (sendto(SOCK(&s), (const char *)q, qn, 0, (struct sockaddr *)&to, (socklen_t_)sizeof to) < 0)
            break;
        for (int waits = 0; waits < 8 && !answered; waits++) {   /* datagrams from elsewhere are skipped */
            struct sockaddr_storage from;
            socklen_t_ fl = sizeof from;
            if (net_sock_wait(&s, timeout_ms * 1000 / 2) != 1)
                break;
            int k = (int)recvfrom(SOCK(&s), (char *)r, (int)sizeof r, 0, (struct sockaddr *)&from, &fl);
            if (k <= 0 || from.ss_family != AF_INET ||
                memcmp(&((struct sockaddr_in *)&from)->sin_addr, &to.sin_addr, 4) != 0)
                continue;
            answered = k >= 12 && (r[0] << 8 | r[1]) == id;   /* the server's answer, a name or none (NXDOMAIN) */
            ok = net_dns_ptr_answer(r, k, id, out, n);
        }
    }
    close_fd(s.fd);
    return ok;
}
