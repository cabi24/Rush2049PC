/* port/net/tests/vnet.c: the in-memory network (vnet.h). */
#include "vnet.h"

#include <stdlib.h>
#include <string.h>

NetAddr vnet_addr(int id)
{
    NetAddr a;
    memset(&a, 0, sizeof a);
    a.family = NET_AF_VIRTUAL;
    a.addr[0] = (uint8_t)id;
    a.port = 1;
    return a;
}

static void push(VNode *n, const NetAddr *from, const void *buf, int len)
{
    if (n->cut || len > (int)sizeof n->q[0].data)
        return;
    if (n->n == n->cap) {
        int nc = n->cap ? n->cap * 2 : 64;
        VPkt *q = malloc((size_t)nc * sizeof *q);
        for (int i = 0; i < n->n; i++)
            q[i] = n->q[(n->head + i) % n->cap];
        free(n->q);
        n->q = q;
        n->head = 0;
        n->cap = nc;
    }
    VPkt *p = &n->q[(n->head + n->n++) % n->cap];
    p->from = *from;
    p->len = len;
    memcpy(p->data, buf, (size_t)len);
}

static int v_send(void *ctx, const NetAddr *to, const void *buf, int len)
{
    VNode *self = ctx;
    if (to->family != NET_AF_VIRTUAL || to->addr[0] >= VNET_NODES)
        return -1;
    if (self->cut)
        return 0;
    NetAddr from = vnet_addr(self->id);
    push(&self->net->node[to->addr[0]], &from, buf, len);
    return 0;
}

static int v_recv(void *ctx, NetAddr *from, void *buf, int cap)
{
    VNode *self = ctx;
    if (!self->n)
        return 0;
    VPkt *p = &self->q[self->head];
    self->head = (self->head + 1) % self->cap;
    self->n--;
    if (p->len > cap)
        return -2;
    *from = p->from;
    memcpy(buf, p->data, (size_t)p->len);
    return p->len;
}

static int v_wait(void *ctx, int timeout_us)
{
    (void)ctx;
    (void)timeout_us;
    return 0;   /* the test moves the clock */
}

uint64_t vnet_clock(void *user) { return ((VNet *)user)->now; }

void vnet_init(VNet *v)
{
    memset(v, 0, sizeof *v);
    v->now = 1000000000ull;
    for (int i = 0; i < VNET_NODES; i++) {
        VNode *n = &v->node[i];
        n->net = v;
        n->id = i;
        n->tr.ctx = n;
        n->tr.send = v_send;
        n->tr.recv = v_recv;
        n->tr.wait = v_wait;
    }
}

void vnet_free(VNet *v)
{
    for (int i = 0; i < VNET_NODES; i++)
        free(v->node[i].q);
}

void vnet_inject(VNet *v, int to, const NetAddr *from, const void *buf, int len)
{
    push(&v->node[to], from, buf, len);
}
