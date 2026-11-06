/* port/ultra/lockstep.c: the pads gate at each retrace, the pads sources behind it, the input delay line, record and
 * replay, and the gate's own test (docs/port/LOCKSTEP.md, pc_lockstep.h).
 *
 * The gate is in the main loop's path only with something behind it: a source (a replay file, a session), the record
 * file, the delay, the final pads' log or RUSH_SIM_GATE. Then, when retrace n is due and before anything else of the
 * retrace, the loop calls pc_pads_gate(n): this PC's pads are polled once (the window's events first, as the loop
 * does), go through the delay line, are handed to the source, and the four pads the game gets come back, or the gate
 * waits. The pads are latched at the retrace's place in the loop as before (pc_pads_take, then pc_si_set_pads).
 *
 * RUSH_SIM_GATE=SEED,P,MS: the gate's own test. At each retrace, with probability P (a seeded generator of its own),
 * the pads are held back for a random time up to MS ms of real time, and a thread of its own releases them and
 * signals the gate, as a network thread would on a packet; the trace and the hashes must be those of a run without it
 * (ctest gate_same). */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pc_lockstep.h"
#include "ultra.h"

void pc_hash_set_listener(void (*fn)(void *, uint32_t, const PcStateHash *), void *user);   /* statehash.c */

/* ---------------------------------------------------------------- the delay line */

int pc_delay_parse(const char *spec, PcDelayStep *steps, int max, char *why, size_t why_size)
{
    int n = 0;
    const char *p = spec;
    if (!spec || !*spec) {
        snprintf(why, why_size, "no delay given");
        return -1;
    }
    while (*p) {
        char *end;
        long d = strtol(p, &end, 10), from = 0;
        if (end == p || d < 0 || d > PC_DELAY_MAX) {
            snprintf(why, why_size, "'%s': a delay is 0 to %d retraces", spec, PC_DELAY_MAX);
            return -1;
        }
        p = end;
        if (*p == ':') {
            from = strtol(p + 1, &end, 10);
            if (end == p + 1 || from < 0) {
                snprintf(why, why_size, "'%s': D:FROM takes the retrace it starts at", spec);
                return -1;
            }
            p = end;
        }
        if (n == max) {
            snprintf(why, why_size, "'%s': at most %d steps", spec, max);
            return -1;
        }
        if ((n == 0 && from != 0) || (n > 0 && (uint32_t)from <= steps[n - 1].from)) {
            snprintf(why, why_size, "'%s': the first step is from retrace 0, and each from a later one", spec);
            return -1;
        }
        steps[n++] = (PcDelayStep){(uint32_t)from, (int)d};
        if (*p == ',')
            p++;
        else if (*p) {
            snprintf(why, why_size, "'%s': D, or D:FROM,D:FROM,...", spec);
            return -1;
        }
    }
    return n;
}

int pc_delay_at(const PcDelayStep *steps, int n, uint32_t retrace)
{
    int d = 0;
    for (int i = 0; i < n && steps[i].from <= retrace; i++)
        d = steps[i].delay;
    return d;
}

void pc_delay_reset(PcDelayLine *d)
{
    memset(d, 0, sizeof *d);
    d->last = -1;
}

int pc_delay_put(PcDelayLine *d, uint32_t n, int delay, const PcPad sample[4])
{
    int64_t t = (int64_t)n + (delay < 0 ? 0 : delay > PC_DELAY_MAX ? PC_DELAY_MAX : delay);
    if (t <= d->last) {   /* the delay shrank: this sample's slot is taken */
        d->dropped++;
        return 0;
    }
    if (d->last < 0) {   /* idle before the first sample, with its controllers plugged in */
        for (int p = 0; p < 4; p++)
            d->last_pads[p] = (PcPad){0, 0, 0, sample[p].connected, sample[p].rumble};
    } else {
        d->repeated += (uint32_t)(t - d->last - 1);   /* it grew: the skipped slots get the last pads again */
    }
    for (int64_t s = d->last + 1; s < t; s++)
        memcpy(d->slot[s % PC_DELAY_RING], d->last_pads, sizeof d->last_pads);
    memcpy(d->slot[t % PC_DELAY_RING], sample, 4 * sizeof *sample);
    memcpy(d->last_pads, sample, sizeof d->last_pads);
    d->last = t;
    return 1;
}

int pc_delay_get(const PcDelayLine *d, uint32_t n, PcPad out[4])
{
    if ((int64_t)n > d->last || (int64_t)n <= d->last - PC_DELAY_RING)
        return -1;
    memcpy(out, d->slot[n % PC_DELAY_RING], 4 * sizeof *out);
    return 0;
}

