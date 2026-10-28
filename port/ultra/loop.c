/* port/ultra/loop.c: the host main thread's loop, standing in for the RCP and the interrupts
 * (docs/port/ULTRA.md).
 *
 * The N64 threads run on their own host threads; this loop owns the window and the GL context. It runs the
 * RSP tasks they submit (graphics, then the SP and DP events; audio, then SP), fires the OS timers, sends the
 * AI events, and at each 60 Hz retrace handles window events, polls input, sends the VI event and shows a swapped
 * framebuffer. Between deadlines it sleeps until a thread submits work. --screenshot-at and --frames act two
 * retraces after their graphics task, once the scheduler has swapped its frame in and it has been shown. While
 * port/input says the game is paused, it holds (hold()). With a game-state hook (port/main.c), each retrace it logs
 * a change of the game's state word and checks --until-state (docs/port/SYNTH.md). With --fast the clock is
 * virtual (timer.c): the waits move it instead of sleeping, and a run whose clock stops moving for 30 s of real
 * time (a thread computing for ever) is stopped as hung.
 *
 * A high frame rate (video.frame_rate, docs/port/GFX.md "High frame rate"): port/gfx then shows nothing at the
 * retraces; the loop calls pc_gfx_present_tick that many times a second instead, in step with the retraces, with
 * the moment in retraces, and port/gfx shows the swapped-in frame or one drawn between two. The ticks post nothing
 * to the game, so its timing is the same. With --fast they are not deadlines either: they run when the loop wakes
 * for something else, so the virtual clock stops at the same moments with the setting on or off.
 *
 * Windows has no SIGQUIT: Ctrl-Break (SIGBREAK) prints the threads' states there. Its timeouts follow the system
 * timer (15.6 ms by default), and winpthreads' are 15.6 ms steps whatever the period: the scheduler's own waits use
 * high-resolution timers (sched.c), and the loop asks for a 1 ms period for the rest (SDL's waits). */
#define _GNU_SOURCE
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#define SIGQUIT SIGBREAK
#endif

#include "ultra.h"

#define VI_CATCH_UP 6   /* retraces up to this many periods late are caught up; a longer stall drops them */
#define VI_CATCH_UP_GAP_NS 2000000u   /* a late retrace waits at most this long for the game to take the last */

uint32_t pc_frame;
volatile int pc_quit;
static volatile sig_atomic_t g_dump;

static void on_signal(int sig)
{
#ifdef _WIN32
    signal(sig, on_signal);   /* the CRT resets a handler to the default before calling it */
#endif
    if (sig == SIGQUIT)
        g_dump = 1;
    else
        pc_quit = 1 + 130;
}

static uint32_t (*g_game_state)(void);
static void (*g_game_info)(char *, size_t);

/* The game's state word and its state-log values, read when a retrace is due before the loop does anything
 * else: with --fast every N64 thread waits then (pc_os_wait_main), so the log, the status lines and --until-state
 * see the same values in every run; read after the retrace's event they would race the threads it wakes. */
static uint32_t g_snap_state;
static char g_snap_info[256];

static void snapshot(void)
{
    if (g_game_state)
        g_snap_state = g_game_state();
    if (g_game_info)
        g_game_info(g_snap_info, sizeof g_snap_info);
}

static void status(const char *tag)
{
    char state[288] = "";
    if (g_game_state)
        snprintf(state, sizeof state, " state=0x%08X", (unsigned)g_snap_state);
    if (g_game_info) {
        size_t n = strlen(state);
        snprintf(state + n, sizeof state - n, ": %s", g_snap_info);
    }
    fprintf(stderr, "rush2049pc: %s t=%.1fs vi=%u gfx_tasks=%u audio_tasks=%u%s\n", tag, pc_host_ns() / 1e9,
            pc_frame, pc_rcp_gfx_tasks, __atomic_load_n(&pc_rcp_audio_tasks, __ATOMIC_RELAXED), state);
}

/* Once per retrace: logs each change of the game's state word, and says when --until-state is met (the
 * graphics-task count at which its frames are done, else 0). */
