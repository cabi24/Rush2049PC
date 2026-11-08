/* port/include/pc_game_fps.h: game.frame_rate, the game's own frame rate (docs/port/FRAMERATE.md 7).
 *
 * At `original` the game draws a frame every two retraces at most, as on the N64; at 60 every retrace. The setting is
 * latched where the game changes its state (game_loop, pc_game_fps_begin): from the next screen, never in the middle
 * of one. The game's per-frame logic then steps by the frame's length: what already steps by the frame time
 * (D_8002EB94) or the retraces (D_8002EB98) does so by itself; what steps by a constant tuned to a frame of two
 * retraces goes through the macros below. Included by pc_prelude.h, so they are in every game file. At `original`
 * each macro gives exactly the N64's value (no float operation is added), so the game is the N64's. */
#ifndef PC_GAME_FPS_H
#define PC_GAME_FPS_H

extern volatile int pc_fps60;       /* the latched setting: 1 while the game draws a frame every retrace */
extern int pc_frame_steps;          /* whole 30 Hz frames (two retraces) this frame: at 60, 0 or 1 a retrace */
extern float pc_frame_scale;        /* this frame's length in 30 Hz frames: retraces / 2 (0.5 at 60) */

void pc_game_fps_begin(void);       /* latch the setting (the game's state changes) */
/* The state the game changes to there (state_word_b, the next gGameState): at 60 a race is watched for frames lost to a
 * host that can't keep 60 (FRAMERATE.md 7.7) */
void pc_game_fps_state(unsigned int state);
void pc_game_frame_note(int retraces);   /* viUpdateTime: this frame's retraces, at most 5 */
extern void (*pc_game_frame_hook)(int retraces);   /* --log-frames (main.c): called at each frame's note; NULL none */
float pc_fps_filter(float e);       /* e to the power of the frame's length in 30 Hz frames */
/* For x = x * e + noise once a 30 Hz frame, taken every frame (x * pc_fps_filter(e) + noise * this): the noise's scale
 * that keeps x's spread, sqrt((1 - e^2f) / (1 - e^2)) for a frame of f 30 Hz frames; 1 at f = 1 */
float pc_fps_noise(float e);

/* A float step tuned to a frame of two retraces (a menu slide, a fade): scaled to this frame's length. */
#define PC_FSCALE(x) (pc_fps60 ? (x) * pc_frame_scale : (x))
/* An integer step per frame, as whole 30 Hz frames (0 or 1 at 60). */
#define PC_STEPS(n) (pc_fps60 ? (n) * pc_frame_steps : (n))
/* A statement run once a 30 Hz frame: a count or a test against one (`if (--t == 0)`) that a step of 0 would repeat.
 * It is an `if`: never directly before an `else` of the code around it (every use is checked so). */
#define PC_EACH30(stmt) if (!pc_fps60 || pc_frame_steps) { stmt }
/* A filter or decay applied once a frame, x = x * e + y * (1 - e), tuned to a frame of two retraces: e^(frames). */
#define PC_FILTER(e) (pc_fps60 ? pc_fps_filter(e) : (e))
/* The scheduler's retraces between two swaps (src/rom/lib_1050_sdk.c). */
#define PC_SWAP_RETRACES ((unsigned int)(pc_fps60 ? 1 : 2))

#endif
