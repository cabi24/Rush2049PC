/* port/ultra/pace.c: paced mode, the virtual N64 clock paced to the wall clock (docs/port/ULTRA.md 10).
 *
 * Online play is lockstep on the virtual clock --fast uses (docs/port/NETPLAY.md 1.2): the N64 side moves only while
 * every N64 thread waits, and only by N64 time, so two PCs with the same inputs take the same course. --fast moves
 * the clock to its next deadline at once. Paced mode moves it there only once the wall clock has reached that
 * deadline's wall time too (sched.c's pc_os_wait_main_paced), so the game plays in real time and its course is still
 * --fast's, to the bit: the wall clock decides when the N64 side moves, never what it does.
 *
 * The map: N64 time v is due at wall time epoch + v, on pc_real_ns's clock (CLOCK_MONOTONIC; winpthreads' is
 * QueryPerformanceCounter). The epoch is set when the loop starts (the N64 time then due then), or to an agreed
 * session start (pc_pace_set_start: N64 time 0 at that wall time, so retrace N at start + N retraces). It moves with:
 *   - a hold that says so (loop.c's hold(): the pause key, focus lost, the F1 overlay): later by the time held;
 *   - a lag, from anything that holds the clock without saying so (a slow host, a debugger, a gate waiting for a
 *     remote player's input, a thread computing long, a present that blocks): a move whose wall time has gone by is
 *     made at once. A lag of up to two retraces is caught up smoothly, each retrace at most a quarter shorter in wall
 *     time than a retrace (12.5 ms apart, not a burst; with coarse waits at once); a longer one re-bases, the epoch
 *     moved so that the move is due now. Both are counted, and the exit line says them;
 *   - time sync (the network layer, from any thread): pc_pace_adjust_ns makes every retrace up to 1 ms longer or
 *     shorter, pc_pace_shift_ns moves the epoch once;
 *   - the vblank lock (loop.c's vb_lock): where real time moves the retraces against the display's vblanks, paced
 *     mode moves the epoch instead, since the retraces are N64 time.
 *
 * RUSH_PACE_SPEED=S (tests): N64 time runs S times as fast as the wall clock (the map is epoch + v / S), so a test
 * runs the paced code in a fraction of the game's time. 1 by default. */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ultra.h"

#define VI_NS (1000000000ull / PC_VI_HZ)
#define PACE_REBASE_NS ((int64_t)(2 * VI_NS))   /* a lag past two retraces re-bases */
#define PACE_SMOOTH_NS 2000000                   /* a lag past this is caught up a quarter fast at most */
#define PACE_ADJUST_MAX 1000000                  /* pc_pace_adjust_ns's cap, a retrace */
#define WORK_BIN_NS 250000                       /* the work histogram's bins: 0.25 ms, to 50 ms */
#define WORK_BINS 200

static pthread_mutex_t g_plock = PTHREAD_MUTEX_INITIALIZER;
static int g_paced;

static struct {
    int started;             /* the epoch is set */
    int64_t epoch;           /* wall ns of N64 time 0 */
    double speed;            /* RUSH_PACE_SPEED */
    int64_t adjust;          /* wall ns added to each retrace (pc_pace_adjust_ns) */
    int64_t adjusted;        /* all it added */
    int64_t shifted;         /* pc_pace_shift_ns's and the vblank lock's, in all */
    uint32_t shifts;
    int moved;               /* the clock has moved since the start */
    uint64_t last_v, last_w; /* the last move: to this N64 time, at this wall time */
    int64_t lag;             /* how late the last move came after its wall time */
    int64_t lag_max;
    uint32_t retrace;        /* the last retrace that went out */
    uint64_t retrace_v;      /* its N64 time */
    uint64_t retrace_due;    /* its wall time */
    uint64_t retrace_w;      /* the wall time its move came */
    uint32_t first_retrace;  /* the first retrace counted for the rate, and when it came */
    uint64_t first_w;
    uint32_t late;           /* retraces that came more than PACE_SMOOTH_NS late, caught up */
    uint32_t holds, rebases;
    uint64_t held_ns, rebased_ns;
    int64_t rebase_max;
    int64_t rebased_since;   /* the longest lag re-based since the last retrace */
    uint64_t wake_late;      /* how late the paced wait's timed wakes return (a decaying maximum) */
    /* the host's work a retrace: the wall time between two retraces less what the main thread slept with every N64
     * thread waiting (sched.c's slack), in WORK_BIN_NS bins; not across a hold, a re-base or a time-sync shift */
    uint64_t slack_at;       /* the slack at the last retrace */
    int work_skip;           /* the interval since the last retrace isn't a retrace's own */
    uint32_t work_hist[WORK_BINS], work_n;
    uint64_t work_sum, work_max;
    struct {                 /* the same since the last periodic line (pc_pace_periodic) */
        uint32_t hist[WORK_BINS], n, rebases, late;
        uint64_t sum, max, rebased_ns;
        int64_t lag_max;
        uint64_t at;         /* its wall time */
    } span;
} P = {.speed = 1.0};

