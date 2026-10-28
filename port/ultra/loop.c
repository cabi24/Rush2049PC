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
static void sim_swap(void)
{
    static int read;
    static double extra_ms, hz;
    if (!read) {
        const char *a = getenv("RUSH_SIM_PRESENT_MS"), *b = getenv("RUSH_SIM_VSYNC_HZ");
        extra_ms = a ? atof(a) : 0;
        hz = b ? atof(b) : 0;
        read = 1;
    }
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
    uint64_t tick_ns = 0;                  /* how long showing one takes, in real time (a running mean) */
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
            pc_vi_retrace(&fb);
            if (fb) {
                pc_gfx_present(fb);
                if (pc_gfx_frame_rate() == 0)
                    sim_swap();
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
        if (rate > 0) {   /* a high frame rate: a frame shown, in step with the retraces */
            uint64_t period = 1000000000u / (uint64_t)rate, t = pc_host_ns();
            if (!next_tick || rate != tick_rate)
                next_tick = vi_at;
            tick_rate = rate;
            if (t >= next_tick && !pc_time_virtual() && pc_rcp_pending()) {
                deadline = t;   /* the game first: its task runs in the next pass, then this frame */
            } else if (t >= next_tick) {
                /* On the host's clock a frame is skipped (the screen keeps the last one) when drawing it would run
                 * into the next retrace, by how long the last ones took: the frames between never delay the
                 * game's retraces or frames, which would slow the game in real time. */
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
        if (!pc_os_wait_main(deadline) && !pc_quit)
            goto wait;   /* --fast: a thread still computes; the loop's work waits until it waits (sched.c) */
    }
    pc_si_flush();
    if (tick_rate)
        fprintf(stderr, "rush2049pc: frame rate %d: %u frames not shown to keep the retraces on time\n", tick_rate,
                ticks_skipped);
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
