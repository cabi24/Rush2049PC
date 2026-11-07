/* port/net/tests/driver.h: a peer's game loop around the toy simulation, as the game's main loop will use port/net:
 * at retrace n submit the pads sampled now, wait for all pads of n, step, hash, and pace the next retrace by the
 * retrace period plus net_pace_adjust_ns (re-based after a stall of over half a retrace, net.h). On a virtual clock it steps
 * without blocking (drv_step); in real time it sleeps and waits (drv_tick). */
#ifndef NET_DRIVER_H
#define NET_DRIVER_H

#include <stdint.h>

#include "net.h"
#include "toy.h"

#define DRV_TRACE 4096

typedef struct Driver {
    NetSession *s;
    int port;          /* the port this peer owns (0 host, 1 guest) */
    uint64_t seed;
    int unique;        /* inputs that carry their retrace (the assignment check) */
    Toy toy;
    uint32_t n, ticks;
    int submitted, started, done, ended;
    int64_t period;
    uint64_t deadline, t_first, *t_tick;   /* t_tick[n]: when tick n ran */
    uint32_t *hashes;
    NetPad *own, *other;   /* the merged pads of its own port and of the peer's, per tick */
    int64_t desync_at, quit_at;
    /* the delay over time */
    uint64_t trace_every, next_trace;
    int trace[DRV_TRACE], ntrace;
    int64_t adjust_total;
    uint32_t replayed;      /* ticks replayed from the keeper's log (a rejoin) */
    uint64_t replay_ns;     /* the replay's time on the session's clock */
} Driver;

int drv_init(Driver *d, NetSession *s, int port, uint32_t ticks, uint64_t seed, int64_t period, uint64_t trace_every);
void drv_free(Driver *d);
/* A rejoiner (net_catchup): replay the keeper's log from tick 0 to its live tick, each tick's hash checked against
 * the keeper's (net_catchup_hash, the window hash every 60), as fast as it goes. 1 when it replayed, 0 nothing to
 * replay, -1 the replay differed (the session ended). drv_step and drv_tick do it first by themselves. */
int drv_catchup(Driver *d, uint64_t now);
/* Virtual clock: do what is due at now without waiting; 1 when a tick ran. */
int drv_step(Driver *d, uint64_t now);
/* Real time: run the next tick (sleep to its deadline, wait for its pads). 1 ran, 0 done, -1 the session ended. */
int drv_tick(Driver *d);
/* Ticks a second between ticks a and b. */
double drv_rate(const Driver *d, uint32_t a, uint32_t b);
/* The delay trace as text. */
void drv_trace_str(const Driver *d, char *buf, int n);
/* The assignment check (unique inputs): its own port's pads are its samples in order, a sample repeated when the
 * delay grew, one skipped when it shrank. 0 when they are; the counts found. */
int drv_check_assignment(const Driver *d, uint64_t *repeats, uint64_t *drops, char *why, int n);

#endif