/* ---------------------------------------------------------------- the predicates */

void pc_time_set_paced(int on)
{
    g_paced = on != 0;
    if (on)
        pc_time_set_virtual(1);
}

int pc_time_paced(void) { return g_paced; }

int pc_time_free(void) { return pc_time_virtual() && !g_paced; }

/* ---------------------------------------------------------------- the map (g_plock held) */

/* N64 ns as wall ns. */
static int64_t span(uint64_t n64_ns)
{
    return P.speed == 1.0 ? (int64_t)n64_ns : (int64_t)((double)n64_ns / P.speed);
}

/* The N64 time now is due now, unless the epoch is set already (pc_pace_set_start). */
static void start_locked(uint64_t now)
{
    if (!P.started) {
        P.epoch = (int64_t)now - span(pc_host_ns());
        P.started = 1;
    }
}

static int64_t due_locked(uint64_t n64_ns)
{
    return P.epoch + span(n64_ns);
}

/* ---------------------------------------------------------------- the main loop's and sched.c's */

uint64_t pc_pace_wall_ns(void) { return pc_real_ns(); }

/* loop.c, before its first pass: the speed, and the epoch unless the session set it. */
void pc_pace_begin(void)
{
    const char *s = getenv("RUSH_PACE_SPEED");
    pthread_mutex_lock(&g_plock);
    if (s && atof(s) > 0)
        P.speed = atof(s);
    start_locked(pc_real_ns());
    int64_t ahead = P.epoch + span(pc_host_ns()) - (int64_t)pc_real_ns();
    double speed = P.speed;
    pthread_mutex_unlock(&g_plock);
    char how[96] = "";
    if (speed != 1.0)
        snprintf(how, sizeof how, " %g times as fast (RUSH_PACE_SPEED)", speed);
    fprintf(stderr, "rush2049pc: paced: the virtual N64 clock, paced to the wall clock%s%s", how,
            ahead > 1000000 ? "; the start in " : "\n");
    if (ahead > 1000000)
        fprintf(stderr, "%.3f s\n", ahead / 1e9);
}

/* The wall time N64 time n64_ns is due at, as the map stands now. */
uint64_t pc_pace_wall_of(uint64_t n64_ns)
{
    pthread_mutex_lock(&g_plock);
    start_locked(pc_real_ns());
    int64_t w = due_locked(n64_ns);
    pthread_mutex_unlock(&g_plock);
    return w > 0 ? (uint64_t)w : 0;
}

/* Wall ns for a span of N64 ns (RUSH_PACE_SPEED's). */
uint64_t pc_pace_span(uint64_t n64_ns)
{
    pthread_mutex_lock(&g_plock);
    int64_t w = span(n64_ns);
    pthread_mutex_unlock(&g_plock);
    return (uint64_t)w;
}

/* sched.c, with every N64 thread waiting: when the clock may move to N64 time d, the wall clock being at now. Past
 * now: wait until then. A lag past two retraces re-bases here (the move is then due now); a smaller one past
 * PACE_SMOOTH_NS is caught up by retraces at most a quarter of a retrace short. */
uint64_t pc_pace_due(uint64_t d, uint64_t now)
{
    if (d <= pc_host_ns())   /* not a move (a deadline due already): nothing to pace, no lag to see */
        return now;
    pthread_mutex_lock(&g_plock);
    start_locked(now);
    int64_t due = due_locked(d), lag = (int64_t)now - due;
    if (lag > PACE_REBASE_NS) {
        P.epoch += lag;
        P.rebases++;
        P.rebased_ns += (uint64_t)lag;
        if (lag > P.rebase_max)
            P.rebase_max = lag;
        if (lag > P.rebased_since)
            P.rebased_since = lag;
        P.span.rebases++;
        P.span.rebased_ns += (uint64_t)lag;
        due = (int64_t)now;
    } else if (lag > PACE_SMOOTH_NS && P.first_w && d == P.retrace_v + VI_NS && P.wake_late < PACE_SMOOTH_NS) {
        /* The next retrace, late: no sooner than three quarters of a retrace after the last one came, so a lag is
         * caught up by a quarter of a retrace a retrace (12.5 ms apart, not a burst). Only the retraces: the moves
         * between them (the AI, timers, a busy-wait's naps) come as soon as they can, and the floor counts from the
         * last retrace's own time, so a delay of the loop's (a blocking swap) never adds to the next. Not with coarse
         * waits (RUSH_SIM_TIMER_STEP_US's, a Windows without the high-resolution timer): a sleep to the floor would
         * end a whole step late; there the lag is caught up at once. */
        int64_t floor = (int64_t)P.retrace_w + span(VI_NS) - span(VI_NS) / 4;
        if (floor > due)
            due = floor;
    }
    pthread_mutex_unlock(&g_plock);
    return due > 0 ? (uint64_t)due : 0;
}

