/* port/net/net_sim.h: RUSH_SIM_NET, a bad network modelled in a session's receive path, seeded and reproducible
 * (docs/port/NETWORK.md 6). The spec is comma-separated key=value pairs:
 *
 *   delay=MS        one-way delay added to every packet received
 *   jitter=MS       plus a uniform draw in [-MS, +MS] per packet (packets overtake each other)
 *   loss=PCT        packets dropped
 *   dup=PCT         packets delivered twice (the copy with its own delay)
 *   reorder=PCT     packets held back a further 5-40 ms
 *   at=S            the keys after it apply from S seconds after the session was made (a latency that changes)
 *   burst=E:L:PCT   every E seconds, for L seconds, PCT loss on top
 *   spike=E:L:MS    every E seconds, for L seconds, MS more delay
 *   pause=AT:LEN    this peer freezes at AT seconds for LEN seconds: it sends nothing, and what arrives meanwhile
 *                   waits in the socket, as for a stopped process (net_frozen tells the caller to stop too)
 *   seed=N          the random draws' seed (else the session's)
 *
 * e.g. RUSH_SIM_NET=delay=150,jitter=40,loss=10,dup=2,reorder=2 or delay=10,at=20,delay=200,at=40,delay=30 */
#ifndef NET_SIM_H
#define NET_SIM_H

#include <stddef.h>
#include <stdint.h>

#include "net_sock.h"

#define NET_SIM_PHASES 16

typedef struct NetSimPhase {
    double at_s, delay_ms, jitter_ms, loss_pct, dup_pct, reorder_pct;
} NetSimPhase;

typedef struct NetSimModel {
    int active;
    int nphases;
    NetSimPhase phase[NET_SIM_PHASES];
    double burst_every_s, burst_len_s, burst_loss_pct;
    double spike_every_s, spike_len_s, spike_ms;
    double pause_at_s, pause_len_s;
    int has_seed;
    uint64_t seed;
} NetSimModel;

/* 0 on success (an empty or NULL spec is an inactive model); err says what is wrong otherwise. */
int net_sim_parse(const char *spec, NetSimModel *m, char *err, size_t errlen);
void net_sim_describe(const NetSimModel *m, char *buf, size_t n);

typedef struct NetSim NetSim;

/* origin: the time "at=" counts from (the session's creation), on the session's clock */
NetSim *net_sim_new(const NetSimModel *m, uint64_t seed, uint64_t origin_ns);
void net_sim_free(NetSim *s);
/* In a pause: the peer is stopped. */
int net_sim_frozen(const NetSim *s, uint64_t now);
/* A datagram that came in at now: queued for later, twice, or dropped. flag goes with it (the session's "read
 * after a gap": its arrival time is unknown, and stays so however long the model holds it). */
void net_sim_push(NetSim *s, uint64_t now, const NetAddr *from, const void *buf, int len, int flag);
/* The next datagram due by now into buf (cap bytes): its length, 0 for none; *flag as pushed. */
int net_sim_pop(NetSim *s, uint64_t now, NetAddr *from, void *buf, int cap, int *flag);
/* When the next one is due (UINT64_MAX: none waiting). */
uint64_t net_sim_next_due(const NetSim *s);

typedef struct NetSimStats {
    uint64_t in, dropped, duplicated, reordered, overflow;
} NetSimStats;
void net_sim_stats(const NetSim *s, NetSimStats *st);

#endif