/* ---------------------------------------------------------------- the gate */

static struct {
    int gated;
    const PcPadsSource *src;
    /* the delay */
    int delay_on;
    PcDelayStep steps[PC_DELAY_STEPS];
    int nsteps;
    PcDelayLine line;
    /* this retrace */
    uint32_t n;
    int polled, ready, waiting;
    uint64_t wait_from;
    PcPad local[4], given[4], out[4];
    /* the signal */
    pthread_mutex_t m;
    pthread_cond_t cv;
    uint64_t seq;
    PcPadsStats stats;
    /* the record file, the final pads' log */
    PcPadWriter *rec;
    int log_pads;
    PcPad logged[4];
    int logged_any;
} G = {.m = PTHREAD_MUTEX_INITIALIZER};

static void init_cv(void)
{
    static int done;
    if (done)
        return;
    pthread_condattr_t a;
    pthread_condattr_init(&a);
    pthread_condattr_setclock(&a, CLOCK_MONOTONIC);
    pthread_cond_init(&G.cv, &a);
    pthread_condattr_destroy(&a);
    done = 1;
}

int pc_pads_set_source(const PcPadsSource *source)
{
    if (source && G.src && source != G.src)
        return -1;
    init_cv();
    G.src = source;
    G.gated |= source != NULL;
    pc_hash_set_listener(source ? source->hash : NULL, source ? source->user : NULL);
    return 0;
}

void pc_pads_signal(void)
{
    init_cv();
    pthread_mutex_lock(&G.m);
    G.seq++;
    pthread_cond_broadcast(&G.cv);
    pthread_mutex_unlock(&G.m);
}

void pc_pads_stats(PcPadsStats *out)
{
    *out = G.stats;
}

int pc_pads_gated(void)
{
    return G.gated;
}

/* RUSH_SIM_GATE (above) */
static struct {
    int on;
    uint64_t rng;
    double prob, max_ms;
    volatile int holding;
    uint64_t release_at;      /* real ns */
    pthread_mutex_t m;
    pthread_cond_t cv;
    uint32_t held;
    uint64_t held_ns;
} S = {.m = PTHREAD_MUTEX_INITIALIZER};

static uint64_t splitmix(uint64_t *x)
{
    uint64_t z = (*x += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

static void *sim_thread(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&S.m);
    for (;;) {
        while (!__atomic_load_n(&S.holding, __ATOMIC_ACQUIRE))
            pthread_cond_wait(&S.cv, &S.m);
        uint64_t at = S.release_at;
        pthread_mutex_unlock(&S.m);
        for (uint64_t now = pc_real_ns(); now < at; now = pc_real_ns()) {
            struct timespec ts = {(time_t)((at - now) / 1000000000u), (long)((at - now) % 1000000000u)};
            nanosleep(&ts, NULL);
        }
        pthread_mutex_lock(&S.m);
        __atomic_store_n(&S.holding, 0, __ATOMIC_RELEASE);
        pc_pads_signal();   /* as a network thread on a packet */
    }
    return NULL;
}

static int sim_setup(void)
{
    const char *e = getenv("RUSH_SIM_GATE");
    unsigned long long seed;
    double prob, ms;
    pthread_t t;
    if (!e || !*e)
        return 0;
    if (sscanf(e, "%llu,%lf,%lf", &seed, &prob, &ms) != 3 || prob < 0 || prob > 1 || ms < 0) {
        fprintf(stderr, "rush2049pc: RUSH_SIM_GATE takes SEED,PROBABILITY,MAX_MS, not '%s'\n", e);
        return -1;
    }
    S.rng = seed, S.prob = prob, S.max_ms = ms;
    pthread_condattr_t a;
    pthread_condattr_init(&a);
    pthread_cond_init(&S.cv, &a);
    pthread_condattr_destroy(&a);
    if (pthread_create(&t, NULL, sim_thread, NULL) != 0) {
        fprintf(stderr, "rush2049pc: RUSH_SIM_GATE: no thread\n");
        return -1;
    }
    pthread_detach(t);
    S.on = 1;
    fprintf(stderr, "rush2049pc: test: the pads held back at a retrace in %g for up to %g ms (RUSH_SIM_GATE, seed %llu)\n",
            prob, ms, seed);
    return 0;
}

static void sim_draw(void)
{
    double u = (double)(splitmix(&S.rng) >> 11) / 9007199254740992.0;
    double w = (double)(splitmix(&S.rng) >> 11) / 9007199254740992.0 * S.max_ms;
    if (u >= S.prob)
        return;
    pthread_mutex_lock(&S.m);
    S.release_at = pc_real_ns() + (uint64_t)(w * 1e6);
    S.held++;
    S.held_ns += (uint64_t)(w * 1e6);
    __atomic_store_n(&S.holding, 1, __ATOMIC_RELEASE);
    pthread_cond_signal(&S.cv);
    pthread_mutex_unlock(&S.m);
}

static void log_pads(uint32_t n, const PcPad p[4])
{
    char line[256];
    int len;
    if (G.logged_any && !memcmp(p, G.logged, sizeof G.logged))
        return;
    len = snprintf(line, sizeof line, "rush2049pc: pads vi=%u:", n);
    for (int i = 0; i < 4 && len > 0 && (size_t)len < sizeof line; i++)
        len += snprintf(line + len, sizeof line - (size_t)len, " %d %04x %+d %+d%s%s;", i + 1, p[i].buttons,
                        p[i].stick_x, p[i].stick_y, p[i].connected ? " c" : "", p[i].rumble ? " r" : "");
    fprintf(stderr, "%s\n", line);
    for (int i = 0; i < 4; i++)   /* only the fields the game reads, so padding never counts as a change */
        G.logged[i] = (PcPad){p[i].buttons, p[i].stick_x, p[i].stick_y, p[i].connected, p[i].rumble};
    G.logged_any = 1;
}

/* Waits up to PC_PADS_POLL_MS for a signal after seq. */
static void wait_signal(uint64_t seq)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_nsec += PC_PADS_POLL_MS * 1000000L;
    if (ts.tv_nsec >= 1000000000L)
        ts.tv_sec++, ts.tv_nsec -= 1000000000L;
    pthread_mutex_lock(&G.m);
    if (G.seq == seq)
        pthread_cond_timedwait(&G.cv, &G.m, &ts);
    pthread_mutex_unlock(&G.m);
}

