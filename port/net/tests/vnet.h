/* port/net/tests/vnet.h: an in-memory network on a virtual clock for port/net's deterministic tests: each node's
 * datagrams arrive at once, in order (the sessions' RUSH_SIM_NET models do the delaying). */
#ifndef NET_VNET_H
#define NET_VNET_H

#include <stdint.h>

#include "net.h"

#define VNET_NODES 4

typedef struct VPkt {
    NetAddr from;
    int len;
    uint8_t data[1500];
} VPkt;

typedef struct VNode {
    struct VNet *net;
    int id, cut;   /* cut: the node is gone (its datagrams vanish both ways) */
    VPkt *q;
    int head, n, cap;
    NetTransport tr;
} VNode;

typedef struct VNet {
    uint64_t now;
    VNode node[VNET_NODES];
} VNet;

void vnet_init(VNet *v);
void vnet_free(VNet *v);
NetAddr vnet_addr(int id);
/* A raw datagram into node `to` as from `from` (the fuzzers, spoofing). */
void vnet_inject(VNet *v, int to, const NetAddr *from, const void *buf, int len);
uint64_t vnet_clock(void *user);

#endif
