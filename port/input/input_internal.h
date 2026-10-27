/* port/input/input_internal.h: the parts of port/input (docs/port/FRONTEND.md).
 *
 *   input.c       pc_input_poll: live input, then scripted presses (--press), rumble, --log-input
 *   input_sdl.c   live input and the window's keys (SDL2 builds): keyboard, game controllers, hot-plug,
 *                 rumble, hotkeys, pause; events arrive through port/gfx's event hook
 *   stick.c       stick math: dead zone, N64 range, the octagonal gate */
#ifndef PC_INPUT_INTERNAL_H
#define PC_INPUT_INTERNAL_H

#include <stdint.h>

#include "config.h"
#include "pc_port.h"

/* ---------------------------------------------------------------- stick.c */

typedef struct {
    double deadzone;   /* radial, 0-0.9 */
    int range;         /* full tilt along an axis */
    int diagonal;      /* each axis at full diagonal tilt; 0: round */
} PcStickShape;

/* x, y in -1..1, +y up. Applies the dead zone (rescaling the rest from 0), clamps to full tilt, and shapes the
 * result to the gate. */
void pc_stick_shape(double x, double y, const PcStickShape *shape, int8_t *out_x, int8_t *out_y);
/* One axis or trigger, -1..1, with the dead zone rescaled away. */
double pc_axis_deadzone(double v, double deadzone);

/* ---------------------------------------------------------------- input_sdl.c (no-ops without SDL2) */

/* After pc_gfx_init. 0 when live input is on: an SDL window exists and its events reach port/input. */
int pc_input_live_init(const PcSettings *s);
void pc_input_live_shutdown(void);
/* Merge the keyboard and controllers into pads (buttons OR'ed, the stronger stick per axis) and mark ports
 * with a controller connected. */
void pc_input_live_poll(PcPad pads[4]);
/* Rumble strength 0-1 for a port's controller, if it has one. */
void pc_input_live_rumble(int port, double level);
int pc_input_live_paused(void);
/* The window event handler port/gfx calls (pc_gfx_set_event_hook): an SDL_Event. Nonzero asks to quit. */
int pc_input_live_event(const void *sdl_event);

/* ---------------------------------------------------------------- port/ultra */
uint64_t pc_host_ns(void);
int pc_si_motor(int channel);

#endif
