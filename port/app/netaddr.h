/* port/app/netaddr.h: this PC's addresses a friend can join with, best first, and its MagicDNS name (netaddr.c), for
 * the host's waiting screen (netwait.c) and the F1 Online page (netmenu.c). */
#ifndef PC_NETADDR_H
#define PC_NETADDR_H

#include "net.h"
#include "net_os.h"
#include "net_sock.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PC_NET_ADDRS 32
typedef struct {
    NetLocalAddr addr[PC_NET_ADDRS];
    int naddr, order[PC_NET_ADDRS];   /* the addresses, best first */
    NetAddr ts4;                      /* the first Tailscale IPv4 address, whose name is looked up */
    int ts_any;
    int lookup;                       /* 0 none, 1 running, 2 done (atomic: the thread's result is published) */
    char magic[256];                  /* the MagicDNS name, "" if none was found */
    NetThread thread;
    int thread_on;
} PcNetAddrs;
/* The addresses, best first (Tailscale's, IPv4 first; the local network's: 192.168, 10, then 172.16-31; public IPv4,
 * public IPv6; the rest; containers' and virtual machines' bridges last), each in the log; and the MagicDNS name of
 * the Tailscale IPv4 address looked up on a thread (Tailscale's resolver 100.100.100.100, then the system's, then the
 * computer's own name; only a name under ts.net counts). */
void pc_net_addrs_find(PcNetAddrs *a);
int pc_net_addrs_rank(const NetLocalAddr *a);           /* 90 and more: a bridge, not shown */
const char *pc_net_addrs_tag(const NetLocalAddr *a);    /* "Tailscale", "local network", "internet", ... */
int pc_net_addrs_lookup(const PcNetAddrs *a);           /* the name's lookup: 0 none, 1 running, 2 done */
void pc_net_addrs_close(PcNetAddrs *a);                 /* joins the thread when it is done */
/* netwait.c: the window was closed before the game (its close button, not Esc): the player stopped the program. */
int pc_net_window_closed(void);

#ifdef __cplusplus
}
#endif

#endif
