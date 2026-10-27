/* port/ultra/loop.c: the host main thread's loop, standing in for the RCP and the interrupts
 * (docs/port/ULTRA.md).
 *
 * The N64 threads run on their own host threads; this loop owns the window and the GL context. It runs the
 * RSP tasks they submit (graphics, then the SP and DP events; audio, then SP), fires the OS timers, sends the
 * AI events, and at each 60 Hz retrace polls input, sends the VI event, shows a swapped framebuffer and handles
 * window events. Between deadlines it sleeps until a thread submits work. --screenshot-at and --frames act two
 * retraces after their graphics task, once the scheduler has swapped its frame in and it has been shown. */
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>

#include "ultra.h"

uint32_t pc_frame;
volatile int pc_quit;
static volatile sig_atomic_t g_dump;

static void on_signal(int sig)
{
    if (sig == SIGQUIT)
        g_dump = 1;
    else
        pc_quit = 1 + 130;
}

static void status(const char *tag)
{
    fprintf(stderr, "rush2049pc: %s t=%.1fs vi=%u gfx_tasks=%u audio_tasks=%u\n", tag, pc_host_ns() / 1e9,
            pc_frame, pc_rcp_gfx_tasks, pc_rcp_audio_tasks);
}

int pc_ultra_run(const PcRunOptions *opt)
{
    const uint64_t vi_ns = 1000000000u / PC_VI_HZ;
    uint64_t next_vi = pc_host_ns() + vi_ns, next_status = 0, next_flush = 0;
    int shot = 0;
    uint32_t shot_vi = 0, frames_vi = 0;   /* the retrace at which each limit was reached */
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGQUIT, on_signal);
    if (opt->status_every)
        next_status = (uint64_t)opt->status_every * 1000000000u;

    while (!pc_quit) {
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
            pc_input_poll(pads);
            pc_si_set_pads(pads);
            pc_vi_retrace(&fb);
            if (fb)
                pc_gfx_present(fb);
            if (pc_gfx_poll_events())
                pc_quit = 1;
            if (opt->screenshot_at && !shot_vi && pc_rcp_gfx_tasks >= opt->screenshot_at)
                shot_vi = pc_frame;
            if (opt->frames && !frames_vi && pc_rcp_gfx_tasks >= opt->frames)
                frames_vi = pc_frame;
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
                break;
            }
        }
        if (g_dump) {
            g_dump = 0;
            status("status");
            pc_os_dump_threads();
        }
        pc_os_wait_main(deadline);
    }
    pc_si_flush();
    return pc_quit > 1 ? pc_quit - 1 : 0;
}
