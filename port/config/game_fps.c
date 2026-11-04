/* port/config/game_fps.c: game.frame_rate, the game's own frame rate (docs/port/FRAMERATE.md 7, FRONTEND.md).
 *
 * The setting comes from main at start (rush2049pc.ini, --set game.frame_rate) and from the settings overlay when it
 * changes (pc_game_fps_set): 0 for original (a frame every two retraces at most, the N64's), 60 for a frame every
 * retrace. The game and its scheduler read the latched copy, pc_fps60, which the game thread takes where the game's
 * state changes (game_loop's state change, pc_game_fps_begin): a change applies from the next screen (the next menu,
 * the race's start, the results), never in the middle of one, so a screen's per-frame steps and the scheduler's swaps
 * agree for all of it. Until the first state change it is original.
 *
 * Each frame viUpdateTime gives the frame's retraces (pc_game_frame_note, on the game thread): pc_frame_scale is the
 * frame's length in frames of two retraces and pc_frame_steps the whole ones completed in it, so a step tuned to the
 * N64's 30 Hz frame keeps its speed in real time (pc_game_fps.h). At original both are 1 every frame.
 *
 * A host that can't keep 60 (FRAMERATE.md 7.8): in a race at 60 the frames the game drew and the retraces they took
 * are counted (pc_game_frame_note, the race told by pc_game_fps_state); when over 4 s it kept fewer than 0.9 frames a
 * retrace, one line says so in the log, once a race, and the overlay's About page shows the last race's share
 * (pc_game_fps_describe). The game keeps its speed either way: a frame of two retraces at 60 is the N64's frame. */
#include <math.h>
#include <stdio.h>

#include "config.h"
#include "pc_game_fps.h"

volatile int pc_fps60;
int pc_frame_steps = 1;
float pc_frame_scale = 1.0f;

static volatile int s_setting;   /* 0 original, 60 */
static int s_half;               /* a retrace left over toward the next 30 Hz frame */

#define RACE_STATE 0x400000u     /* gGameState's race (the countdown is 0x200000) */
#define SLOW_WINDOW 240          /* retraces a share is taken over: 4 s */
#define SLOW_PERMILLE 900        /* under 0.9 of the frames: the host can't keep 60 */
/* The game thread's (pc_game_frame_note, pc_game_fps_state) */
static struct {
    int racing, told;
    unsigned win_frames, win_retraces, frames, retraces;
} s_watch;
/* What the overlay reads, from the main thread: the last race's share at 60, frames per 1,000 retraces (-1: none
 * yet), and whether a 4 s window of it fell under 0.9 */
static int s_kept = -1, s_slow;

void pc_game_fps_set(int fps)
{
    s_setting = fps == 60 ? 60 : 0;
}

int pc_game_fps_setting(void)
{
    return s_setting;
}

void pc_game_fps_begin(void)
{
    pc_fps60 = s_setting == 60;
}

void pc_game_fps_state(unsigned int state)
{
    int racing = pc_fps60 && (state & RACE_STATE);
    if (racing && !s_watch.racing) {   /* a race begins: its own count */
        s_watch.told = 0;
        s_watch.win_frames = s_watch.win_retraces = s_watch.frames = s_watch.retraces = 0;
        __atomic_store_n(&s_slow, 0, __ATOMIC_RELAXED);
    }
    s_watch.racing = racing;
}

/* A frame of the race at 60: its share, and over each 4 s whether the host kept 60 */
static void watch_frame(int retraces)
{
    s_watch.frames++, s_watch.retraces += (unsigned)retraces;
    s_watch.win_frames++, s_watch.win_retraces += (unsigned)retraces;
    __atomic_store_n(&s_kept, (int)(s_watch.frames * 1000ull / s_watch.retraces), __ATOMIC_RELAXED);
    if (s_watch.win_retraces < SLOW_WINDOW)
        return;
    unsigned kept = (unsigned)(s_watch.win_frames * 1000ull / s_watch.win_retraces);
    if (kept < SLOW_PERMILLE) {
        __atomic_store_n(&s_slow, 1, __ATOMIC_RELAXED);
        if (!s_watch.told)
            fprintf(stderr, "rush2049pc: game.frame_rate = 60: the game drew %.2f of its frames in %.0f s of the "
                    "race (%.0f a second): this host can't keep 60 here. The game keeps its speed: a frame it can't "
                    "draw in a retrace takes two, as the N64's 30 Hz frames do; game.frame_rate = original keeps an "
                    "even 30\n",
                    kept / 1000.0, s_watch.win_retraces / 60.0, kept * 60 / 1000.0);
        s_watch.told = 1;
    }
    s_watch.win_frames = s_watch.win_retraces = 0;
}

void pc_game_frame_note(int retraces)
{
    if (retraces < 0)
        retraces = 0;
    if (!pc_fps60) {
        pc_frame_steps = 1;
        pc_frame_scale = 1.0f;
        s_half = 0;
    } else {
        pc_frame_steps = (s_half + retraces) >> 1;
        s_half = (s_half + retraces) & 1;
        pc_frame_scale = (float)retraces * 0.5f;
        if (s_watch.racing && retraces > 0)
            watch_frame(retraces);
    }
}

void pc_game_fps_describe(char *out, size_t size)
{
    int kept = __atomic_load_n(&s_kept, __ATOMIC_RELAXED), slow = __atomic_load_n(&s_slow, __ATOMIC_RELAXED);
    if (!size)
        return;
    if (s_setting != 60 && kept < 0)
        snprintf(out, size, "original (30 a second)");
    else if (kept < 0)
        snprintf(out, size, "60");
    else
        snprintf(out, size, "%s; the last race at 60 kept %.2f of its frames (%.0f a second)%s",
                 s_setting == 60 ? "60" : "original (30 a second)", kept / 1000.0, kept * 60 / 1000.0,
                 slow ? ": this host can't keep 60 there" : "");
}

float pc_fps_filter(float e)
{
    if (pc_frame_scale == 1.0f || e <= 0.0f)
        return e;
    return powf(e, pc_frame_scale);
}
