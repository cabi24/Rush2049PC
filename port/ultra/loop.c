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
 * Windows has no SIGQUIT: Ctrl-Break (SIGBREAK) prints the threads' states there. Its timeouts are as coarse as
 * the system timer (15.6 ms by default): the scheduler's own waits use high-resolution timers (sched.c), and the
 * loop asks for a 1 ms period for the rest (hold()'s sleep, SDL). */
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#define SIGQUIT SIGBREAK
#endif

#include "ultra.h"

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

static void status(const char *tag)
{
    char state[32] = "";
    if (g_game_state)
        snprintf(state, sizeof state, " state=0x%08X", (unsigned)g_game_state());
    fprintf(stderr, "rush2049pc: %s t=%.1fs vi=%u gfx_tasks=%u audio_tasks=%u%s\n", tag, pc_host_ns() / 1e9,
            pc_frame, pc_rcp_gfx_tasks, pc_rcp_audio_tasks, state);
}

/* Once per retrace: logs each change of the game's state word, and says when --until-state is met (the
 * graphics-task count at which its frames are done, else 0). */
static uint32_t watch_state(const PcRunOptions *opt, uint32_t until_gfx)
{
    static uint32_t last, seen;
    uint32_t s = g_game_state();
    if (s != last || !seen) {
        fprintf(stderr, "rush2049pc: game state 0x%08X (was 0x%08X) at vi %u, after %u graphics tasks\n",
                (unsigned)s, (unsigned)last, pc_frame, pc_rcp_gfx_tasks);
        last = s, seen = 1;
    }
    if (opt->until_state && !until_gfx && (s & opt->until_state))
        until_gfx = pc_rcp_gfx_tasks + opt->until_state_frames + (opt->until_state_frames == 0);
    return until_gfx;
}

void osSetTime(uint64_t ticks);   /* timer.c */

#ifdef _WIN32
/* A 1 ms timer period while the loop runs. Windows 11 may ignore the request from a process nobody sees (a
 * headless run, the hidden GL window) unless the process opts out of that power throttling, which needs Windows 8's
 * SetProcessInformation (looked up, so older Windows still start). Some machines keep 15.6 ms anyway (the
 * windows-latest runner did), which is why sched.c doesn't rely on it. */
static void timer_period(int on)
{
    typedef BOOL(WINAPI * SetInfo)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);
    if (!on) {
        timeEndPeriod(1);
        return;
    }
    SetInfo set = (SetInfo)(void *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetProcessInformation");
    PROCESS_POWER_THROTTLING_STATE s = {PROCESS_POWER_THROTTLING_CURRENT_VERSION,
                                        PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION, 0};
    if (set)
        set(GetCurrentProcess(), ProcessPowerThrottling, &s, sizeof s);
    timeBeginPeriod(1);
}
#endif

/* The pause key, or focus lost with pause_on_focus_loss (port/input): no retraces, timers, RSP tasks or AI
 * events while held, so the game's threads stop where they wait for them; window events are still handled.
 * The N64 clock is set back by the time held, so osGetTime and the OS timers don't jump. */
static void hold(void)
{
    uint64_t ticks = pc_ticks_now();
    while (pc_input_paused() && !pc_quit) {
        if (pc_gfx_poll_events())
            pc_quit = 1;
        struct timespec ts = {0, 1000000000 / PC_VI_HZ};
        nanosleep(&ts, NULL);
    }
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
    g_game_state = opt->game_state;
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
            continue;
        }
        uint64_t now = pc_host_ns(), deadline = next_vi, ticks = pc_ticks_now();
        uint64_t timer = pc_timer_service(ticks);
        if (timer && timer > ticks && now + pc_ticks_to_ns(timer - ticks) < deadline)
            deadline = now + pc_ticks_to_ns(timer - ticks);
        else if (timer && timer <= ticks)
            deadline = now;
        uint64_t ai = 0;
        pc_rcp_service(now, &ai);
        if (ai && ai < deadline)
            deadline = ai;

        if (now >= next_vi) {
            PcPad pads[4];
            const void *fb;
            pc_frame++;
            last_vi_real = pc_real_ns();
            if (pc_gfx_poll_events())   /* first, so the pads below are this retrace's */
                pc_quit = 1;
            pc_input_poll(pads);
            pc_si_set_pads(pads);
            pc_vi_retrace(&fb);
            if (fb)
                pc_gfx_present(fb);
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
            if (next_vi <= now)   /* stalled (a debugger, a slow frame): resynchronize */
                next_vi = now + vi_ns;
            deadline = next_vi;
            if (now >= next_flush) {
                pc_si_flush();
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
        if (g_dump) {
            g_dump = 0;
            status("status");
            pc_os_dump_threads();
        }
        if (pc_time_virtual() && pc_real_ns() - last_vi_real > 30000000000u) {
            status("hung:");
            fprintf(stderr, "rush2049pc: --fast: no retrace for 30 s of real time: a thread computes without "
                    "waiting\n");
            pc_os_dump_threads();
            pc_quit = 1 + 1;
            break;
        }
        pc_os_wait_main(deadline);
    }
    pc_si_flush();
#ifdef _WIN32
    timer_period(0);
#endif
    return pc_quit > 1 ? pc_quit - 1 : 0;
}
