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