/* sched.c: a timed wake of the paced wait came late_ns late. */
void pc_pace_wake_late(uint64_t late_ns)
{
    pthread_mutex_lock(&g_plock);
    uint64_t keep = P.wake_late - P.wake_late / 32;
    P.wake_late = late_ns > keep ? late_ns : keep;
    pthread_mutex_unlock(&g_plock);
}

/* sched.c: the clock moved to N64 time d at wall time now. */
void pc_pace_moved(uint64_t d, uint64_t now)
{
    if (d <= pc_host_ns())   /* the clock stays */
        return;
    pthread_mutex_lock(&g_plock);
    int64_t lag = (int64_t)now - due_locked(d);
    P.lag = lag > 0 ? lag : 0;
    if (P.lag > P.lag_max)
        P.lag_max = P.lag;
    if (P.lag > P.span.lag_max)
        P.span.lag_max = P.lag;
    P.last_v = d;
    P.last_w = now;
    P.moved = 1;
    pthread_mutex_unlock(&g_plock);
}

/* loop.c: retrace frame went out, at N64 time n64_ns. The adjustment lengthens the retrace that follows. Returns how
 * late its move came after its wall time, or the longest lag re-based since the last retrace (the host behind). */
int64_t pc_pace_retrace(uint32_t frame, uint64_t n64_ns)
{
    pthread_mutex_lock(&g_plock);
    P.retrace = frame;
    P.retrace_v = n64_ns;
    int64_t due = due_locked(n64_ns);
    P.retrace_due = due > 0 ? (uint64_t)due : 0;
    uint64_t prev_w = P.retrace_w, slack = pc_os_paced_slack_ns();
    P.retrace_w = P.moved && P.last_v == n64_ns ? P.last_w : pc_real_ns();
    P.late += (int64_t)P.retrace_w - due > PACE_SMOOTH_NS;
    P.span.late += (int64_t)P.retrace_w - due > PACE_SMOOTH_NS;
    if (!P.first_w) {
        P.first_retrace = frame;
        P.first_w = P.retrace_w;
    } else if (!P.work_skip && !P.rebased_since && P.retrace_w > prev_w) {
        uint64_t gap = P.retrace_w - prev_w, s = slack - P.slack_at, work = gap > s ? gap - s : 0;
        uint64_t bin = work / WORK_BIN_NS;
        P.work_hist[bin < WORK_BINS ? bin : WORK_BINS - 1]++;
        P.work_n++;
        P.work_sum += work;
        if (work > P.work_max)
            P.work_max = work;
        P.span.hist[bin < WORK_BINS ? bin : WORK_BINS - 1]++;
        P.span.n++;
        P.span.sum += work;
        if (work > P.span.max)
            P.span.max = work;
    }
    P.slack_at = slack;
    P.work_skip = 0;
    P.epoch += P.adjust;
    P.adjusted += P.adjust;
    int64_t late = (int64_t)P.retrace_w - due;
    if (P.rebased_since > late)
        late = P.rebased_since;
    P.rebased_since = 0;
    pthread_mutex_unlock(&g_plock);
    return late;
}

/* loop.c's hold(): the game was held for wall_ns; the map moves on by as much. */
void pc_pace_hold(uint64_t wall_ns)
{
    pthread_mutex_lock(&g_plock);
    P.epoch += (int64_t)wall_ns;
    P.holds++;
    P.held_ns += wall_ns;
    P.work_skip = 1;
    pthread_mutex_unlock(&g_plock);
}

int64_t pc_pace_lag_ns(void)
{
    pthread_mutex_lock(&g_plock);
    int64_t lag = P.lag;
    pthread_mutex_unlock(&g_plock);
    return lag;
}

/* ---------------------------------------------------------------- time sync (any thread) */

void pc_pace_set_start(uint64_t wall_ns)
{
    pthread_mutex_lock(&g_plock);
    P.epoch = (int64_t)wall_ns;
    P.started = 1;
    pthread_mutex_unlock(&g_plock);
    pc_os_nudge_main();
}

void pc_pace_shift_ns(int64_t ns)
{
    pthread_mutex_lock(&g_plock);
    P.epoch += ns;
    P.shifted += ns;
    P.shifts++;
    P.work_skip = 1;
    pthread_mutex_unlock(&g_plock);
    pc_os_nudge_main();
}