static uint32_t watch_state(const PcRunOptions *opt, uint32_t until_gfx)
{
    static uint32_t last, seen, first, entered;
    uint32_t s = g_snap_state;
    if (!seen)
        first = s;
    entered |= s != first;
    if (s != last || !seen) {
        char info[260] = "";
        if (opt->game_info)
            snprintf(info, sizeof info, ": %s", g_snap_info);
        fprintf(stderr, "rush2049pc: game state 0x%08X (was 0x%08X) at vi %u, after %u graphics tasks%s\n",
                (unsigned)s, (unsigned)last, pc_frame, pc_rcp_gfx_tasks, info);
        last = s, seen = 1;
    }
    /* The value the state word starts with, before initGameState changes it, is no state the game has entered: the
     * game's own "no state" is -1 (func_800B7FF8), whose bits match every mask. */
    if (opt->until_state && !until_gfx && entered && (s & opt->until_state))
        until_gfx = pc_rcp_gfx_tasks + opt->until_state_frames + (opt->until_state_frames == 0);
    return until_gfx;
}

void osSetTime(uint64_t ticks);   /* timer.c */

#ifdef _WIN32
/* A 1 ms timer period while the loop runs. The program asks for it at startup too, for the whole run, with the
 * opt-out from Windows 11's power throttling that would otherwise ignore it in a process nobody sees (a headless
 * run, the game minimized): port/app/startup.c. A program that runs the loop without that startup still gets the
 * period here. winpthreads' timed waits stay at 15.6 ms steps anyway (sched.c), and a machine may refuse the period,
 * so the scheduler doesn't rely on it. */
static void timer_period(int on)
{
    if (on)
        timeBeginPeriod(1);
    else
        timeEndPeriod(1);
}
#endif

/* A test of a window's swap on a headless run (docs/port/AUDIO.md): RUSH_SIM_PRESENT_MS holds each present that
 * long (a slow GPU), then RUSH_SIM_VSYNC_HZ until the next vblank of a display at that rate (a vsync swap). Called
 * where the window swaps: after a retrace's present, or with a high frame rate after a tick that showed a frame
 * (the retrace's present then only keeps the frame for the ticks, and swaps nothing). */
static double sim_extra_ms, sim_hz;

static void sim_read(void)
{
    static int read;
    if (!read) {
        const char *a = getenv("RUSH_SIM_PRESENT_MS"), *b = getenv("RUSH_SIM_VSYNC_HZ");
        sim_extra_ms = a ? atof(a) : 0;
        sim_hz = b ? atof(b) : 0;
        read = 1;
    }
}

static void sim_swap(void)
{
    double extra_ms, hz;
    sim_read();
    extra_ms = sim_extra_ms, hz = sim_hz;
    if (pc_time_virtual() || (extra_ms <= 0 && hz <= 0))
        return;
    uint64_t t = pc_real_ns() + (uint64_t)(extra_ms * 1e6);
    if (hz > 0) {
        uint64_t period = (uint64_t)(1e9 / hz);
        t = (t / period + 1) * period;
    }
    for (uint64_t now = pc_real_ns(); now < t; now = pc_real_ns()) {
        struct timespec ts = {(time_t)((t - now) / 1000000000u), (long)((t - now) % 1000000000u)};
        nanosleep(&ts, NULL);
    }
}

/* A high frame rate paced by the display (docs/port/FRAMERATE.md 2). With a swap that waits for vsync (the window's,
 * or RUSH_SIM_VSYNC_HZ's headless), what is shown changes only at the display's vblanks, so each tick is for one:
 * its moment is that vblank, it is drawn as late as it still makes it, and the swap then waits briefly, if at all.
 * The vblanks: the display's refresh, the phase and period corrected by each swap that waited (it returned at one).
 * A swap that blocks holds the loop, so it never waits across a retrace: the retrace goes out first. */
#define VB_MARGIN_NS 1000000u    /* a tick's drawing ends this long before its vblank (and the loop's wake lateness) */
#define VB_GUARD_NS 300000u      /* ... and this long before a retrace due during it */
#define VB_VI_WORK_NS 500000u    /* the loop's own work at a retrace (events, the VI's present), about */
#define VB_BLOCKED_NS 300000u    /* a swap that took longer waited for a vblank */
#define VB_PRECISE_NS 2000000u   /* the loop's timed waits return within this: a swap can wait for a retrace */

static struct {
    double period;           /* ns: the display's refresh, corrected by the swaps that waited for a vblank */
    int hz;                  /* the display's refresh it started from */
    uint64_t at;             /* a vblank, real ns; 0 none yet */
    uint64_t seen;           /* the last swap that returned at one */
    uint64_t target;         /* the vblank the next tick is for; 0 to choose */
    uint64_t shown;          /* the vblank the last tick was for */
    uint64_t swap;           /* a tick drawn, its swap after the retrace due before its vblank: that vblank */
    uint32_t swap_vi;        /* the retrace count when it was drawn */
    uint32_t shown_n, deferred, missed, retrace_shows, observed;
} vb;

