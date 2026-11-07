/* port/net/net_nat.h: the host's router opens its UDP port: PCP (RFC 6887), NAT-PMP (RFC 6886) and UPnP IGD
 * (SSDP, then WANIPConnection's or WANPPPConnection's AddPortMapping, GetExternalIPAddress, DeletePortMapping), in
 * that order (docs/port/NETWORK.md 13). Written from the RFCs and the UPnP Device Architecture and IGD service
 * specifications; no code of miniupnpc, libnatpmp or anyone else's.
 *
 * A NetNat is a thread of its own: it finds the router, asks for the mapping with a lease (600 s), renews it at half
 * the lease the router granted, and removes it when stopped. A process that is killed leaves it to the lease. It
 * speaks only to this PC's own network: the default gateway (PCP, NAT-PMP) and the device that answers SSDP's
 * multicast search on the LAN, whose description and control URLs must be on that device's own address, a private,
 * link-local or loopback one. Nothing outside the LAN is ever contacted.
 *
 * Tests (never the real router): RUSH_NAT_SSDP=HOST:PORT sends the search there (unicast) instead of
 * 239.255.255.250:1900; RUSH_NAT_GATEWAY=HOST:PORT sends PCP and NAT-PMP there instead of the default gateway's port
 * 5351. Either one set turns the other off unless it is set too ("off" turns one off). RUSH_NAT_LEASE=S asks for
 * an S-second lease. net_nat_config_defaults reads them.
 *
 * The parsers below are exposed for the unit tests; each takes a length and a buffer from the network as hostile:
 * every field bounded, nothing truncated silently (a field too long is an error), no recursion, no DTD. */
#ifndef NET_NAT_H
#define NET_NAT_H

#include <stddef.h>
#include <stdint.h>

#include "net_sock.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NET_NAT_LEASE_S 600    /* the lease asked for; renewed at half what the router grants */
#define NET_NAT_PMP_PORT 5351  /* PCP and NAT-PMP servers listen here (RFC 6886, 6887) */

typedef struct NetNatConfig {
    int internal_port;         /* this PC's UDP port to open (the session's) */
    int external_port;         /* the port asked for outside; 0: the internal one */
    int lease_s;               /* NET_NAT_LEASE_S; RUSH_NAT_LEASE */
    char ssdp[64];             /* "" the LAN's multicast; "HOST:PORT" unicast (tests); "off" */
    char gateway[64];          /* "" the default gateway's 5351; "HOST:PORT" (tests); "off" */
    int test;                  /* RUSH_NAT_SSDP or RUSH_NAT_GATEWAY set: loopback allowed, nothing else tried */
    const char *description;   /* the mapping's name in the router's list ("Rush2049PC") */
    int wait_ms;               /* PCP's and NAT-PMP's answer (retries at 250 ms and 750 ms); 1500 */
    int search_ms;             /* SSDP's answers; 2500 */
    int http_ms;               /* each HTTP request (connect, send, the whole answer); 3000 */
    void (*log)(void *user, const char *line);   /* from the thread; NULL: none */
    void *user;
} NetNatConfig;

/* Defaults, the environment's test hooks applied. */
void net_nat_config_defaults(NetNatConfig *c);

typedef enum {
    NET_NAT_OFF,               /* not started, or stopped */
    NET_NAT_WORKING,           /* looking for the router, asking */
    NET_NAT_MAPPED,            /* the port is open: external (when the router said it) */
    NET_NAT_FAILED,            /* why: below */
} NetNatState;

typedef enum {
    NET_NAT_OK,
    NET_NAT_NO_ROUTER,         /* nothing answered: no PCP, NAT-PMP or UPnP router (or UPnP is off in it) */
    NET_NAT_REFUSED,           /* a router answered and refused (detail: its error) */
    NET_NAT_CONFLICT,          /* the port is forwarded to another PC already (UPnP 718) */
    NET_NAT_NO_INTERNET,       /* the router has no internet address now (0.0.0.0, NAT-PMP's network failure) */
    NET_NAT_CGNAT,             /* its internet address is carriers' shared range (100.64/10): mapping removed */
    NET_NAT_DOUBLE,            /* its internet address is private: another router outside it (mapping kept) */
    NET_NAT_LOST,              /* renewals failed past the lease */
} NetNatWhy;

enum { NET_NAT_NONE, NET_NAT_PCP, NET_NAT_PMP, NET_NAT_UPNP };

typedef struct NetNatStatus {
    NetNatState state;
    NetNatWhy why;             /* MAPPED: OK or DOUBLE; FAILED: the reason */
    int proto;                 /* NET_NAT_PCP, _PMP, _UPNP: which one did it */
    NetAddr external;          /* the router's internet address and the port opened there (family NONE: unknown) */
    NetAddr router;            /* who answered (the gateway, the IGD) */
    uint32_t lease_s;          /* granted; 0 with MAPPED: permanent (an IGD that takes no other) */
    unsigned renewals, renew_failures;
    char detail[128];          /* the router's own words: "UPnP error 606 (Action not authorized)" */
    char name[64];             /* the IGD's friendlyName, when it said one */
} NetNatStatus;

typedef struct NetNat NetNat;

/* Starts the thread. NULL when it can't (no memory, no thread). */
NetNat *net_nat_start(const NetNatConfig *c);
void net_nat_status(NetNat *n, NetNatStatus *st);
/* Ends it: the mapping removed (keep 0) or left to its lease (keep 1: this PC's game goes and comes back), waiting up
 * to timeout_ms for the router's answer; the object freed. 1 when the mapping was removed (or there was none). */