int64_t pc_pace_adjust_ns(int64_t per_retrace)
{
    if (per_retrace > PACE_ADJUST_MAX)
        per_retrace = PACE_ADJUST_MAX;
    if (per_retrace < -PACE_ADJUST_MAX)
        per_retrace = -PACE_ADJUST_MAX;
    pthread_mutex_lock(&g_plock);
    P.adjust = per_retrace;
    pthread_mutex_unlock(&g_plock);
    return per_retrace;
}

void pc_pace_info(PcPaceInfo *out)
{
    pthread_mutex_lock(&g_plock);
    out->paced = g_paced;
    out->retrace = P.retrace;
    out->retrace_due_ns = P.retrace_due;
    out->retrace_sent_ns = P.retrace_w;
    int64_t next = due_locked(P.retrace_v + VI_NS);   /* the retrace's adjustment is in the epoch already */
    out->next_due_ns = next > 0 ? (uint64_t)next : 0;
    out->lag_ns = P.lag;
    out->adjust_ns = P.adjust;
    out->holds = P.holds;
    out->rebases = P.rebases;
    pthread_mutex_unlock(&g_plock);
}

/* ---------------------------------------------------------------- the exit line */

/* The work a retrace from a histogram: mean, median, 99th percentile (g_plock held). */
static void work_line(char *out, size_t size, const uint32_t *hist, uint32_t n, uint64_t sum, uint64_t max)
{
    uint32_t k = 0, acc = 0, p99 = 0, p50 = 0;
    for (; k < WORK_BINS; k++) {
        acc += hist[k];
        if (!p50 && acc * 2 >= n)
            p50 = k + 1;
        if (acc * 100 >= n * 99u) {
            p99 = k + 1;
            break;
        }
    }
    double r = span(VI_NS) / 1e6, mean = n ? sum / 1e6 / n : 0;
    snprintf(out, size, "of %.2f ms: mean %.2f ms (%.0f%%), half under %.2f ms, 99%% under %.2f ms, the most %.2f ms, "
             "over %u retraces", r, mean, 100.0 * mean / r, p50 * WORK_BIN_NS / 1e6, p99 * WORK_BIN_NS / 1e6, max / 1e6,
             n);
}

/* loop.c, about once a second: with -v a line every 10 s of wall time, as the audio's (a long run's record). */
void pc_pace_periodic(void)
{
    char work[256];
    uint64_t now = pc_real_ns();
    pthread_mutex_lock(&g_plock);
    if (!P.span.at)
        P.span.at = now;
    if (!pc_verbose || now - P.span.at < 10000000000u || !P.span.n) {
        pthread_mutex_unlock(&g_plock);
        return;
    }
    work_line(work, sizeof work, P.span.hist, P.span.n, P.span.sum, P.span.max);
    fprintf(stderr, "rush2049pc: paced, the last %.1f s of wall time to retrace %u: the host's work a retrace %s; %u "
            "re-based (%.3f s), %u retraces caught up a lag (the most %.1f ms)\n", (now - P.span.at) / 1e9, P.retrace,
            work, P.span.rebases, P.span.rebased_ns / 1e9, P.span.late, P.span.lag_max / 1e6);
    memset(&P.span, 0, sizeof P.span);
    P.span.at = now;
    pthread_mutex_unlock(&g_plock);
}

void pc_pace_report(void)
{
    pthread_mutex_lock(&g_plock);
    uint32_t n = P.retrace - P.first_retrace;
    double wall = P.first_w && P.retrace_w > P.first_w ? (P.retrace_w - P.first_w) / 1e9 : 0;
    double gaps = (P.held_ns + P.rebased_ns) / 1e9;
    double rate = wall > 0 ? n / wall : 0, paced = wall - gaps > 0 ? n / (wall - gaps) : 0;
    fprintf(stderr, "rush2049pc: paced: %u retraces in %.3f s of wall time, %.3f a second (%.3f without the %.3f s "
            "held or re-based); %u holds (%.3f s), %u re-based after a lag (%.3f s, the longest %.1f ms), %u retraces "
            "caught up a lag of over %d ms (the most %.1f ms); time sync: %+lld ns a retrace (%+.3f ms in all), %u "
            "shifts (%+.3f ms)\n",
            n, wall, rate, paced, gaps, P.holds, P.held_ns / 1e9, P.rebases, P.rebased_ns / 1e9, P.rebase_max / 1e6,
            P.late, PACE_SMOOTH_NS / 1000000, P.lag_max / 1e6, (long long)P.adjust, P.adjusted / 1e6, P.shifts,
            P.shifted / 1e6);
    if (P.work_n) {   /* the headroom: what the host did a retrace (the N64 side and the presentation) */
        char work[256];
        work_line(work, sizeof work, P.work_hist, P.work_n, P.work_sum, P.work_max);
        fprintf(stderr, "rush2049pc: paced: the host's work a retrace (the wall time between two less the main "
                "thread's sleep with every N64 thread waiting), %s\n", work);
    }
    pthread_mutex_unlock(&g_plock);
}