static void vb_reset(void)
{
    vb.at = vb.seen = vb.target = vb.shown = vb.swap = 0;
}

/* The first vblank at or after t; while none is known, t itself, and the vblanks are counted from there. */
static uint64_t vb_next(uint64_t t)
{
    double k;
    int64_t ki;
    if (!vb.at) {
        vb.at = t;
        return t;
    }
    k = ((double)t - (double)vb.at) / vb.period;
    ki = (int64_t)k;
    ki += (double)ki < k;   /* ceil */
    uint64_t v = (uint64_t)((int64_t)vb.at + (int64_t)((double)ki * vb.period + 0.5));
    return v < t ? v + (uint64_t)vb.period : v;
}

/* A swap that took longer than VB_BLOCKED_NS returned at a vblank: the phase is there, and the period moves toward the
 * time since the last one seen (within 2% of the display's: SDL's refresh is whole hertz, 59 for 59.94). */
static void vb_observe(uint64_t o)
{
    if (vb.seen && o > vb.seen && vb.hz > 0) {
        double d = (double)(o - vb.seen), n = (double)(uint64_t)(d / vb.period + 0.5), nominal = 1e9 / vb.hz;
        if (n >= 1 && n <= 16) {
            double p = vb.period + (d / n - vb.period) / 8;
            if (p > nominal * 0.98 && p < nominal * 1.02)
                vb.period = p;
        }
    }
    vb.at = vb.seen = o;
    vb.observed++;
}

/* The swap of a tick drawn for vblank v (0: none, the next one): timed, and the time beyond that vblank and one more
 * (a slow GPU, not the display) returned as the tick's cost. */
static uint64_t vb_swap_now(uint64_t v)
{
    uint64_t s0 = pc_real_ns(), s1;
    pc_gfx_swap_tick();
    sim_swap();
    s1 = pc_real_ns();
    if (s1 - s0 > VB_BLOCKED_NS)
        vb_observe(s1);
    if (!v)
        v = s0;
    return s1 > v + (uint64_t)vb.period ? s1 - v - (uint64_t)vb.period : 0;
}

/* When a tick for vblank v starts, from now t: as late as its drawing (cost) still ends VB_MARGIN_NS before v, after
 * the loop's wake lateness. Not while the retrace due at r would come during it: then after that retrace if it still
 * makes v, else early enough to end before it (its swap then waits for the retrace). 0: none of these. With waits
 * that return late by more than VB_PRECISE_NS (coarse timers: RUSH_SIM_TIMER_STEP_US, a Windows without the
 * high-resolution timer) the swap is the clock: each tick is drawn as soon as the last swap returns, and its swap
 * waits for the vblank, across a retrace if one comes, as the setting off waits after each of the game's frames;
 * not before the vblank before its own. */
static uint64_t vb_start(uint64_t v, uint64_t cost, uint64_t late, uint64_t r, uint64_t t)
{
    uint64_t need = cost + VB_MARGIN_NS + late, s;
    if (v < t + cost + VB_GUARD_NS)
        return 0;
    if (late >= VB_PRECISE_NS)
        return v > t + (uint64_t)vb.period ? v - (uint64_t)vb.period : t;   /* from the vblank before it */
    s = v > t + need ? v - need : t;
    if (r <= s || r >= s + late + cost + VB_GUARD_NS)
        return s;
    if (r + late + VB_VI_WORK_NS + cost + VB_GUARD_NS <= v)
        return r;
    if (r >= t + late + cost + VB_GUARD_NS)
        return r - late - cost - VB_GUARD_NS;
    return 0;
}

/* The pause key, or focus lost with pause_on_focus_loss (port/input): no retraces, timers, RSP tasks or AI
 * events while held, so the game's threads stop where they wait for them; window events are still handled.
 * The N64 clock is set back by the time held, so osGetTime and the OS timers don't jump. */
static void hold(void)
{
    uint64_t ticks = pc_ticks_now();
    pc_audio_set_paused(1);
    while (pc_input_paused() && !pc_quit) {
        if (pc_gfx_poll_events())
            pc_quit = 1;
        struct timespec ts = {0, 1000000000 / PC_VI_HZ};
        nanosleep(&ts, NULL);
    }
    pc_audio_set_paused(0);
    osSetTime(ticks);
}

