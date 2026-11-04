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
 * N64's 30 Hz frame keeps its speed in real time (pc_game_fps.h). At original both are 1 every frame. */
#include <math.h>

#include "config.h"
#include "pc_game_fps.h"

volatile int pc_fps60;
int pc_frame_steps = 1;
float pc_frame_scale = 1.0f;
unsigned int pc_frames30;

static volatile int s_setting;   /* 0 original, 60 */
static int s_half;               /* a retrace left over toward the next 30 Hz frame */

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
    }
    pc_frames30 += (unsigned int)pc_frame_steps;
}

float pc_fps_filter(float e)
{
    if (pc_frame_scale == 1.0f || e <= 0.0f)
        return e;
    return powf(e, pc_frame_scale);
}
