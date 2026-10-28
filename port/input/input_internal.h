/* port/input/input_internal.h: the parts of port/input (docs/port/FRONTEND.md).
 *
 *   input.c       pc_input_poll: live input, then scripted presses (--press), rumble, --log-input; the tick
 *                 port/gfx's event pump calls (the overlay's scripted input and redraw)
 *   input_sdl.c   live input and the window's keys (SDL2 builds): keyboard, game controllers, hot-plug,
 *                 rumble, hotkeys, pause; events arrive through port/gfx's event hook; the overlay's keys and
 *                 buttons, and what a binding item captures
 *   menu.c        the settings overlay: its pages, live changes, Save, and its drawing (port/gfx's UI hook)
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
    double gain;       /* input.sensitivity: the travel past the dead zone times this; 0 is 1 */
} PcStickShape;

/* x, y in -1..1, +y up. Applies the dead zone (rescaling the rest from 0) and the gain, clamps to full tilt, and
 * shapes the result to the gate. */
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
/* For the overlay: who plays a player ("keyboard", the controller's name, or both) and the N64 stick its controller
 * gives now. 2 with a controller, 1 with the keyboard alone, 0 for nobody. */
int pc_input_live_describe(int port, char *who, size_t n, int8_t *x, int8_t *y);
/* Rumble strength 0-1 for a port's controller, if it has one. */
void pc_input_live_rumble(int port, double level);
int pc_input_live_paused(void);
/* The window event handler port/gfx calls (pc_gfx_set_event_hook): an SDL_Event. Nonzero asks to quit. */
int pc_input_live_event(const void *sdl_event);
/* The overlay's changes: the bindings, hotkeys, the menu button, sticks, threshold, pause_on_focus_loss (not the
 * players). */
void pc_input_live_apply(const PcSettings *s);
/* The overlay opened (game keys released) or closed (keys and buttons still held are ignored until released). */
void pc_input_live_menu(int open);
void pc_input_live_tick(void);                /* a held D-pad or stick repeats in the overlay */
void pc_input_live_fullscreen(int on);
int pc_input_live_is_fullscreen(void);        /* -1 without a window */
void pc_input_live_vsync(int on);
void pc_input_live_window_size(int w, int h); /* headless: the size port/gfx's auto settings follow */
/* --press-via: the press scripts through SDL instead of into the pads (tests). The kind, when its virtual pads
 * are plugged in (seconds on the N64 clock; 0: before input starts) and how many, before pc_input_live_init; then
 * the scripts' pads (and whether MENU is held) at each poll, before pc_input_live_poll. -1 without live input. */
enum { PC_VIA_PADS, PC_VIA_CONTROLLER, PC_VIA_JOYSTICK, PC_VIA_KEYBOARD };
void pc_input_live_via_setup(int kind, double at, int nports);
int pc_input_live_via(const PcPad script[4], int menu, double now);

/* ---------------------------------------------------------------- input.c */
void pc_input_apply(const PcSettings *s);     /* the overlay's input changes, rumble strength included */

/* ---------------------------------------------------------------- menu.c: the settings overlay */

/* What keys, buttons and scripts ask of it. TOGGLE opens or closes it; the rest act only while it is open. */
enum {
    PC_MENU_UP, PC_MENU_DOWN, PC_MENU_LEFT, PC_MENU_RIGHT, PC_MENU_PAGE_UP, PC_MENU_PAGE_DOWN,
    PC_MENU_ACCEPT, PC_MENU_BACK, PC_MENU_CLEAR, PC_MENU_ADD, PC_MENU_CLOSE, PC_MENU_TOGGLE
};
enum { PC_MENU_CAPTURE_KEY = 1, PC_MENU_CAPTURE_PAD = 2 };
/* The settings in effect, and s->save_path for Save. */
void pc_menu_init(const PcSettings *s);
void pc_menu_shutdown(void);
int pc_menu_is_open(void);
void pc_menu_action(int action);
/* A binding item waits for a key (PC_MENU_CAPTURE_KEY) or a controller input (PC_MENU_CAPTURE_PAD), else 0. */
int pc_menu_capturing(void);
/* The input it waits for, by its name in the file ("Space", "y", "+leftx"); NULL cancels. */
void pc_menu_capture(const char *name);
/* At each event pump: a capture's timeout, and the redraw after a change. Nonzero when Quit was chosen. */
int pc_menu_tick(void);
/* For tests: the page's title, the selected item's label and what it shows. */
void pc_menu_state(const char **page, const char **item, char *value, size_t value_size);
const PcSettings *pc_menu_settings(void);     /* the settings as the overlay has them */

/* ---------------------------------------------------------------- port/ultra */
uint64_t pc_host_ns(void);
int pc_si_motor(int channel);

/* ---------------------------------------------------------------- port/gfx (gfx_internal.h): the live settings */
void pc_gfx_set_widescreen(float aspect);
void pc_gfx_set_scale(int scale);
void pc_gfx_set_window_size(int w, int h);

#endif