int pc_pads_gate(uint32_t n)
{
    uint64_t seq;
    int r;
    if (!G.polled || G.n != n) {   /* the retrace's first pass: this PC's pads, once */
        G.n = n, G.polled = 1, G.ready = 0, G.waiting = 0;
        if (pc_gfx_poll_events())   /* first, so the pads below are this retrace's (as the loop does) */
            pc_quit = 1;
        pc_input_poll(G.local);
        memcpy(G.given, G.local, sizeof G.given);
        if (G.delay_on) {
            pc_delay_put(&G.line, n, pc_delay_at(G.steps, G.nsteps, n), G.local);
            pc_delay_get(&G.line, n, G.given);   /* always there: a sample goes to its retrace or later */
        }
        if (G.src && G.src->local)
            G.src->local(G.src->user, n, G.given);
        if (S.on)
            sim_draw();
    }
    if (G.ready)
        return 0;
    pthread_mutex_lock(&G.m);
    seq = G.seq;
    pthread_mutex_unlock(&G.m);
    if (S.on && __atomic_load_n(&S.holding, __ATOMIC_ACQUIRE))
        r = PC_PADS_WAIT;
    else if (G.src)
        r = G.src->pads(G.src->user, n, G.out);
    else
        memcpy(G.out, G.given, sizeof G.out), r = PC_PADS_READY;
    if (r == PC_PADS_READY) {
        G.ready = 1;
        G.stats.retraces++;
        if (G.waiting) {
            uint64_t w = pc_real_ns() - G.wait_from;
            G.stats.wait_ns += w;
            if (w > G.stats.longest_ns)
                G.stats.longest_ns = w;
        }
        if (G.rec && pc_padfile_write(G.rec, n, G.out) != 0) {
            pc_padfile_close(G.rec);
            G.rec = NULL;
        }
        if (G.log_pads)
            log_pads(n, G.out);
        return 0;
    }
    if (!G.waiting) {   /* not known yet: the N64 clock stays, nothing goes to the game */
        G.waiting = 1;
        G.wait_from = pc_real_ns();
        G.stats.waited++;
    }
    wait_signal(seq);
    if (pc_gfx_poll_events())   /* the window stays alive, and quitting works */
        pc_quit = 1;
    return 1;
}

int pc_pads_take(uint32_t n, PcPad pads[4])
{
    if (!G.gated || !G.ready || G.n != n)
        return 0;
    memcpy(pads, G.out, sizeof G.out);
    return 1;
}

/* ---------------------------------------------------------------- the replay source */

static struct {
    PcPadFile file;
    const char *path;
    int ended;
    PcPadsSource src;
} R;

