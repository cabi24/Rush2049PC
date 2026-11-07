/* port/app/netrouter.h: the host's router asked to open the session's port (net.upnp; port/net/net_nat.h does it),
 * and what the host's waiting screen and the F1 Online page say about it (docs/port/NETPLAY.md 16). */
#ifndef PC_NETROUTER_H
#define PC_NETROUTER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PC_NET_ROUTER_LINES 2
#define PC_NET_ROUTER_LINE 72
enum { PC_NET_ROUTER_HINT, PC_NET_ROUTER_OK, PC_NET_ROUTER_WARN, PC_NET_ROUTER_VALUE };

/* The host, its port open (pc_net_connect): asks the router when on (net.upnp); settings: the settings file a toggle
 * is kept in ("" none: this run only). Off: nothing is sent anywhere. */
void pc_net_router_start(int port, int on, const char *settings);
/* U on the waiting screen: on (the router asked now) or off (the mapping removed); kept in the settings file. */
void pc_net_router_toggle(void);
/* 1 while the router is being asked (the first answer not in yet). */
int pc_net_router_working(void);
/* The waiting screen's lines about it (at most PC_NET_ROUTER_LINES; each PC_NET_ROUTER_LINE long at most) and each
 * one's tone (PC_NET_ROUTER_*). The count. Each new sentence is also said in the log once. */
int pc_net_router_lines(char out[][PC_NET_ROUTER_LINE], int *tone);
/* The router's internet address and the port opened there, "203.0.113.5:24049", when open and usable: 0, or -1. */
int pc_net_router_external(char *out, size_t n, char *ip, size_t ip_n, int *port);
/* The session's end (pc_net_finish): the mapping removed, or left to its lease (keep: this PC's game goes and comes
 * back, a rejoin). */
void pc_net_router_finish(int keep);

#ifdef __cplusplus
}
#endif

#endif
