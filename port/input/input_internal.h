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
/* input.keyboard_steering = smooth: one axis of the stick from keys (v, -1..1) a retrace later, with the keys
 * pointing to target (-1, 0 or 1). out: the share of full tilt a retrace adds on the way out; back: the share it
 * takes off on the way to the centre (the key released, or the other way pressed). Through the centre on a
 * reversal, what is left of the retrace goes out the other way. */
double pc_steer_step(double v, int target, double out, double back);
/* The steps for a ramp of ms from the centre to full tilt at hz retraces a second, back in a third of it; 0 ms gives
 * full tilt and the centre in one retrace. */
void pc_steer_rates(int ms, int hz, double *out, double *back);

/* ---------------------------------------------------------------- input_sdl.c (no-ops without SDL2) */

/* After pc_gfx_init. 0 when live input is on: an SDL window exists and its events reach port/input. */
int pc_input_live_init(const PcSettings *s);
void pc_input_live_shutdown(void);
/* Merge the keyboard and controllers into pads (buttons OR'ed, the stronger stick per axis) and mark ports
 * with a controller connected. */
void pc_input_live_poll(PcPad pads[4]);
void pc_input_live_held(uint16_t held[4]);   /* the buttons the keyboard and controllers hold now, nothing else done */
void pc_input_menu_closed(void);             /* the overlay just closed: its buttons held stay idle (input.c) */
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
/* What the desktop offers the About page: 0 nothing (headless, no SDL2); 1 a window with a clipboard (SDL's
 * offscreen and dummy drivers: the tests); 2 a window on a real display, where a folder can be opened too. */
int pc_input_live_desktop(void);
int pc_input_live_clipboard(const char *text);   /* 0 when it is on the clipboard */
int pc_input_live_open_folder(const char *path); /* the file manager (SDL_OpenURL: ShellExecute, xdg-open); 0 asked */
/* --press-via: the press scripts through SDL instead of into the pads (tests). The kind, when its virtual pads
 * are plugged in (seconds on the N64 clock; 0: before input starts) and how many, before pc_input_live_init; then
 * the scripts' pads (and whether MENU is held) at each poll, before pc_input_live_poll. -1 without live input. */
enum { PC_VIA_PADS, PC_VIA_CONTROLLER, PC_VIA_JOYSTICK, PC_VIA_KEYBOARD };
void pc_input_live_via_setup(int kind, double at, int nports);
int pc_input_live_via(const PcPad script[4], int menu, double now);

/* ---------------------------------------------------------------- input.c */
void pc_input_apply(const PcSettings *s);     /* the overlay's input changes, rumble strength included */
/* An online session's rules (docs/port/NETPLAY.md 1.5, port/app/netplay.c): nothing holds the game, neither the pause
 * key, nor focus lost with pause_on_focus_loss, nor the settings overlay, which opens over the running game with this
 * player's pads idle while it is open (that idle input is what is sent, so both PCs still run one game); and the
 * overlay locks the settings the game reads (pc_settings_session_rule) and the Saves page's changes to the pak. */
void pc_input_set_session(int on);
int pc_input_session(void);

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
const char *pc_menu_message(void);            /* the footer's message ("" for none): "Saved 2 settings to ..." */
/* A notice over the game and the overlay (an online session's "Waiting for the host..."): a title and a line in a box
 * near the top, drawn at each present and by pc_gfx_redraw; NULL removes it. */
void pc_menu_notice(const char *title, const char *line);

/* The mouse, from input_sdl.c while the overlay is open: x, y in the drawable's pixels, w x h the drawable's size
 * (pc_menu_mouse_pixels turns the window system's units into them). MOVE selects the item under it; LEFT chooses
 * it (a value: left of its middle the previous one, right of it the next, a bar where it is clicked), scrolls at the
 * list's arrows, and outside the panel goes back; RIGHT goes back (and cancels a capture, as a click does); WHEEL
 * scrolls a long page by 3 rows a notch (amount: notches, + away from the user). */
enum { PC_MOUSE_MOVE, PC_MOUSE_LEFT, PC_MOUSE_RIGHT, PC_MOUSE_WHEEL };
void pc_menu_mouse(int what, int x, int y, int w, int h, int amount);
/* A point in the window system's units (SDL's mouse events, a win_w x win_h window) in the drawable's pixels
 * (draw_w x draw_h: SDL_GL_GetDrawableSize): the same on Windows (per-monitor DPI aware) and X11, twice on a 2x
 * macOS or Wayland output. */
void pc_menu_mouse_pixels(int x, int y, int win_w, int win_h, int draw_w, int draw_h, int *px, int *py);
/* What is at drawable pixel (x, y) of a w x h window on the page shown: outside the panel, the panel, an item (and
 * which part of it: its label, left or right of its value, or its bar, with bar the place 0-1000), or the list's
 * scroll arrows. Returns hit->area. */
