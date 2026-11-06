/* port/net/net_sock.h: one non-blocking UDP socket per session, POSIX or Winsock 2 (docs/port/NETWORK.md 2).
 * Dual-stack IPv6 (IPV6_V6ONLY off) where the host has IPv6, else IPv4. Addresses are kept in a small
 * OS-independent form; an IPv4 peer seen through a dual-stack socket (::ffff:a.b.c.d) is kept as IPv4. */
#ifndef NET_SOCK_H
#define NET_SOCK_H

#include <stddef.h>
#include <stdint.h>

enum { NET_AF_NONE = 0, NET_AF_INET = 4, NET_AF_INET6 = 6, NET_AF_VIRTUAL = 0xEE };

typedef struct NetAddr {
    uint16_t family;   /* NET_AF_* */
    uint16_t port;     /* host order */
    uint32_t scope;    /* IPv6 scope id */
    uint8_t addr[16];  /* IPv4 in the first 4 bytes; NET_AF_VIRTUAL: a node number in addr[0] (tests) */
} NetAddr;

int net_addr_equal(const NetAddr *a, const NetAddr *b);
/* "1.2.3.4:5", "[::1]:5", "virtual:1" */
void net_addr_str(const NetAddr *a, char *buf, size_t n);
/* The address alone, as a player types it: "1.2.3.4", "2001:db8::1". */
void net_addr_ip(const NetAddr *a, char *buf, size_t n);

/* What an address is, by its range (the host's waiting screen names this PC's addresses by it). */
enum { NET_KIND_OTHER, NET_KIND_TAILSCALE, NET_KIND_LAN, NET_KIND_PUBLIC, NET_KIND_LOOPBACK, NET_KIND_LINK_LOCAL };
/* Tailscale's ranges (100.64.0.0/10, also carriers' shared NAT range; fd7a:115c:a1e0::/48), a private network's
 * (10/8, 172.16/12, 192.168/16, fc00::/7), loopback, link-local (169.254/16, fe80::/10), public (the rest of IPv4's
 * unicast, IPv6's 2000::/3), or other. */
int net_addr_kind(const NetAddr *a);

/* This PC's own addresses, which a peer could reach it at: every IPv4 and IPv6 address of an interface that is up,
 * without loopback and link-local ones, each once, in the system's order, at most max. ifname is the interface's
 * name where the system gives it (getifaddrs: Linux, the BSDs), else "". Windows: getaddrinfo on the computer's own
 * name, which gives its permanent addresses (Tailscale's among them), with the sockets library started. How many. */
typedef struct NetLocalAddr {
    NetAddr addr;   /* port 0 */
    char ifname[32];
} NetLocalAddr;
int net_local_addrs(NetLocalAddr *out, int max);
/* The name an address resolves back to (getnameinfo, NI_NAMEREQD), and this computer's name with its canonical name
 * where the resolver gives one (getaddrinfo AI_CANONNAME; "" when not). 0, or -1. May wait on DNS; the sockets
 * library started (a session open). */
int net_addr_name(const NetAddr *a, char *out, size_t n);
int net_host_names(char *name, size_t n, char *canon, size_t canon_n);
/* A DNS PTR question for an IPv4 address (D.C.B.A.in-addr.arpa) asked of one server, server_ip:53 over UDP (the
 * host's waiting screen asks Tailscale's resolver, 100.100.100.100, for this PC's MagicDNS name: the system's own
 * resolver may answer for its own addresses with the bare computer name). The name without its final dot: 0, or -1
 * (no answer within timeout_ms, an error, no PTR). The query and the answer are built and read by the two functions
 * after it (the unit tests' too). */
int net_dns_ptr(const char *server_ip, const NetAddr *a, int timeout_ms, char *out, size_t n);
int net_dns_ptr_query(const NetAddr *a, uint16_t id, uint8_t *buf, int cap);   /* its length, or -1 */
int net_dns_ptr_answer(const uint8_t *buf, int len, uint16_t id, char *out, size_t n);   /* 0, or -1 */
/* "host", "host:port", "[v6]:port" or a bare IPv6 address ("::1"): host and port (default_port when none).
 * 0 on success. */
int net_split_hostport(const char *s, char *host, size_t hostcap, int *port, int default_port);

#ifdef _WIN32
typedef uintptr_t NetSockFd;   /* SOCKET */
#define NET_SOCK_INVALID ((NetSockFd)~(uintptr_t)0)
#else
typedef int NetSockFd;
#define NET_SOCK_INVALID (-1)
#endif

typedef struct NetSock {
    NetSockFd fd;
    int family;   /* NET_AF_INET6 (dual-stack when dual) or NET_AF_INET */
    int dual;
} NetSock;

/* WSAStartup on Windows (counted; the last shutdown calls WSACleanup). 0 on success. */
int net_sock_startup(void);
void net_sock_shutdown(void);
/* bind_host NULL or "" for any address; port 0 for any port. Dual-stack IPv6 when possible. 0 on success. */
int net_sock_open(NetSock *s, const char *bind_host, int port, char *err, size_t errlen);
void net_sock_close(NetSock *s);
int net_sock_port(const NetSock *s);
/* getaddrinfo for host (a name, a MagicDNS name, an IPv4 or IPv6 literal): IPv4 first when it has both,
 * unless the socket is IPv6-only. 0 on success. */
int net_sock_resolve(const NetSock *s, const char *host, int port, NetAddr *out, char *err, size_t errlen);
/* 0 sent (or dropped by the OS: UDP), -1 an error that is not the network's (bad address) */
int net_sock_send(NetSock *s, const NetAddr *to, const void *buf, int len);
/* >0 a datagram's length, 0 nothing waiting, -2 a datagram longer than cap (dropped), -1 an error */
int net_sock_recv(NetSock *s, NetAddr *from, void *buf, int cap);
/* Wait until a datagram is waiting or timeout_us passes: 1 readable, 0 timeout, -1 error. */
int net_sock_wait(NetSock *s, int timeout_us);

#endif