int pc_ultra_run(const PcRunOptions *opt)
{
    const uint64_t vi_ns = 1000000000u / PC_VI_HZ;
    uint64_t next_vi = pc_host_ns() + vi_ns, next_status = 0, next_flush = 0;
    int shot = 0;
    uint32_t shot_vi = 0, frames_vi = 0;   /* the retrace at which each limit was reached */
    uint32_t until_gfx = 0;                /* --until-state: the graphics task to run to, once the state is seen */
    uint64_t last_vi_real = pc_real_ns();  /* --fast: when the last retrace was, in real time */
    uint64_t vi_at = next_vi - vi_ns;      /* when retrace pc_frame was due */
    uint64_t next_tick = 0;                /* a high frame rate: the next frame to show; 0 to start again */
    uint64_t tick_ns = 0;                  /* how long showing one takes, in real time (a running mean); paced by
                                            * the display, without the swap's wait for its vblank */
    uint64_t wake_late = 0;                /* how late the loop's timed waits return (a decaying maximum) */
    uint32_t ticks_skipped = 0;            /* frames not shown, to keep the game's retraces on time */
    uint32_t vi_dropped = 0;               /* retraces dropped: the loop stalled VI_CATCH_UP periods or more */
    uint32_t vi_caught_up = 0;             /* retraces a whole period late, caught up */
    uint32_t vi_not_taken = 0;             /* of them, sent before the game was done with the last */
    uint64_t vi_late_cap = 0;              /* real time: the next retrace is late; it goes out once the game is
                                            * done with the last (pc_os_quiet), and at the latest at this time */
    uint64_t late_at[2] = {0, 0};          /* real time of the last two retraces late (the loop fell behind) */
    int tick_rate = 0;
    g_game_state = opt->game_state;
    g_game_info = opt->game_info;
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGQUIT, on_signal);
#ifdef _WIN32
    timer_period(1);