static int replay_pads(void *user, uint32_t n, PcPad out[4])
{
    (void)user;
    uint32_t have = R.file.bad;
    if (n < have) {
        pc_padfile_record(&R.file, n, out);
        return PC_PADS_READY;
    }
    if (have)   /* past its end: idle, with the last record's controllers */
        pc_padfile_record(&R.file, have - 1, out);
    for (int p = 0; p < 4; p++) {
        out[p].buttons = 0, out[p].stick_x = out[p].stick_y = 0;
        if (!have)
            out[p].connected = p == 0, out[p].rumble = 0;
    }
    if (!R.ended) {
        R.ended = 1;
        fprintf(stderr, "rush2049pc: replay: %s ends at retrace %u%s; idle pads from here\n", R.path, have,
                R.file.bad < R.file.count ? " (a record's check failed there: the file is damaged)"
                : R.file.tail ? " (cut short: a record's last bytes are missing)" : "");
    }
    return PC_PADS_READY;
}

/* ---------------------------------------------------------------- the options */

int pc_lockstep_setup(const PcLockstepOptions *o)
{
    char why[512];
    init_cv();
    if (o->replay) {
        static char diff[4096];
        int differ;
        if (o->delay) {
            fprintf(stderr, "rush2049pc: --input-delay delays this PC's pads, which --replay-inputs doesn't use\n");
            return 2;
        }
        if (pc_padfile_read(o->replay, &R.file, why, sizeof why) != 0) {
            fprintf(stderr, "rush2049pc: --replay-inputs: %s\n", why);
            return 2;
        }
        R.path = o->replay;
        differ = pc_padfile_mismatch(R.file.must_match, o->must_match ? o->must_match : "", diff, sizeof diff);
        if (differ && !o->replay_force) {
            fprintf(stderr, "rush2049pc: --replay-inputs: %s was recorded by another build, cartridge or settings, so "
                    "its pads wouldn't play the same game here. What differs (the file's, then this run's):\n%s"
                    "rush2049pc: --replay-force replays it anyway\n", o->replay, diff);
            return 2;
        }
        if (differ)
            fprintf(stderr, "rush2049pc: replay: %d of what a replay must match differ%s (--replay-force):\n%s",
                    differ, differ == 1 ? "s" : "", diff);
        fprintf(stderr, "rush2049pc: replay: %u retraces of pads from %s%s\n", R.file.bad, o->replay,
                R.file.bad < R.file.count || R.file.tail ? " (the file is cut short or damaged after them)" : "");
        R.src = (PcPadsSource){"replay", NULL, NULL, replay_pads, NULL};
        pc_pads_set_source(&R.src);
    }
    if (o->delay) {
        G.nsteps = pc_delay_parse(o->delay, G.steps, PC_DELAY_STEPS, why, sizeof why);
        if (G.nsteps < 0) {
            fprintf(stderr, "rush2049pc: --input-delay: %s\n", why);
            return 2;
        }
        pc_delay_reset(&G.line);
        G.delay_on = 1;
        fprintf(stderr, "rush2049pc: input delay %s (retraces; docs/port/LOCKSTEP.md 4)\n", o->delay);
    }
    if (o->record) {
        char *text = malloc(strlen(o->must_match ? o->must_match : "") + 512);   /* and what is only said */
        if (!text)
            return 2;
        sprintf(text, "%s# clock: %s\n# input delay: %.64s\n# replayed from: %.256s\n",
                o->must_match ? o->must_match : "", pc_time_virtual() ? "virtual (--fast)" : "real time",
                o->delay ? o->delay : "0", o->replay ? o->replay : "none");
        G.rec = pc_padfile_create(o->record, text, why, sizeof why);
        free(text);
        if (!G.rec) {
            fprintf(stderr, "rush2049pc: --record-inputs: %s\n", why);
            return 2;
        }
    }
    if (sim_setup() != 0)
        return 2;
    G.log_pads = o->log_pads;
    G.gated |= G.delay_on || G.rec || S.on || G.log_pads;
    return 0;
}

void pc_lockstep_finish(void)
{
    if (G.rec)
        pc_padfile_close(G.rec);
    G.rec = NULL;
    if (G.delay_on)
        fprintf(stderr, "rush2049pc: input delay: %u samples dropped as it shrank, %u retraces repeated as it grew\n",
                G.line.dropped, G.line.repeated);
    if (S.on)
        fprintf(stderr, "rush2049pc: test: the pads held back at %u retraces, %.1f ms in all (RUSH_SIM_GATE)\n",
                S.held, S.held_ns / 1e6);
    if (G.gated)
        fprintf(stderr, "rush2049pc: pads gate: %u retraces, %u waited for their pads (%.1f ms in all, the longest "
                "%.1f ms)\n", G.stats.retraces, G.stats.waited, G.stats.wait_ns / 1e6, G.stats.longest_ns / 1e6);
}