int net_nat_stop(NetNat *n, int keep, int timeout_ms);
/* "PCP", "NAT-PMP", "UPnP" */
const char *net_nat_proto_name(int proto);
/* A guest that got no answer from host (at port): a sentence more for the player by the address's kind, "" when
 * none: an internet address needs the host's router to forward the port (UPnP, or by hand); 100.x is Tailscale's. */
void net_nat_join_hint(const NetAddr *host, int port, char *out, size_t n);

/* ---------------------------------------------------------------- the wire, for the tests */

/* NAT-PMP (RFC 6886). The request's length, or -1. */
int net_pmp_external_req(uint8_t *buf, int cap);
int net_pmp_map_req(uint8_t *buf, int cap, int internal_port, int external_port, uint32_t lifetime);
/* A response: 0 parsed (result: NAT-PMP's result code); -1 not this kind of answer (wrong opcode, too short, not
 * version 0). External: the address (opcode 128); map: the ports and lifetime (opcode 129). */
int net_pmp_parse_external(const uint8_t *buf, int len, int *result, uint8_t ext[4]);
int net_pmp_parse_map(const uint8_t *buf, int len, int *result, int *internal_port, int *external_port,
                      uint32_t *lifetime);

/* PCP (RFC 6887): a MAP request for UDP. client: this PC's address as the server sees it (IPv4: as ::ffff:a.b.c.d).
 * The request's length (60), or -1. */
int net_pcp_map_req(uint8_t *buf, int cap, const NetAddr *client, const uint8_t nonce[12], int internal_port,
                    int external_port, uint32_t lifetime);
typedef struct {
    int version;               /* 2, or 0 when a NAT-PMP server answered (result: NAT-PMP's code) */
    int result;                /* PCP's result code (0 SUCCESS, 1 UNSUPP_VERSION, 2 NOT_AUTHORIZED, ...) */
    uint32_t lifetime, epoch;
    int internal_port, external_port;
    NetAddr external;          /* IPv4 for ::ffff:a.b.c.d, else IPv6 */
} NetPcpAnswer;
/* 0 parsed: a MAP answer with the nonce asked with (or a NAT-PMP server's version error); -1 anything else. */
int net_pcp_parse(const uint8_t *buf, int len, const uint8_t nonce[12], NetPcpAnswer *a);
const char *net_pcp_result_name(int result);
const char *net_pmp_result_name(int result);

/* SSDP (UPnP Device Architecture 1.1, 1.3). The M-SEARCH for st, its length or -1. */
int net_ssdp_search(char *buf, int cap, const char *host_port, const char *st, int mx);
/* An answer: "HTTP/1.1 200 OK", then LOCATION and ST. 0, or -1 (not one, a field too long, none). */
int net_ssdp_parse(const char *buf, int len, char *location, size_t location_n, char *st, size_t st_n);

/* http://A.B.C.D[:PORT][/PATH]: an IPv4 literal only (no name, no user, no https). 0, or -1. */
typedef struct {
    NetAddr addr;              /* IPv4, the port (80 when none) */
    char path[256];            /* "/" when none */
} NetUrl;
int net_url_parse(const char *url, NetUrl *u);
/* ref against base (an absolute URL, an absolute path, or a path relative to base's directory). 0, or -1. */
int net_url_resolve(const NetUrl *base, const char *ref, NetUrl *out);

/* XML, as UPnP's descriptions and SOAP answers need it: an element's text by its local name (a prefix "u:" ignored),
 * the first at or after *from in doc[0..len), entities decoded (&amp; &lt; &gt; &quot; &apos; &#N; &#xN;, ASCII).
 * *from past it. 0; -1 none, malformed (no end tag, an element inside it), or longer than n-1. */
int net_xml_text(const char *doc, size_t len, const char *name, size_t *from, char *out, size_t n);
/* The span of the next element named name (its content, between its start and end tags): 0, or -1. */
int net_xml_span(const char *doc, size_t len, const char *name, size_t *from, size_t *start, size_t *end);

/* The device description: up to max WAN connection services, best first (WANIPConnection:2, :1, WANPPPConnection:1),
 * each with its type and control URL resolved against the description's URL (URLBase when it has one); the device's
 * friendlyName. Every URL must be on location's own address. The count (0: none). */
typedef struct {
    char type[96];
    NetUrl control;
} NetIgdService;
int net_igd_parse(const char *xml, size_t len, const NetUrl *location, NetIgdService *out, int max, char *name,
                  size_t name_n);

/* SOAP (UPnP's control): the request's body for action of service type with n arguments (name, value; values are
 * escaped). Its length, or -1. */
int net_soap_body(char *buf, size_t cap, const char *type, const char *action, const char *const *names,
                  const char *const *values, int n);
/* An HTTP answer (status line, headers, then a body by Content-Length, chunked, or to the end): the status and the
 * body (in place: chunks joined), NUL-terminated: buf[len] must be writable. 0, or -1 when malformed, cut short or
 * past its limits. */
int net_http_parse(char *buf, size_t len, int *status, char **body, size_t *body_len);
/* 1 when an answer is whole without its connection's end: the headers and Content-Length's body, or the last chunk. */
int net_http_complete(const char *buf, size_t len);
/* A SOAP fault's UPnPError: errorCode and errorDescription. 0, or -1 when it isn't one. */
int net_soap_fault(const char *body, size_t len, int *code, char *desc, size_t desc_n);

#ifdef __cplusplus
}
#endif

#endif