#endif
    if (opt->status_every)
        next_status = (uint64_t)opt->status_every * 1000000000u;

    while (!pc_quit) {
        if (pc_input_paused()) {
            hold();
            next_vi = pc_host_ns() + vi_ns;
            vi_late_cap = 0;
            next_tick = 0;
            vb_reset();
            continue;
        }
        uint64_t now = pc_host_ns(), deadline = next_vi, ticks = pc_ticks_now();
        int vi_due = now >= next_vi;
        if (vi_due && vi_late_cap) {   /* late: the game's threads take each retrace before the next comes */
            int quiet = pc_os_quiet();
            if (!quiet && now < vi_late_cap) {
                vi_due = 0;
                deadline = vi_late_cap;
            } else {
                vi_not_taken += !quiet;
                vi_late_cap = 0;
            }
        }
        if (vi_due)
            snapshot();
        uint64_t timer = pc_timer_service(ticks);
        if (timer && timer > ticks && now + pc_ticks_to_ns(timer - ticks) < deadline)
            deadline = now + pc_ticks_to_ns(timer - ticks);
        else if (timer && timer <= ticks)
            deadline = now;
        uint64_t ai = 0;
        pc_rcp_service(now, &ai);
        if (ai && ai < deadline)
            deadline = ai;

        if (vi_due) {
            PcPad pads[4];
            const void *fb;
            pc_frame++;
            last_vi_real = pc_real_ns();
            if (pc_gfx_poll_events())   /* first, so the pads below are this retrace's */
                pc_quit = 1;
            pc_input_poll(pads);
            pc_si_set_pads(pads);
            pc_gfx_retrace(pc_frame);
            if (!pc_time_virtual())
                pc_os_quiet_mark();
            /* a high frame rate: the game's last frame, swapped in a retrace or more ago, not shown by any tick */
            int stale = !pc_time_virtual() && pc_gfx_frame_rate() != 0 && pc_gfx_frame_waiting();
            pc_vi_retrace(&fb);
            if (fb) {
                pc_gfx_present(fb);
                if (pc_gfx_frame_rate() == 0) {
                    sim_swap();
                } else if (!pc_time_virtual() && !vb.swap &&
                           (stale || tick_ns + VB_MARGIN_NS + VB_VI_WORK_NS >= vi_ns)) {
                    /* With the setting on, ticks show the game's frames. When none showed the last one (they were
                     * skipped to keep the retraces on time), or none fits between two retraces (a host behind), the
                     * game's new frame is shown here, at its swap, as with the setting off: the game's frames are
                     * never lost to skipped ticks, and showing them costs no retrace (docs/port/QA.md "Integration
                     * soak"). */
                    uint64_t r0 = pc_real_ns();
                    if (pc_gfx_present_tick(pc_frame, 2 | PC_GFX_TICK_NO_SWAP)) {
                        uint64_t cost = pc_real_ns() - r0;
                        cost += vb_swap_now(0);   /* the swap beyond a refresh: a slow GPU */
                        tick_ns = (tick_ns * 3 + cost) / 4;   /* so the ticks come back once the host has time */
                        vb.shown = vb.seen;   /* the vblank it waited for, if it did */
                        vb.target = 0;
                        vb.retrace_shows++;
                    }
                }
            }
            if (opt->screenshot_at && !shot_vi && pc_rcp_gfx_tasks >= opt->screenshot_at)
                shot_vi = pc_frame;
            if (opt->frames && !frames_vi && pc_rcp_gfx_tasks >= opt->frames)
                frames_vi = pc_frame;
            if (g_game_state)
                until_gfx = watch_state(opt, until_gfx);
            if (until_gfx && pc_rcp_gfx_tasks >= until_gfx) {
                status("state reached:");
                break;
            }
            if (shot_vi && !shot && pc_frame >= shot_vi + 2) {
                shot = 1;
                if (pc_gfx_save_screenshot(opt->screenshot) == 0)
                    fprintf(stderr, "rush2049pc: screenshot after %u graphics tasks: %s\n", pc_rcp_gfx_tasks,
                            opt->screenshot);
                else
                    fprintf(stderr, "rush2049pc: no screenshot (%s)\n", opt->screenshot);
            }
            int shot_due = opt->screenshot_at && opt->screenshot_at <= opt->frames && !shot;
            if (frames_vi && pc_frame >= frames_vi + 2 && !shot_due) {
                status("frames reached:");
                break;
            }
            next_vi += vi_ns;
            if (next_vi <= now) {   /* behind (a slow frame, a swap waiting for vsync) */
                late_at[0] = late_at[1];   /* when the last two were */
                late_at[1] = pc_real_ns();
                /* A late retrace comes soon after, so the game keeps its 60 a second: it counts its time in
                 * retraces, and its audio renders the buffers the AI has passed at each, skipping some when it
                 * finds more than its cap (docs/port/AUDIO.md 7). In real time it waits until the game's threads
                 * are done with this one, up to 2 ms: sent at once, a burst of them filled the queues of the
                 * scheduler's clients (the audio manager's holds a command per sound source per retrace). A
                 * stall (a debugger, a long hitch) resynchronizes. */
                if (now - next_vi >= VI_CATCH_UP * vi_ns) {
                    vi_dropped += (uint32_t)((now - next_vi) / vi_ns + 1);
                    next_vi = now + vi_ns;
                } else {
                    vi_caught_up++;
                    if (!pc_time_virtual())
                        vi_late_cap = pc_host_ns() + VI_CATCH_UP_GAP_NS;
                }
            }
            vi_at = next_vi - vi_ns;
            deadline = next_vi;
            if (now >= next_flush) {
                pc_si_flush();
                pc_audio_periodic();
                next_flush = now + 1000000000u;
            }
            if (next_status && now >= next_status) {
                status("status");
                next_status += (uint64_t)opt->status_every * 1000000000u;
            }
            if (opt->seconds > 0 && now >= (uint64_t)(opt->seconds * 1e9)) {
                status("time reached:");
                if (opt->until_state) {
                    fprintf(stderr, "rush2049pc: --until-state 0x%X:%u not reached in %.0f s\n",
                            (unsigned)opt->until_state, (unsigned)opt->until_state_frames, opt->seconds);
                    pc_quit = 1 + 1;
                }
                break;
            }
        }
        int rate = pc_gfx_frame_rate();
        if (rate > 0 && !pc_time_virtual() && (pc_gfx_vsync() || (sim_read(), sim_hz > 0))) {
            /* A high frame rate with a swap that waits for vsync: paced by the display's vblanks (vb above). */
            uint64_t period = 1000000000u / (uint64_t)rate, t = pc_host_ns();
            int hz = pc_gfx_display_hz(), every = rate >= hz;   /* a rate at or above the refresh: every vblank */
            if (!next_tick || rate != tick_rate)
                next_tick = vi_at;
            tick_rate = rate;
            if (hz != vb.hz) {   /* the first tick, or another display */
                vb.hz = hz;
                vb.period = 1e9 / hz;
                vb_reset();
            }
            if (vb.swap) {   /* drawn; its swap after the retrace due before its vblank */
                if (pc_frame != vb.swap_vi || t + VB_VI_WORK_NS >= vb.swap) {
                    if (t < vb.swap) {
                        tick_ns += vb_swap_now(vb.swap) / 4;
                        vb.deferred++;
                    } else {   /* the vblank went by (a graphics task ran first): a swap now would wait for the next */
                        vb.missed++;
                        ticks_skipped++;
                    }
                    vb.swap = 0;
                } else if (vb.swap - VB_VI_WORK_NS < deadline) {
                    deadline = vb.swap - VB_VI_WORK_NS;
                }
            }
            if (!vb.swap && tick_ns + VB_MARGIN_NS + VB_VI_WORK_NS < vi_ns) {
                /* else no tick fits between two retraces: the game's frames are shown at the retraces (above) */
                uint64_t late = wake_late < vi_ns / 2 ? wake_late : vi_ns / 2;
                if (!vb.target) {   /* the next vblank: one a tick can still make, one after the last, and with a
                                     * rate below the refresh the one at or after the rate's next tick */
                    uint64_t from = t + tick_ns + VB_MARGIN_NS;
                    if (vb.shown && from < vb.shown + (uint64_t)vb.period / 2)
                        from = vb.shown + (uint64_t)vb.period / 2;
                    if (!every && from < next_tick)
                        from = next_tick;
                    vb.target = vb_next(from);
                }
                uint64_t v = vb.target, start = vb_start(v, tick_ns, late, next_vi, t), last;
                double vt = pc_frame + ((double)v - (double)vi_at) / (double)vi_ns;   /* its moment, in retraces */
                if (!start) {   /* it can't make that vblank without running into a retrace: the screen keeps what
                                 * it shows */
                    ticks_skipped++;
                    vb.shown = v;
                    vb.target = 0;
                    deadline = t;
                } else if (t >= start) {
                    if (pc_rcp_pending()) {
                        deadline = t;   /* the game first: its task runs in the next pass, then this frame */
                    } else if (next_vi <= t + tick_ns + VB_GUARD_NS && late < VB_PRECISE_NS) {
                        if (next_vi > t && next_vi < deadline)   /* the retrace first (a late one: its own wait) */
                            deadline = next_vi;
                    } else if (pc_gfx_tick_waits(vt) &&
                               (last = v - tick_ns - VB_GUARD_NS - (late < VB_PRECISE_NS ? late : 0)) > t &&
                               last < v) {
                        /* the frame is between the shown one and the game's next, which isn't drawn yet: its
                         * graphics task wakes the loop, and runs first, as long as the tick still makes its vblank */
                        if (last < deadline)
                            deadline = last;
                    } else {
                        /* the mode: the GPU's spare time only; only the game's frames while the host is behind (two
                         * retraces dropped within a second) */
                        uint64_t r0 = pc_real_ns(), cost;
                        int mode = late_at[0] && r0 - late_at[0] < 1000000000u ? 2 : 1;
                        int drew = pc_gfx_present_tick(vt, mode | PC_GFX_TICK_NO_SWAP);
                        cost = pc_real_ns() - r0;
                        if (drew && next_vi + VB_VI_WORK_NS < v && wake_late < VB_PRECISE_NS) {
                            vb.swap = v;   /* a retrace comes first: the swap waits for it, not it for the swap */
                            vb.swap_vi = pc_frame;
                            if (next_vi < deadline)
                                deadline = next_vi;
                        } else if (drew) {
                            cost += vb_swap_now(v);
                        }
                        tick_ns = (tick_ns * 3 + cost) / 4;
                        vb.shown_n += drew;
                        vb.shown = v;
                        vb.target = 0;
                        if (next_tick <= v)
                            next_tick += ((v - next_tick) / period + 1) * period;
                        if (!vb.swap)
                            deadline = t;   /* the next one chosen in the next pass */
                    }
                } else if (start < deadline) {
                    deadline = start;
                }
            }
        } else if (rate > 0) {   /* a high frame rate: a frame shown, in step with the retraces */
            uint64_t period = 1000000000u / (uint64_t)rate, t = pc_host_ns();
            vb.swap = vb.target = 0;   /* vsync just turned off: a swap left is the next tick's */
            if (!next_tick || rate != tick_rate)
                next_tick = vi_at;
            tick_rate = rate;
            if (t >= next_tick && !pc_time_virtual() && pc_rcp_pending()) {
                deadline = t;   /* the game first: its task runs in the next pass, then this frame */
            } else if (t >= next_tick) {
                /* On the host's clock a frame is skipped (the screen keeps the last one) when drawing it would run
                 * into the next retrace, by how long the last ones took: the frames between never delay the
                 * game's retraces or frames, which would slow the game in real time. The game's frames are then
                 * shown at the retraces (above). */
                if (pc_time_virtual() || t + tick_ns <= next_vi) {
                    /* the mode: exact with --fast; else the GPU's spare time only; and only the game's frames
                     * while the host is behind (two retraces dropped within a second) */
                    uint64_t r0 = pc_real_ns();
                    int mode = pc_time_virtual() ? 0 : late_at[0] && r0 - late_at[0] < 1000000000u ? 2 : 1;
                    if (pc_gfx_present_tick(pc_frame + ((double)next_tick - (double)vi_at) / (double)vi_ns, mode))
                        sim_swap();
                    tick_ns = (tick_ns * 3 + (pc_real_ns() - r0)) / 4;
                } else {
                    ticks_skipped++;
                }
                next_tick += period;
                if (next_tick <= t)   /* behind (a slow frame, a blocking swap): skip to the next one due */
                    next_tick += ((t - next_tick) / period + 1) * period;
            }
            if (!pc_time_virtual() && next_tick < deadline)
                deadline = next_tick;
        } else {
            next_tick = 0;
            vb.swap = vb.target = 0;
        }
        if (g_dump) {
            g_dump = 0;
            snapshot();
            status("status");
            pc_os_dump_threads();
        }
    wait:
        if (pc_time_virtual() && pc_real_ns() - last_vi_real > 30000000000u) {
            snapshot();
            status("hung:");
            fprintf(stderr, "rush2049pc: --fast: no retrace for 30 s of real time: a thread computes without "
                    "waiting\n");
            pc_os_dump_threads();
            pc_quit = 1 + 1;
            break;
        }
        uint64_t w0 = pc_host_ns();
        if (!pc_os_wait_main(deadline) && !pc_quit)
            goto wait;   /* --fast: a thread still computes; the loop's work waits until it waits (sched.c) */
        if (!pc_time_virtual() && deadline > w0) {   /* a timed wait: how late it returned, if it timed out */
            uint64_t w1 = pc_host_ns(), keep = wake_late - wake_late / 32;
            if (w1 >= deadline)
                wake_late = w1 - deadline > keep ? w1 - deadline : keep;
        }
    }
    pc_si_finish();   /* the paks' writes done before the process ends (si.c) */
    if (tick_rate)
        fprintf(stderr, "rush2049pc: frame rate %d: %u frames not shown to keep the retraces on time\n", tick_rate,
                ticks_skipped);
    if (vb.hz)
        fprintf(stderr, "rush2049pc: paced by the display's vblanks (%.3f Hz, %u swaps returned at one): %u ticks "
                "shown at their vblank, %u of them swapped after a retrace, %u missed it; %u of the game's frames "
                "shown at their retrace; the loop's waits up to %.1f ms late\n", 1e9 / vb.period, vb.observed,
                vb.shown_n, vb.deferred, vb.missed, vb.retrace_shows, wake_late / 1e6);
    fprintf(stderr, "rush2049pc: %u retraces in %.1f s; %u came a whole retrace late and were caught up, %u dropped "
            "(the host stalled %u retraces or more); %u late ones went out before the game was done with the last\n",
            pc_frame, pc_host_ns() / 1e9, vi_caught_up, vi_dropped, VI_CATCH_UP, vi_not_taken);
    /* the game's scheduler may still be counting (ThreadSanitizer) */
    fprintf(stderr, "rush2049pc: %u audio tasks; %u graphics tasks yielded to one\n",
            __atomic_load_n(&pc_rcp_audio_tasks, __ATOMIC_RELAXED),
            __atomic_load_n(&pc_rcp_gfx_yields, __ATOMIC_RELAXED));
#ifdef _WIN32
    timer_period(0);
#endif
    return pc_quit > 1 ? pc_quit - 1 : 0;
}
