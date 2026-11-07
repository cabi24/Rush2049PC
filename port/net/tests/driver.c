/* port/net/tests/driver.c: the test peer's loop (driver.h). */
#include "driver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_os.h"


int drv_init(Driver *d, NetSession *s, int port, uint32_t ticks, uint64_t seed, int64_t period, uint64_t trace_every)
{
    memset(d, 0, sizeof *d);
    d->s = s;
    d->port = port;
    d->ticks = ticks;
    d->seed = seed;
    d->period = period;
    d->desync_at = d->quit_at = -1;
    d->trace_every = trace_every;
    toy_init(&d->toy);
    d->hashes = calloc(ticks + 1, sizeof *d->hashes);
    d->t_tick = calloc(ticks + 1, sizeof *d->t_tick);
    d->own = calloc(ticks + 1, sizeof *d->own);
    d->other = calloc(ticks + 1, sizeof *d->other);
    return d->hashes && d->t_tick && d->own && d->other ? 0 : -1;
}

void drv_free(Driver *d)
{
    free(d->hashes);
    free(d->t_tick);
    free(d->own);
    free(d->other);
}

/* After retrace n's pads came at `ready`: step, hash, pace. */
static void after(Driver *d, const NetPad pads[NET_MAX_PORTS], uint64_t ready)
{
    uint32_t n = d->n;
    toy_step(&d->toy, pads);
    if (d->desync_at >= 0 && (int64_t)n == d->desync_at)
        d->toy.b ^= 1;   /* a deliberate desync: one bit of this side's state */
    d->hashes[n] = toy_hash(&d->toy);
    d->own[n] = pads[d->port];
    d->other[n] = pads[d->port ^ 1];
    d->t_tick[n] = ready;
    net_hash(d->s, n, d->hashes[n]);
    if (n % 60 == 59)
        net_window_hash(d->s, n, toy_window_hash(&d->toy));
    int64_t adj = net_pace_adjust_ns(d->s);
    d->adjust_total += adj;
    if ((int64_t)(ready - d->deadline) > d->period / 2)
        d->deadline = ready + (uint64_t)d->period + (uint64_t)adj;   /* re-based after a stall (net.h) */
    else
        d->deadline += (uint64_t)d->period + (uint64_t)adj;
    if (d->trace_every && ready >= d->next_trace && d->ntrace < DRV_TRACE) {
        d->trace[d->ntrace++] = net_delay(d->s);
        d->next_trace = (d->next_trace ? d->next_trace : ready) + d->trace_every;
    }
    d->n++;
    d->submitted = 0;
    if (d->quit_at >= 0 && (int64_t)d->n == d->quit_at) {
        net_end(d->s, NET_END_LOCAL);
        d->done = d->ended = 1;
    }
    if (d->n >= d->ticks)
        d->done = 1;
}

static int submit(Driver *d)
{
    NetPad pad = toy_input(d->seed, d->port, d->n, d->unique);
    if (net_submit(d->s, d->n, &pad) != 0) {
        d->done = d->ended = 1;
        return -1;
    }
    d->submitted = 1;
    return 0;
}

int drv_catchup(Driver *d, uint64_t now)
{
    uint32_t live;
    NetPad pads[NET_MAX_PORTS];
    if (d->done || !net_catchup(d->s, &live) || d->n >= live)
        return 0;
    uint64_t t0 = net_os_now_ns();
    while (d->n < live) {
        uint32_t n = d->n;
        if (!net_catchup_pads(d->s, n, pads))
            return -1;
        toy_step(&d->toy, pads);
        if (d->desync_at >= 0 && (int64_t)n == d->desync_at)
            d->toy.b ^= 1;   /* the same deliberate desync: a deterministic one repeats in the replay */
        d->hashes[n] = toy_hash(&d->toy);
        d->own[n] = pads[d->port];
        d->other[n] = pads[d->port ^ 1];
        d->t_tick[n] = now;
        if (net_catchup_hash(d->s, n, d->hashes[n]) == 0 ||
            (n % 60 == 59 && net_catchup_window_hash(d->s, n, toy_window_hash(&d->toy)) == 0)) {
            d->n++;
            d->done = d->ended = 1;
            return -1;
        }
        d->n++;
        d->replayed++;
        if ((d->n & 255) == 0)
            net_catchup_at(d->s, d->n);
    }
    net_catchup_at(d->s, d->n);
    d->replay_ns = net_os_now_ns() - t0;
    d->deadline = now;   /* live from here: the next tick now */
    d->submitted = 0;
    if (d->n >= d->ticks)
        d->done = 1;
    return 1;
}