enum { PC_HIT_OUTSIDE, PC_HIT_PANEL, PC_HIT_ITEM, PC_HIT_SCROLL_UP, PC_HIT_SCROLL_DOWN };
enum { PC_PART_LABEL, PC_PART_LESS, PC_PART_MORE, PC_PART_BAR };
typedef struct {
    int area, item, part, bar;
} PcMenuHit;
int pc_menu_hit(int w, int h, int x, int y, PcMenuHit *hit);

/* The About page's facts that main has (port/main.c): fill is called each time the page is made. The overlay adds
 * the GL driver (pc_gfx_driver), the players (live), the settings file and the screenshots folder itself. "" for
 * what isn't known. */
typedef struct {
    char version[64], build_type[32], commit[64], build[128];   /* git describe, CMake's type, the hash, the build */
    char audio[96], audio_buffer[128];                         /* "sdl at 48000 Hz", its pulls and queue */
    char rom[PC_CFG_PATH], saves[PC_CFG_PATH], log[PC_CFG_PATH], data[PC_CFG_PATH];
    char memory[128];   /* game.memory and the heap's high-water mark (docs/port/MEMORY.md 4) */
    char mods[256];     /* the mods in use (docs/port/MODS.md) */
} PcAbout;
void pc_menu_set_about(void (*fill)(PcAbout *about));
/* An online session's status for the main page's two Online rows (port/app/netplay.c's pc_net_status: the other PC;
 * the round trip, the input delay, stalls and the last hash check), called each time the overlay is drawn; NULL
 * removes the rows. */
void pc_menu_set_online(void (*status)(char *peer, size_t peer_size, char *link, size_t link_size));

/* The Mods page (docs/port/MODS.md): the mods main read at start from dir. The page orders them as game.mods_order and
 * their mod.ini priorities say, and turns them on and off ([mods]); both apply at the next start. */
#define PC_MENU_MODS 64
typedef struct {
    char folder[64], name[64], version[32], author[64];
    int priority;
    int files;          /* the files it gives for waves */
    int used, refused;  /* of them, in use and refused at start (with the mod on then) */
    int texts, textures;   /* text strings it overrides; a texture pack */
    char problem[160];  /* why it can't be used ("" if it can) */
} PcMenuMod;
void pc_menu_set_mods(const PcMenuMod *mods, int n, const char *dir);
/* The About page as text, "Label: value" lines under a title (the clipboard's, and the tests'). Its length. */
size_t pc_menu_about_text(char *out, size_t size);

/* The Saves page (docs/port/SAVES.md): each player's saves, from main (port/main.c, over port/ultra's pakstore.c).
 * A row is a profile, a ghost or another game's note in the player's saves folder (game.saves = files). */
typedef struct {
    char file[192];      /* in the folder */
    char label[64];      /* "T1 fwd ALICE 1:02.35", "ALICE" */
    int kind;            /* 1 a profile, 2 a ghost, 0 another game's note */
    int track, backward, car, pages;
    char name[14];
    float finish;
    int on_pak, pinned, imported, deleting;
} PcSaveRow;
enum { PC_SAVES_NONE = -1, PC_SAVES_PAK = 0, PC_SAVES_FILES = 1 };
enum { PC_SAVES_PIN, PC_SAVES_UNPIN, PC_SAVES_EXPORT, PC_SAVES_DELETE, PC_SAVES_IMPORT, PC_SAVES_EXPORT_MPK };
typedef struct {
    /* PC_SAVES_FILES (a folder), PC_SAVES_PAK (an .mpk) or PC_SAVES_NONE (no controller pak), as the player has it
     * now; where says the folder or the file ("" for none) */
    int (*backend)(int player, char *where, size_t size);
    int (*list)(int player, PcSaveRow *out, int max);
    /* PC_SAVES_*: 0 done, -1 not; msg the footer's line either way. file is the row's (unused for IMPORT and
     * EXPORT_MPK). */
    int (*act)(int player, int action, const char *file, char *msg, size_t size);
    /* what the import folder gave at start (a refused file's reason), shown on the page; 0 for nothing; NULL: none */
    int (*notice)(int player, char *out, size_t size);
} PcSavesUi;
void pc_menu_set_saves(const PcSavesUi *ui);

/* ---------------------------------------------------------------- port/ultra */
uint64_t pc_host_ns(void);
int pc_si_motor(int channel);
int pc_si_set_pak(int channel, int kind);   /* a controller's accessory, PC_CFG_PAK_* (a hot swap) */
int pc_si_pak(int channel);                 /* what the game finds in it now: none, memory or rumble */

/* ---------------------------------------------------------------- port/gfx (gfx_internal.h): the live settings */
void pc_gfx_set_widescreen(float aspect);
void pc_gfx_set_fill(int on);
void pc_gfx_set_scale(int scale);
void pc_gfx_set_window_size(int w, int h);

#endif