int drv_step(Driver *d, uint64_t now)
{
    NetPad pads[NET_MAX_PORTS];
    if (d->done)
        return 0;
    if (net_state(d->s) == NET_ENDED) {
        d->done = d->ended = 1;
        return 0;
    }
    if (!d->started) {
        if (!net_start_time(d->s, &d->deadline))
            return 0;
        d->started = 1;
        d->t_first = d->deadline;
    }
    if (drv_catchup(d, now) != 0)
        return 0;
    if (net_frozen(d->s))
        return 0;
    if (!d->submitted) {
        if (now < d->deadline)
            return 0;
        if (submit(d) != 0)
            return 0;
    }
    int r = net_pads(d->s, d->n, pads, 0);
    if (r < 0) {
        d->done = d->ended = 1;
        return 0;
    }
    if (r == 0)
        return 0;
    after(d, pads, now);
    return 1;
}

int drv_tick(Driver *d)
{
    NetPad pads[NET_MAX_PORTS];
    if (d->done)
        return 0;
    while (!d->started) {
        if (net_state(d->s) == NET_ENDED) {
            d->done = d->ended = 1;
            return -1;
        }
        if (net_start_time(d->s, &d->deadline)) {
            d->started = 1;
            d->t_first = d->deadline;
            break;
        }
        net_os_sleep_ns(1000000);
    }
    int c = drv_catchup(d, net_os_now_ns());
    if (c < 0)
        return -1;
    if (d->done)
        return 0;
    while (net_frozen(d->s))
        net_os_sleep_ns(1000000);
    net_os_sleep_until(d->deadline);
    while (net_frozen(d->s))
        net_os_sleep_ns(1000000);
    if (submit(d) != 0)
        return -1;
    for (;;) {
        int r = net_pads(d->s, d->n, pads, 5);
        if (r < 0) {
            d->done = d->ended = 1;
            return -1;
        }
        if (r > 0)
            break;
    }
    after(d, pads, net_os_now_ns());
    return d->ended ? -1 : 1;
}

double drv_rate(const Driver *d, uint32_t a, uint32_t b)
{
    if (b <= a || b >= d->n || d->t_tick[b] <= d->t_tick[a])
        return 0;
    return (double)(b - a) / ((double)(d->t_tick[b] - d->t_tick[a]) * 1e-9);
}

void drv_trace_str(const Driver *d, char *buf, int n)
{
    int at = 0;
    buf[0] = 0;
    for (int i = 0; i < d->ntrace && at < n - 8; i++) {
        if (i && d->trace[i] == d->trace[i - 1] && i + 1 < d->ntrace && d->trace[i + 1] == d->trace[i])
            continue;   /* runs shown as their first and last */
        at += snprintf(buf + at, (size_t)(n - at), "%s%d@%d", at ? "," : "", d->trace[i], i);
    }
}

int drv_check_assignment(const Driver *d, uint64_t *repeats, uint64_t *drops, char *why, int n)
{
    uint32_t prev = 0;
    int in_prefix = 1;
    *repeats = *drops = 0;
    for (uint32_t t = 0; t < d->n; t++) {
        uint32_t m = d->own[t].buttons | (uint32_t)(uint8_t)d->own[t].stick_y << 16;   /* sample m - 1; 0 idle */
        if (in_prefix) {
            if (m == 0)
                continue;
            if (m != 1) {
                snprintf(why, (size_t)n, "tick %u: the first sample is %u, not 0", t, m - 1);
                return -1;
            }
            in_prefix = 0;
            prev = m;
            continue;
        }
        if (m < prev || m - prev > 2) {
            snprintf(why, (size_t)n, "tick %u: sample %u after sample %u", t, m - 1, prev - 1);
            return -1;
        }
        if (m > t) {
            snprintf(why, (size_t)n, "tick %u: sample %u is used before it was taken", t, m - 1);
            return -1;
        }
        *repeats += m == prev;
        *drops += m - prev == 2;
        prev = m;
    }
    return 0;
}
