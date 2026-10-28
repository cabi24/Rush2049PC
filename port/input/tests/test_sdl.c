/* port/input/tests/test_sdl.c: live input through the real window and event loop (docs/port/FRONTEND.md).
 *
 *   frontend_sdl_test keyboard|hotkeys|focus|controller|hotplug|rumble|menu|script|all SCRATCH_DIR
 *
 * menu drives the settings overlay with keys and a controller: F1 and the menu button open it, the game is held
 * and gets nothing, keys and a controller input are captured into bindings the game then sees, the key or button
 * that closes it is ignored until released, fullscreen changes at once.
 *
 * Runs on SDL's offscreen video driver (SDL_VIDEODRIVER=offscreen), so it needs no display. port/gfx opens its
 * SDL window and GL context as rush2049pc does; port/input installs its event hook. Keys and window events are
 * pushed with SDL_PushEvent; controllers are SDL virtual joysticks (SDL_JoystickAttachVirtualEx) of the
 * GameController type, whose buttons and axes are set directly. Each step pumps the events the way the main
 * loop does (pc_gfx_poll_events) and then reads the pads (pc_input_poll). */
#define _GNU_SOURCE
#include <SDL.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "config.h"
#include "input_internal.h"
#include "pc_compat.h"
#include "test_gbi.h"

int pc_input_init(const PcSettings *s);
void pc_input_shutdown(void);
int pc_input_paused(void);
int pc_input_add_spec(const char *spec);
int pc_gfx_read_window(uint8_t *rgba, int *w, int *h);
extern int pc_input_log;
extern uint64_t test_now_ns;
extern int test_motor[4];

static int g_fail, g_checks;
static const char *g_dir;
static SDL_Window *g_win;
static PcPad g_pads[4];

#define CHECK(cond, ...)                                                                                      \
    do {                                                                                                      \
        g_checks++;                                                                                           \
        if (!(cond)) {                                                                                        \
            g_fail++;                                                                                         \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);                                              \
            fprintf(stderr, __VA_ARGS__);                                                                     \
            fprintf(stderr, "\n");                                                                            \
        }                                                                                                     \
    } while (0)

/* ---------------------------------------------------------------- harness */

static int start(const PcSettings *s)
{
    if (pc_gfx_init(0, 640, 480) != 0 || !(g_win = SDL_GL_GetCurrentWindow())) {
        fprintf(stderr, "FAIL: no SDL window (SDL_VIDEODRIVER=%s)\n", getenv("SDL_VIDEODRIVER"));
        return -1;
    }
    /* the test's own hold on the joystick subsystem: the virtual pads outlive an input restart */
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "FAIL: SDL game controllers: %s\n", SDL_GetError());
        return -1;
    }
    if (pc_input_init(s) != 0) {
        fprintf(stderr, "FAIL: live input did not start\n");
        return -1;
    }
    return 0;
}

static void stop(void)
{
    pc_input_shutdown();
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    pc_gfx_shutdown();
}

/* restart input alone with other settings, keeping the window */
static void restart_input(const PcSettings *s)
{
    pc_input_shutdown();
    CHECK(pc_input_init(s) == 0, "input restart");
}

static int pump(void)
{
    return pc_gfx_poll_events();
}

static void poll_pads(void)
{
    pump();
    pc_input_poll(g_pads);
}

static void key(SDL_Scancode sc, int down, Uint16 mod)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.windowID = SDL_GetWindowID(g_win);
    e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    e.key.keysym.scancode = sc;
    e.key.keysym.sym = SDL_GetKeyFromScancode(sc);
    e.key.keysym.mod = mod;
    SDL_PushEvent(&e);
}

static void window_event(Uint8 what)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_WINDOWEVENT;
    e.window.windowID = SDL_GetWindowID(g_win);
    e.window.event = what;
    SDL_PushEvent(&e);
}

static void pad_is(int port, uint16_t buttons, int x, int y, int connected, const char *what)
{
    const PcPad *p = &g_pads[port];
    CHECK(p->buttons == buttons && p->stick_x == x && p->stick_y == y && p->connected == connected,
          "%s: player %d buttons %04x stick (%d, %d) connected %d; expected %04x (%d, %d) %d", what, port + 1,
          p->buttons, p->stick_x, p->stick_y, p->connected, buttons, x, y, connected);
}

/* ---------------------------------------------------------------- virtual controllers */

typedef struct {
    SDL_JoystickID id;
    SDL_Joystick *joy;
    int rumbles;
    Uint16 low, high;
} Pad;

static int SDLCALL on_rumble(void *user, Uint16 low, Uint16 high)
{
    Pad *p = user;
    p->rumbles++;
    p->low = low;
    p->high = high;
    return 0;
}

static void attach(Pad *p, const char *name)
{
    SDL_VirtualJoystickDesc d;
    SDL_zero(d);
    memset(p, 0, sizeof *p);
    d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    d.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    d.naxes = SDL_CONTROLLER_AXIS_MAX;
    d.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    d.name = name;
    d.userdata = p;
    d.Rumble = on_rumble;
    int index = SDL_JoystickAttachVirtualEx(&d);
    CHECK(index >= 0, "attach %s: %s", name, SDL_GetError());
    if (index < 0)
        return;
    CHECK(SDL_IsGameController(index), "%s is a game controller", name);
    p->joy = SDL_JoystickOpen(index);
    p->id = SDL_JoystickInstanceID(p->joy);
    /* at rest: sticks centered, triggers released (a trigger's full axis -32768..32767 maps to 0..32767) */
    for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; a++)
        SDL_JoystickSetVirtualAxis(p->joy, a,
                                   a >= SDL_CONTROLLER_AXIS_TRIGGERLEFT ? SDL_JOYSTICK_AXIS_MIN : 0);
}

static void detach(Pad *p)
{
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_JoystickGetDeviceInstanceID(i) == p->id) {
            SDL_JoystickClose(p->joy);
            CHECK(SDL_JoystickDetachVirtual(i) == 0, "detach: %s", SDL_GetError());
            p->joy = NULL;
            return;
        }
    CHECK(0, "no joystick %d to detach", (int)p->id);
}

static void button(Pad *p, SDL_GameControllerButton b, int down)
{
    SDL_JoystickSetVirtualButton(p->joy, b, (Uint8)down);
}

static void axis(Pad *p, SDL_GameControllerAxis a, int v)
{
    SDL_JoystickSetVirtualAxis(p->joy, a, (Sint16)(v > 32767 ? 32767 : v < -32768 ? -32768 : v));
}

/* ---------------------------------------------------------------- a frame to show */

static uint16_t s_fb[320 * 240];
static PcGfxWords s_list[64];

static void show_frame(void)
{
    gp = s_list;
    g_be_mode = 0;
    gsDPPipeSync();
    gsDPSetScissor(0, 0, 320, 240);
    gsDPSetCycleType(G_CYC_FILL);
    gsDPSetRenderMode(0, 0);
    gsDPSetColorImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 320, (uintptr_t)s_fb);
    gsDPSetFillColor(0xF801F801u);   /* red */
    gsDPFillRectangle(0, 0, 319, 239);
    gsDPFullSync();
    gsSPEndDisplayList();
    pc_gfx_run_dl(s_list);
    pc_gfx_present(s_fb);
}

static int count_png(const char *dir)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    int n = 0;
    if (!d)
        return -1;
    while ((e = readdir(d)))
        n += !strncmp(e->d_name, "rush2049pc-", 11) && strstr(e->d_name, ".png") != NULL;
    closedir(d);
    return n;
}

/* ---------------------------------------------------------------- cases */

static void test_keyboard(void)
{
    static const struct { SDL_Scancode sc; uint16_t mask; } keys[] = {
        {SDL_SCANCODE_X, 0x8000}, {SDL_SCANCODE_C, 0x4000}, {SDL_SCANCODE_Z, 0x2000}, {SDL_SCANCODE_RETURN, 0x1000},
        {SDL_SCANCODE_W, 0x0800}, {SDL_SCANCODE_S, 0x0400}, {SDL_SCANCODE_A, 0x0200}, {SDL_SCANCODE_D, 0x0100},
        {SDL_SCANCODE_Q, 0x0020}, {SDL_SCANCODE_E, 0x0010}, {SDL_SCANCODE_I, 0x0008}, {SDL_SCANCODE_K, 0x0004},
        {SDL_SCANCODE_J, 0x0002}, {SDL_SCANCODE_L, 0x0001}};
    PcSettings s;
    pc_settings_defaults(&s);
    if (start(&s) != 0) {
        g_fail++;
        return;
    }
    poll_pads();
    pad_is(0, 0, 0, 0, 1, "idle");
    for (int p = 1; p < 4; p++)
        pad_is(p, 0, 0, 0, 0, "no controller");
    for (size_t i = 0; i < sizeof keys / sizeof *keys; i++) {
        key(keys[i].sc, 1, 0);
        poll_pads();
        pad_is(0, keys[i].mask, 0, 0, 1, SDL_GetScancodeName(keys[i].sc));
        key(keys[i].sc, 0, 0);
        poll_pads();
        pad_is(0, 0, 0, 0, 1, "released");
    }
    /* several at once, and a held key stays pressed across retraces */
    key(SDL_SCANCODE_X, 1, 0);
    key(SDL_SCANCODE_RETURN, 1, 0);
    poll_pads();
    poll_pads();
    pad_is(0, 0x9000, 0, 0, 1, "A+START held");
    key(SDL_SCANCODE_X, 0, 0);
    key(SDL_SCANCODE_RETURN, 0, 0);
    /* the stick: full tilt, the octagon's corner on a diagonal, opposite keys cancel */
    key(SDL_SCANCODE_RIGHT, 1, 0);
    poll_pads();
    pad_is(0, 0, 80, 0, 1, "Right");
    key(SDL_SCANCODE_UP, 1, 0);
    poll_pads();
    pad_is(0, 0, 70, 70, 1, "Right+Up");
    key(SDL_SCANCODE_RIGHT, 0, 0);
    key(SDL_SCANCODE_DOWN, 1, 0);
    poll_pads();
    pad_is(0, 0, 0, 0, 1, "Up+Down");
    key(SDL_SCANCODE_UP, 0, 0);
    poll_pads();
    pad_is(0, 0, 0, -80, 1, "Down");
    key(SDL_SCANCODE_DOWN, 0, 0);
    key(SDL_SCANCODE_LEFT, 1, 0);
    poll_pads();
    pad_is(0, 0, -80, 0, 1, "Left");
    key(SDL_SCANCODE_LEFT, 0, 0);
    poll_pads();
    pad_is(0, 0, 0, 0, 1, "stick released");

    /* rebinding: two keys for A (one by its alias), an unknown name skipped, the keyboard on player 2 */
    snprintf(s.key[PC_CTL_A], PC_CFG_BIND, "Space, Comma");
    snprintf(s.key[PC_CTL_B], PC_CFG_BIND, "NoSuchKey, Left Shift");
    s.keyboard_player = 2;
    s.stick_range = 100;
    s.stick_diagonal = 0;
    restart_input(&s);
    key(SDL_SCANCODE_COMMA, 1, 0);
    poll_pads();
    pad_is(1, 0x8000, 0, 0, 1, "Comma on player 2");
    pad_is(0, 0, 0, 0, 1, "player 1 without the keyboard");
    key(SDL_SCANCODE_COMMA, 0, 0);
    key(SDL_SCANCODE_SPACE, 1, 0);
    key(SDL_SCANCODE_LSHIFT, 1, 0);
    key(SDL_SCANCODE_X, 1, 0);   /* no longer bound */
    key(SDL_SCANCODE_RIGHT, 1, 0);
    key(SDL_SCANCODE_UP, 1, 0);
    poll_pads();
    pad_is(1, 0xC000, 71, 71, 1, "Space+Left Shift, a round gate of 100");
    stop();
}

static void test_hotkeys(void)
{
    PcSettings s;
    pc_settings_defaults(&s);
    snprintf(s.screenshot_dir, sizeof s.screenshot_dir, "%s/shots", g_dir);
    mkdir(s.screenshot_dir, 0755);
    if (start(&s) != 0) {
        g_fail++;
        return;
    }
    CHECK(!(SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN), "starts windowed");
    CHECK(!strcmp(SDL_GetWindowTitle(g_win), "San Francisco Rush 2049"), "title '%s'", SDL_GetWindowTitle(g_win));
    int w, h;
    SDL_GetWindowMinimumSize(g_win, &w, &h);
    CHECK(w == 320 && h == 240, "minimum size %dx%d", w, h);

    /* Alt+Enter toggles fullscreen and is not START */
    key(SDL_SCANCODE_LALT, 1, KMOD_LALT);
    key(SDL_SCANCODE_RETURN, 1, KMOD_LALT);
    poll_pads();
    CHECK(SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN, "Alt+Enter: fullscreen");
    pad_is(0, 0, 0, 0, 1, "Alt+Enter is not START");
    key(SDL_SCANCODE_RETURN, 0, KMOD_LALT);
    key(SDL_SCANCODE_LALT, 0, 0);
    /* F11 toggles back; a key repeat does nothing */
    key(SDL_SCANCODE_F11, 1, 0);
    poll_pads();
    CHECK(!(SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN), "F11: windowed");
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_KEYDOWN;
    e.key.keysym.scancode = SDL_SCANCODE_F11;
    e.key.repeat = 1;
    SDL_PushEvent(&e);
    key(SDL_SCANCODE_F11, 0, 0);
    poll_pads();
    CHECK(!(SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN), "F11 repeat ignored");
    /* Return alone is START */
    key(SDL_SCANCODE_RETURN, 1, 0);
    poll_pads();
    pad_is(0, 0x1000, 0, 0, 1, "Return");
    key(SDL_SCANCODE_RETURN, 0, 0);

    /* F12: a PNG of the shown frame */
    int before = count_png(s.screenshot_dir);
    show_frame();
    key(SDL_SCANCODE_F12, 1, 0);
    key(SDL_SCANCODE_F12, 0, 0);
    poll_pads();
    CHECK(count_png(s.screenshot_dir) == before + 1, "F12: %d screenshots, expected %d", count_png(s.screenshot_dir),
          before + 1);
    key(SDL_SCANCODE_F12, 1, 0);   /* a second one in the same second gets its own name */
    poll_pads();
    CHECK(count_png(s.screenshot_dir) == before + 2, "second F12");

    /* Pause toggles the hold; Escape quits; neither reaches the game */
    CHECK(!pc_input_paused(), "running");
    key(SDL_SCANCODE_PAUSE, 1, 0);
    poll_pads();
    CHECK(pc_input_paused(), "Pause: held");
    CHECK(strstr(SDL_GetWindowTitle(g_win), "paused") != NULL, "paused title");
    window_event(SDL_WINDOWEVENT_EXPOSED);   /* shows the last frame again while held */
    key(SDL_SCANCODE_PAUSE, 0, 0);
    key(SDL_SCANCODE_PAUSE, 1, 0);
    poll_pads();
    CHECK(!pc_input_paused(), "Pause again: running");
    key(SDL_SCANCODE_ESCAPE, 1, 0);
    CHECK(pump() != 0, "Escape quits");
    pc_input_poll(g_pads);
    pad_is(0, 0, 0, 0, 1, "Escape is not a game key");

    /* hotkeys rebound or cleared */
    snprintf(s.hotkey[PC_HOTKEY_QUIT], PC_CFG_BIND, "%s", "");
    snprintf(s.hotkey[PC_HOTKEY_FULLSCREEN], PC_CFG_BIND, "%s", "F2");
    snprintf(s.key[PC_CTL_START], PC_CFG_BIND, "%s", "Escape");
    restart_input(&s);
    key(SDL_SCANCODE_ESCAPE, 1, 0);
    CHECK(pump() == 0, "Escape no longer quits");
    pc_input_poll(g_pads);
    pad_is(0, 0x1000, 0, 0, 1, "Escape bound to START");
    key(SDL_SCANCODE_F2, 1, 0);
    poll_pads();
    CHECK(SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN, "F2: fullscreen");
    /* the window closing quits, as before */
    window_event(SDL_WINDOWEVENT_CLOSE);
    CHECK(pump() != 0, "close quits");
    stop();

    /* fullscreen and vsync from the settings */
    s.fullscreen = 1;
    s.vsync = 0;
    if (start(&s) != 0) {
        g_fail++;
        return;
    }
    CHECK(SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN, "fullscreen = on");
    stop();
}

static void test_focus(void)
{
    PcSettings s;
    pc_settings_defaults(&s);
    if (start(&s) != 0) {
        g_fail++;
        return;
    }
    /* losing focus releases held keys (their key-up goes elsewhere); no hold by default */
    key(SDL_SCANCODE_X, 1, 0);
    poll_pads();
    pad_is(0, 0x8000, 0, 0, 1, "held");
    window_event(SDL_WINDOWEVENT_FOCUS_LOST);
    poll_pads();
    pad_is(0, 0, 0, 0, 1, "focus lost: released");
    CHECK(!pc_input_paused(), "no hold without pause_on_focus_loss");
    window_event(SDL_WINDOWEVENT_FOCUS_GAINED);
    s.pause_on_focus_loss = 1;
    restart_input(&s);
    window_event(SDL_WINDOWEVENT_FOCUS_LOST);
    poll_pads();
    CHECK(pc_input_paused(), "focus lost: held");
    window_event(SDL_WINDOWEVENT_SIZE_CHANGED);
    pump();
    window_event(SDL_WINDOWEVENT_FOCUS_GAINED);
    poll_pads();
    CHECK(!pc_input_paused(), "focus back: running");
    /* the pause key and focus are separate reasons: both must clear */
    key(SDL_SCANCODE_PAUSE, 1, 0);
    key(SDL_SCANCODE_PAUSE, 0, 0);
    window_event(SDL_WINDOWEVENT_FOCUS_LOST);
    window_event(SDL_WINDOWEVENT_FOCUS_GAINED);
    pump();
    CHECK(pc_input_paused(), "still held by the pause key");
    key(SDL_SCANCODE_PAUSE, 1, 0);
    pump();
    CHECK(!pc_input_paused(), "released");
    stop();
}

static void test_controller(void)
{
    static const struct { SDL_GameControllerButton b; uint16_t mask; } buttons[] = {
        {SDL_CONTROLLER_BUTTON_A, 0x8000},          {SDL_CONTROLLER_BUTTON_X, 0x4000},
        {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 0x2000}, {SDL_CONTROLLER_BUTTON_START, 0x1000},
        {SDL_CONTROLLER_BUTTON_DPAD_UP, 0x0800},    {SDL_CONTROLLER_BUTTON_DPAD_DOWN, 0x0400},
        {SDL_CONTROLLER_BUTTON_DPAD_LEFT, 0x0200},  {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 0x0100},
        {SDL_CONTROLLER_BUTTON_BACK, 0x0020},       {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 0x0010},
        {SDL_CONTROLLER_BUTTON_Y, 0x0008},          {SDL_CONTROLLER_BUTTON_B, 0x0004},
        {SDL_CONTROLLER_BUTTON_GUIDE, 0},           {SDL_CONTROLLER_BUTTON_LEFTSTICK, 0},
        {SDL_CONTROLLER_BUTTON_RIGHTSTICK, 0}};
    static const struct { SDL_GameControllerAxis a; int v; uint16_t mask; } axes[] = {
        {SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 32767, 0x8000}, {SDL_CONTROLLER_AXIS_TRIGGERLEFT, 32767, 0x4000},
        {SDL_CONTROLLER_AXIS_RIGHTY, -32768, 0x0008},      {SDL_CONTROLLER_AXIS_RIGHTY, 32767, 0x0004},
        {SDL_CONTROLLER_AXIS_RIGHTX, -32768, 0x0002},      {SDL_CONTROLLER_AXIS_RIGHTX, 32767, 0x0001}};
    PcSettings s;
    Pad pad;
    pc_settings_defaults(&s);
    if (start(&s) != 0) {
        g_fail++;
        return;
    }
    attach(&pad, "virtual pad");
    poll_pads();
    pad_is(0, 0, 0, 0, 1, "at rest");
    for (size_t i = 0; i < sizeof buttons / sizeof *buttons; i++) {
        button(&pad, buttons[i].b, 1);
        poll_pads();
        pad_is(0, buttons[i].mask, 0, 0, 1, SDL_GameControllerGetStringForButton(buttons[i].b));
        button(&pad, buttons[i].b, 0);
        poll_pads();
        pad_is(0, 0, 0, 0, 1, "button released");
    }
    for (size_t i = 0; i < sizeof axes / sizeof *axes; i++) {
        int rest = axes[i].a >= SDL_CONTROLLER_AXIS_TRIGGERLEFT ? -32768 : 0;
        axis(&pad, axes[i].a, axes[i].v);
        poll_pads();
        pad_is(0, axes[i].mask, 0, 0, 1, SDL_GameControllerGetStringForAxis(axes[i].a));
        axis(&pad, axes[i].a, rest + (axes[i].v - rest) * 2 / 5);   /* 40% of the travel: under the threshold */
        poll_pads();
        pad_is(0, 0, 0, 0, 1, "axis under the threshold");
        axis(&pad, axes[i].a, rest);
    }
    /* the left stick: dead zone, rescaled travel, full tilt, the gate, +y up */
    axis(&pad, SDL_CONTROLLER_AXIS_LEFTX, 32767);
    poll_pads();
    pad_is(0, 0, 80, 0, 1, "stick right");
    axis(&pad, SDL_CONTROLLER_AXIS_LEFTX, 3000);
    poll_pads();
    pad_is(0, 0, 0, 0, 1, "inside the dead zone");
    axis(&pad, SDL_CONTROLLER_AXIS_LEFTX, (int)(0.575 * 32767));
    poll_pads();
    pad_is(0, 0, 40, 0, 1, "half the travel past the dead zone");
    axis(&pad, SDL_CONTROLLER_AXIS_LEFTX, -23170);
    axis(&pad, SDL_CONTROLLER_AXIS_LEFTY, -23170);   /* SDL's up is negative */
    poll_pads();
    pad_is(0, 0, -70, 70, 1, "up-left diagonal at full tilt");
    axis(&pad, SDL_CONTROLLER_AXIS_LEFTX, 0);
    axis(&pad, SDL_CONTROLLER_AXIS_LEFTY, 32767);
    poll_pads();
    pad_is(0, 0, 0, -80, 1, "stick down");
    axis(&pad, SDL_CONTROLLER_AXIS_LEFTY, 0);
    /* merged with the keyboard on player 1: buttons OR'ed, the stronger stick per axis */
    button(&pad, SDL_CONTROLLER_BUTTON_A, 1);
    axis(&pad, SDL_CONTROLLER_AXIS_LEFTX, 16000);
    axis(&pad, SDL_CONTROLLER_AXIS_LEFTY, -32767);
    key(SDL_SCANCODE_C, 1, 0);
    key(SDL_SCANCODE_LEFT, 1, 0);
    poll_pads();
    CHECK(g_pads[0].buttons == 0xC000 && g_pads[0].stick_x == -80 && g_pads[0].stick_y > 60,
          "merged: %04x (%d, %d)", g_pads[0].buttons, g_pads[0].stick_x, g_pads[0].stick_y);
    key(SDL_SCANCODE_C, 0, 0);
    key(SDL_SCANCODE_LEFT, 0, 0);
    button(&pad, SDL_CONTROLLER_BUTTON_A, 0);
    axis(&pad, SDL_CONTROLLER_AXIS_LEFTX, 0);
    axis(&pad, SDL_CONTROLLER_AXIS_LEFTY, 0);

    /* the right stick as the N64 stick (its C bindings stay), other bindings, and a bigger dead zone */
    s.analog = PC_CFG_STICK_RIGHT;
    s.deadzone = 0.3;
    snprintf(s.pad[PC_CTL_Z], PC_CFG_BIND, "lefttrigger, leftstick");
    snprintf(s.pad[PC_CTL_A], PC_CFG_BIND, "a");
    snprintf(s.pad[PC_CTL_STICK_UP], PC_CFG_BIND, "dpup");
    snprintf(s.pad[PC_CTL_B], PC_CFG_BIND, "-lefttrigger, nonsense, x");   /* two bad names: skipped */
    restart_input(&s);
    poll_pads();
    pad_is(0, 0, 0, 0, 1, "rebound, at rest");
    axis(&pad, SDL_CONTROLLER_AXIS_RIGHTX, 32767);
    poll_pads();
    pad_is(0, 0x0001, 80, 0, 1, "right stick: N64 stick and C-right");
    axis(&pad, SDL_CONTROLLER_AXIS_RIGHTX, 9000);   /* 0.27: inside the 0.3 dead zone */
    poll_pads();
    pad_is(0, 0, 0, 0, 1, "bigger dead zone");
    axis(&pad, SDL_CONTROLLER_AXIS_RIGHTX, 0);
    axis(&pad, SDL_CONTROLLER_AXIS_LEFTX, 32767);
    poll_pads();
    pad_is(0, 0, 0, 0, 1, "the left stick is free");
    axis(&pad, SDL_CONTROLLER_AXIS_LEFTX, 0);
    axis(&pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT, 32767);
    poll_pads();
    pad_is(0, 0x2000, 0, 0, 1, "left trigger now Z");
    axis(&pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT, -32768);
    axis(&pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 32767);
    button(&pad, SDL_CONTROLLER_BUTTON_LEFTSTICK, 1);
    button(&pad, SDL_CONTROLLER_BUTTON_X, 1);
    button(&pad, SDL_CONTROLLER_BUTTON_DPAD_UP, 1);
    poll_pads();
    pad_is(0, 0x6800, 0, 80, 1, "right trigger unbound; L3 is Z; x is B; D-up is D-up and the stick");
    stop();
}

static void test_hotplug(void)
{
    PcSettings s;
    Pad p[7];
    pc_settings_defaults(&s);
    if (start(&s) != 0) {
        g_fail++;
        return;
    }
    attach(&p[1], "pad 1");
    attach(&p[2], "pad 2");
    poll_pads();
    pad_is(1, 0, 0, 0, 1, "pad 2 on player 2");
    pad_is(2, 0, 0, 0, 0, "player 3 empty");
    attach(&p[3], "pad 3");
    poll_pads();
    pad_is(2, 0, 0, 0, 1, "pad 3 on player 3");
    button(&p[2], SDL_CONTROLLER_BUTTON_A, 1);
    button(&p[3], SDL_CONTROLLER_BUTTON_START, 1);
    poll_pads();
    pad_is(0, 0, 0, 0, 1, "player 1");
    pad_is(1, 0x8000, 0, 0, 1, "player 2's A");
    pad_is(2, 0x1000, 0, 0, 1, "player 3's START");
    /* pad 1 leaves: player 1 keeps the keyboard, the others keep their players */
    detach(&p[1]);
    poll_pads();
    pad_is(0, 0, 0, 0, 1, "player 1 after pad 1 left");
    pad_is(1, 0x8000, 0, 0, 1, "player 2 kept");
    /* new pads fill the free players in order, then wait */
    attach(&p[4], "pad 4");
    attach(&p[5], "pad 5");
    attach(&p[6], "pad 6");
    poll_pads();
    button(&p[4], SDL_CONTROLLER_BUTTON_B, 1);
    button(&p[5], SDL_CONTROLLER_BUTTON_Y, 1);
    button(&p[6], SDL_CONTROLLER_BUTTON_X, 1);
    poll_pads();
    pad_is(0, 0x0004, 0, 0, 1, "pad 4 took player 1");
    pad_is(3, 0x0008, 0, 0, 1, "pad 5 took player 4");
    /* pad 2 leaves: the waiting pad 6 takes player 2 */
    detach(&p[2]);
    poll_pads();
    poll_pads();
    pad_is(1, 0x4000, 0, 0, 1, "pad 6 took player 2");
    detach(&p[3]);
    detach(&p[4]);
    detach(&p[5]);
    detach(&p[6]);
    poll_pads();
    pad_is(0, 0, 0, 0, 1, "player 1: the keyboard");
    for (int i = 1; i < 4; i++)
        pad_is(i, 0, 0, 0, 0, "all unplugged");

    /* first_controller_player = 2: the keyboard alone on player 1 (v5's --keyboard-alone) */
    s.first_controller_player = 2;
    restart_input(&s);
    attach(&p[1], "pad 1 again");
    poll_pads();
    button(&p[1], SDL_CONTROLLER_BUTTON_A, 1);
    key(SDL_SCANCODE_C, 1, 0);
    poll_pads();
    pad_is(0, 0x4000, 0, 0, 1, "keyboard alone on player 1");
    pad_is(1, 0x8000, 0, 0, 1, "the pad on player 2");
    /* a pad present before input starts is found at once */
    key(SDL_SCANCODE_C, 0, 0);
    restart_input(&s);
    pc_input_poll(g_pads);
    pad_is(1, 0x8000, 0, 0, 1, "present at start");
    detach(&p[1]);
    stop();
}

/* A joystick SDL has no mapping for: the generic one (axes 1-2 the stick, hat 1 the D-pad, buttons in XInput's
 * order, axes 5-6 triggers from the middle up), and a player only from its first button press. */
static void attach_joystick(Pad *p, const char *name, int axes, int buttons, int hats)
{
    SDL_VirtualJoystickDesc d;
    SDL_zero(d);
    memset(p, 0, sizeof *p);
    d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    d.type = SDL_JOYSTICK_TYPE_UNKNOWN;
    d.naxes = (Uint16)axes;
    d.nbuttons = (Uint16)buttons;
    d.nhats = (Uint16)hats;
    d.name = name;
    int index = SDL_JoystickAttachVirtualEx(&d);
    CHECK(index >= 0, "attach %s: %s", name, SDL_GetError());
    if (index < 0)
        return;
    CHECK(!SDL_IsGameController(index), "%s has no mapping", name);
    p->joy = SDL_JoystickOpen(index);
    p->id = SDL_JoystickInstanceID(p->joy);
}

static void test_generic(void)
{
    PcSettings s;
    Pad pad, odd;
    pc_settings_defaults(&s);
    s.first_controller_player = 2;   /* the keyboard alone on player 1 */
    if (start(&s) != 0) {
        g_fail++;
        return;
    }
    attach_joystick(&pad, "generic pad", 6, 12, 1);
    attach_joystick(&odd, "no stick", 1, 4, 0);   /* one axis, no hat: not a pad */
    poll_pads();
    pad_is(1, 0, 0, 0, 0, "a generic pad has no player before a button");
    SDL_JoystickSetVirtualAxis(pad.joy, 0, 32767);
    poll_pads();
    pad_is(1, 0, 0, 0, 0, "nor from its stick");
    SDL_JoystickSetVirtualAxis(pad.joy, 0, 0);
    SDL_JoystickSetVirtualButton(pad.joy, 0, 1);
    poll_pads();
    pad_is(1, 0x8000, 0, 0, 1, "its first button (a: A) takes player 2, and counts");
    SDL_JoystickSetVirtualButton(pad.joy, 0, 0);
    SDL_JoystickSetVirtualAxis(pad.joy, 0, -32768);
    SDL_JoystickSetVirtualAxis(pad.joy, 1, 0);
    poll_pads();
    pad_is(1, 0, -80, 0, 1, "axis 1: the stick's x");
    SDL_JoystickSetVirtualAxis(pad.joy, 0, 0);
    SDL_JoystickSetVirtualAxis(pad.joy, 1, -32768);
    poll_pads();
    pad_is(1, 0, 0, 80, 1, "axis 2: its y, up");
    SDL_JoystickSetVirtualAxis(pad.joy, 1, 0);
    SDL_JoystickSetVirtualHat(pad.joy, 0, SDL_HAT_UP);
    SDL_JoystickSetVirtualButton(pad.joy, 2, 1);   /* x: B */
    SDL_JoystickSetVirtualButton(pad.joy, 7, 1);   /* start */
    poll_pads();
    pad_is(1, 0x0800 | 0x4000 | 0x1000, 0, 0, 1, "hat 1: the D-pad; buttons 3 and 8: x (B) and start");
    SDL_JoystickSetVirtualHat(pad.joy, 0, SDL_HAT_CENTERED);
    SDL_JoystickSetVirtualButton(pad.joy, 2, 0);
    SDL_JoystickSetVirtualButton(pad.joy, 7, 0);
    SDL_JoystickSetVirtualAxis(pad.joy, 4, -32768);   /* a trigger axis at rest, either way: released */
    SDL_JoystickSetVirtualAxis(pad.joy, 5, 0);
    poll_pads();
    pad_is(1, 0, 0, 0, 1, "axes 5-6 at rest or in the middle: released");
    SDL_JoystickSetVirtualAxis(pad.joy, 5, 32767);
    poll_pads();
    pad_is(1, 0x8000, 0, 0, 1, "axis 6 pulled: the right trigger (A)");
    pad_is(0, 0, 0, 0, 1, "player 1: the keyboard alone");
    pad_is(2, 0, 0, 0, 0, "the joystick without a stick has no player");
    SDL_JoystickSetVirtualButton(odd.joy, 0, 1);
    poll_pads();
    pad_is(2, 0, 0, 0, 0, "not even with a button");
    detach(&odd);
    detach(&pad);
    poll_pads();
    pad_is(1, 0, 0, 0, 0, "unplugged");
    stop();
}

static void test_rumble(void)
{
    PcSettings s;
    Pad pad, other;
    pc_settings_defaults(&s);
    if (start(&s) != 0) {
        g_fail++;
        return;
    }
    attach(&pad, "rumble pad");
    attach(&other, "player 2");
    poll_pads();
    CHECK(pad.rumbles == 0, "quiet at start");
    test_motor[0] = 1;
    poll_pads();
    CHECK(pad.low == 0xFFFF && pad.high == 0xFFFF, "motor on: %04x %04x", pad.low, pad.high);
    CHECK(other.rumbles == 0, "only player 1's pad rumbles");
    /* a steady rumble is renewed: SDL ends one after its 1 s duration */
    for (int i = 0; i < 13; i++) {
        SDL_Delay(100);
        poll_pads();
    }
    CHECK(pad.low == 0xFFFF, "still on after 1.3 s: %04x", pad.low);
    /* off: it fades over the last 4 retraces (a pulsing motor stays steady), then stops */
    test_motor[0] = 0;
    Uint16 seen[5];
    for (int i = 0; i < 5; i++) {
        poll_pads();
        seen[i] = pad.low;
    }
    CHECK(seen[0] == (Uint16)(0.75 * 0xFFFF) && seen[1] == (Uint16)(0.5 * 0xFFFF) &&
              seen[2] == (Uint16)(0.25 * 0xFFFF) && seen[3] == 0 && seen[4] == 0,
          "fade %04x %04x %04x %04x %04x", seen[0], seen[1], seen[2], seen[3], seen[4]);
    int lowest = 0xFFFF;
    for (int i = 0; i < 12; i++) {
        test_motor[0] = i % 2 == 0;
        poll_pads();
        if (i >= 4 && pad.low < lowest)
            lowest = pad.low;
    }
    CHECK(lowest >= 0x7FFF, "a motor pulsed every other retrace keeps at least half: %04x", lowest);
    test_motor[0] = 0;
    for (int i = 0; i < 5; i++)
        poll_pads();
    CHECK(pad.low == 0, "off");
    /* strength from the settings, and 0 for none */
    s.rumble = 0.5;
    restart_input(&s);
    test_motor[0] = 1;
    poll_pads();
    CHECK(pad.low == (Uint16)(0.5 * 0xFFFF), "half strength: %04x", pad.low);
    /* the motor on player 2 */
    test_motor[1] = 1;
    poll_pads();
    CHECK(other.low == (Uint16)(0.5 * 0xFFFF), "player 2's motor");
    test_motor[1] = 0;
    /* shutting down stops it */
    int n = pad.rumbles;
    pc_input_shutdown();
    CHECK(pad.rumbles > n && pad.low == 0, "stopped at shutdown");
    s.rumble = 0;
    CHECK(pc_input_init(&s) == 0, "restart");
    poll_pads();
    CHECK(pad.low == 0, "rumble = 0: none");
    test_motor[0] = 0;
    detach(&pad);
    detach(&other);
    stop();
}

static void test_script(void)
{
    PcSettings s;
    Pad pad;
    pc_settings_defaults(&s);
    pc_input_log = 1;   /* exercised; the lines go to stderr */
    CHECK(pc_input_add_spec("1:A@2:2,1:STICKLEFT:2,1:Z:2") == 0, "script");
    CHECK(pc_input_add_spec("1:A@5") != 0, "port 5 rejected");
    if (start(&s) != 0) {
        g_fail++;
        return;
    }
    attach(&pad, "pad");
    test_now_ns = 0;
    poll_pads();
    pad_is(0, 0, 0, 0, 1, "before the script");
    pad_is(1, 0, 0, 0, 1, "a port the script uses is connected");
    key(SDL_SCANCODE_RIGHT, 1, 0);
    key(SDL_SCANCODE_UP, 1, 0);
    button(&pad, SDL_CONTROLLER_BUTTON_A, 1);
    test_now_ns = 1500000000;
    poll_pads();
    pad_is(0, 0xA000, -80, 70, 1, "live A and stick up, scripted Z and stick left");
    pad_is(1, 0x8000, 0, 0, 1, "scripted A on player 2");
    test_now_ns = 3000000000u;
    poll_pads();
    pad_is(0, 0x8000, 70, 70, 1, "the script is over: live only");
    pad_is(1, 0, 0, 0, 1, "player 2 idle");
    detach(&pad);
    stop();
}

/* ---------------------------------------------------------------- the settings overlay */

static void menu_at(const char *page, const char *item, const char *value, const char *what)
{
    const char *p, *i;
    char v[256];
    pc_menu_state(&p, &i, v, sizeof v);
    CHECK(!strcmp(p, page) && !strcmp(i, item) && (!value || !strcmp(v, value)),
          "%s: on '%s' / '%s' = '%s'; expected '%s' / '%s' = '%s'", what, p, i, v, page, item, value ? value : "*");
}

static void tap(SDL_Scancode sc)
{
    key(sc, 1, 0);
    key(sc, 0, 0);
    pump();
}

static void press(Pad *p, SDL_GameControllerButton b)
{
    button(p, b, 1);
    pump();
    button(p, b, 0);
    pump();
}

static void test_menu(void)
{
    PcSettings s;
    Pad pad;
    pc_settings_defaults(&s);
    if (start(&s) != 0) {
        g_fail++;
        return;
    }
    show_frame();
    /* F1 opens it: the game is held, says so in the title, and gets no input */
    key(SDL_SCANCODE_X, 1, 0);
    poll_pads();
    pad_is(0, 0x8000, 0, 0, 1, "X held");
    tap(SDL_SCANCODE_F1);
    pc_input_poll(g_pads);
    CHECK(pc_menu_is_open() && pc_input_paused(), "F1 opens the overlay and holds the game");
    CHECK(strstr(SDL_GetWindowTitle(g_win), "paused") != NULL, "title '%s'", SDL_GetWindowTitle(g_win));
    pad_is(0, 0, 0, 0, 1, "the game's input while it is open");
    key(SDL_SCANCODE_X, 0, 0);
#ifndef _WIN32   /* WGL leaves the back buffer undefined after the swap (docs/port/WINDOWS.md) */
    {
        int w, h;
        uint8_t *px = malloc(640 * 480 * 4);
        CHECK(pc_gfx_read_window(px, &w, &h) == 0 && px[(240 * 640 + 320) * 4] < 60,
              "the panel over the red frame: %d", px[(240 * 640 + 320) * 4]);
        free(px);
    }
#endif
    /* the arrows move, Enter opens, a binding takes the next key, Escape goes back and doesn't quit */
    tap(SDL_SCANCODE_DOWN);
    tap(SDL_SCANCODE_DOWN);
    menu_at("Settings", "Keyboard", NULL, "two down");
    tap(SDL_SCANCODE_RETURN);
    menu_at("Keyboard", "A", "X", "the keyboard page");
    tap(SDL_SCANCODE_RETURN);
    CHECK(pc_menu_capturing() == PC_MENU_CAPTURE_KEY, "waiting for a key");
    tap(SDL_SCANCODE_SPACE);
    menu_at("Keyboard", "A", "Space", "Space captured");
    tap(SDL_SCANCODE_DOWN);
    tap(SDL_SCANCODE_RETURN);
    tap(SDL_SCANCODE_COMMA);
    menu_at("Keyboard", "B", "Comma", "punctuation by its name");
    key(SDL_SCANCODE_ESCAPE, 1, 0);
    key(SDL_SCANCODE_ESCAPE, 0, 0);
    CHECK(pump() == 0, "Escape is the overlay's back key, not quit");
    menu_at("Settings", "Keyboard", NULL, "back");
    tap(SDL_SCANCODE_ESCAPE);
    CHECK(!pc_menu_is_open() && !pc_input_paused(), "Escape again closes it");
    /* the game has the new keys */
    key(SDL_SCANCODE_SPACE, 1, 0);
    key(SDL_SCANCODE_COMMA, 1, 0);
    poll_pads();
    pad_is(0, 0xC000, 0, 0, 1, "Space is A and Comma B now");
    key(SDL_SCANCODE_SPACE, 0, 0);
    key(SDL_SCANCODE_COMMA, 0, 0);
    key(SDL_SCANCODE_X, 1, 0);
    poll_pads();
    pad_is(0, 0, 0, 0, 1, "X isn't A any more");
    key(SDL_SCANCODE_X, 0, 0);

    /* the key that closes it doesn't reach the game while it is still held */
    tap(SDL_SCANCODE_F1);
    menu_at("Settings", "Keyboard", NULL, "reopened");
    for (int i = 0; i < 5; i++)   /* past Controllers, Help, About and Save */
        tap(SDL_SCANCODE_DOWN);
    menu_at("Settings", "Resume", NULL, "Resume");
    key(SDL_SCANCODE_RETURN, 1, 0);
    poll_pads();
    CHECK(!pc_menu_is_open(), "Resume closes it");
    pad_is(0, 0, 0, 0, 1, "Enter still held from the overlay isn't START");
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_KEYDOWN;
    e.key.keysym.scancode = SDL_SCANCODE_RETURN;
    e.key.repeat = 1;
    SDL_PushEvent(&e);
    poll_pads();
    pad_is(0, 0, 0, 0, 1, "nor its key repeat");
    key(SDL_SCANCODE_RETURN, 0, 0);
    key(SDL_SCANCODE_RETURN, 1, 0);
    poll_pads();
    pad_is(0, 0x1000, 0, 0, 1, "pressed again: START");
    key(SDL_SCANCODE_RETURN, 0, 0);

    /* a controller: both stick clicks open it, the D-pad moves, A chooses, a binding takes the next button */
    attach(&pad, "menu pad");
    poll_pads();
    button(&pad, SDL_CONTROLLER_BUTTON_LEFTSTICK, 1);
    poll_pads();
    CHECK(!pc_menu_is_open(), "one stick click alone doesn't open it");
    button(&pad, SDL_CONTROLLER_BUTTON_RIGHTSTICK, 1);
    poll_pads();
    CHECK(pc_menu_is_open(), "both stick clicks open it");
    button(&pad, SDL_CONTROLLER_BUTTON_LEFTSTICK, 0);
    button(&pad, SDL_CONTROLLER_BUTTON_RIGHTSTICK, 0);
    pump();
    menu_at("Settings", "Resume", NULL, "where it was");
    for (int i = 0; i < 4; i++)
        press(&pad, SDL_CONTROLLER_BUTTON_DPAD_UP);
    menu_at("Settings", "Controllers", NULL, "D-pad up four times");
    press(&pad, SDL_CONTROLLER_BUTTON_A);
    menu_at("Controllers (all players)", "A", "a, righttrigger", "A opens the page");
    press(&pad, SDL_CONTROLLER_BUTTON_A);
    CHECK(pc_menu_capturing() == PC_MENU_CAPTURE_PAD, "waiting for a controller input");
    press(&pad, SDL_CONTROLLER_BUTTON_Y);
    menu_at("Controllers (all players)", "A", "y", "Y captured");
    press(&pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    press(&pad, SDL_CONTROLLER_BUTTON_A);
    axis(&pad, SDL_CONTROLLER_AXIS_LEFTX, -32768);
    pump();
    axis(&pad, SDL_CONTROLLER_AXIS_LEFTX, 0);
    pump();
    menu_at("Controllers (all players)", "B", "-leftx", "a stick direction captured");
    press(&pad, SDL_CONTROLLER_BUTTON_B);
    menu_at("Settings", "Controllers", NULL, "B goes back");
    button(&pad, SDL_CONTROLLER_BUTTON_START, 1);
    poll_pads();
    CHECK(!pc_menu_is_open(), "START closes it");
    pad_is(0, 0, 0, 0, 1, "START still held from the overlay");
    button(&pad, SDL_CONTROLLER_BUTTON_START, 0);
    poll_pads();
    button(&pad, SDL_CONTROLLER_BUTTON_START, 1);
    button(&pad, SDL_CONTROLLER_BUTTON_Y, 1);
    poll_pads();
    pad_is(0, 0x9008, 0, 0, 1, "START again; Y is A now, and still C-up");
    button(&pad, SDL_CONTROLLER_BUTTON_START, 0);
    button(&pad, SDL_CONTROLLER_BUTTON_Y, 0);
    poll_pads();

    /* guide alone as the menu button: it opens and closes it, and isn't a game button. SDL holds back a guide
     * release that comes within 250 ms of the press (and the next press with it), so it is held for 300 ms. */
    snprintf(s.pad_menu, PC_CFG_BIND, "guide");
    restart_input(&s);
    poll_pads();
    for (int i = 0; i < 3; i++) {
        button(&pad, SDL_CONTROLLER_BUTTON_GUIDE, 1);
        poll_pads();
        pad_is(0, 0, 0, 0, 1, "guide isn't a game button");
        CHECK(pc_menu_is_open() == (i != 1), "guide %s it", i == 1 ? "closes" : "opens");
        SDL_Delay(300);
        button(&pad, SDL_CONTROLLER_BUTTON_GUIDE, 0);
        pump();
    }

    /* fullscreen from the overlay, at once */
    CHECK(pc_menu_is_open(), "open again");
    press(&pad, SDL_CONTROLLER_BUTTON_A);
    menu_at("Video", "Render scale", "native", "Video");
    press(&pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    press(&pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    press(&pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    menu_at("Video", "Fullscreen", "on", "fullscreen on");
    CHECK(SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN, "the window is fullscreen");
    tap(SDL_SCANCODE_F11);
    CHECK(!(SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN), "F11 still works in the overlay");
    tap(SDL_SCANCODE_F1);
    CHECK(!pc_menu_is_open(), "F1 closes it");
    tap(SDL_SCANCODE_F1);
    press(&pad, SDL_CONTROLLER_BUTTON_A);
    menu_at("Video", "Fullscreen", "off", "each page keeps its place; the overlay follows F11");
    tap(SDL_SCANCODE_F1);
    detach(&pad);
    stop();
}

/* ---------------------------------------------------------------- the mouse */

/* A mouse event in the window's units (the offscreen window's are its pixels): motion, a button, or wheel notches. */
static void mouse(Uint32 type, int x, int y, int button)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = type;
    if (type == SDL_MOUSEMOTION) {
        e.motion.windowID = SDL_GetWindowID(g_win);
        e.motion.x = x, e.motion.y = y;
    } else if (type == SDL_MOUSEWHEEL) {
        e.wheel.windowID = SDL_GetWindowID(g_win);
        e.wheel.y = button;
        e.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    } else {
        e.button.windowID = SDL_GetWindowID(g_win);
        e.button.button = (Uint8)button;
        e.button.state = type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED;
        e.button.clicks = 1;
        e.button.x = x, e.button.y = y;
    }
    SDL_PushEvent(&e);
    pump();
}

static void click(int x, int y, int button)
{
    mouse(SDL_MOUSEBUTTONDOWN, x, y, button);
    mouse(SDL_MOUSEBUTTONUP, x, y, button);
}

static int cursor_shown(void)
{
    return SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE;
}

/* Row i's middle at 640 x 480: the panel at (16, 16) in units of 2 pixels, rows of 12 units from y 18. */
#define ROW_X 216
#define ROW_Y(i) (16 + (18 + 12 * (i) + 6) * 2)

static void test_mouse(void)
{
    PcSettings s;
    pc_settings_defaults(&s);
    test_now_ns = 1000000000u;
    if (start(&s) != 0) {
        g_fail++;
        return;
    }
    show_frame();
    pump();
    CHECK(!cursor_shown(), "no cursor over the game at start");
    /* during play: the cursor shows when the mouse moves, and hides 2 s after it stops; the game gets nothing */
    mouse(SDL_MOUSEMOTION, 100, 100, 0);
    CHECK(cursor_shown(), "the mouse moved: the cursor shows");
    click(100, 100, SDL_BUTTON_LEFT);
    click(100, 100, SDL_BUTTON_RIGHT);
    mouse(SDL_MOUSEWHEEL, 0, 0, 1);
    poll_pads();
    pad_is(0, 0, 0, 0, 1, "clicks and the wheel during play");
    CHECK(!pc_menu_is_open() && !pc_input_paused(), "clicks don't open the overlay");
    test_now_ns += 1500000000u;
    pump();
    CHECK(cursor_shown(), "1.5 s later it is still there");
    mouse(SDL_MOUSEMOTION, 110, 100, 0);
    test_now_ns += 1000000000u;
    pump();
    CHECK(cursor_shown(), "moved again: 1 s since");
    test_now_ns += 1100000000u;
    pump();
    CHECK(!cursor_shown(), "2 s without a move: hidden");
    mouse(SDL_MOUSEMOTION, 110, 100, 0);
    CHECK(!cursor_shown(), "an event at the same place isn't a move");

    /* the overlay: the cursor shows at once; pointing selects, a click chooses */
    tap(SDL_SCANCODE_F1);
    CHECK(pc_menu_is_open() && cursor_shown(), "F1: the overlay, and the cursor with it");
    test_now_ns += 5000000000u;
    pump();
    CHECK(cursor_shown(), "the cursor stays while it is open");
    mouse(SDL_MOUSEMOTION, ROW_X, ROW_Y(2), 0);
    menu_at("Settings", "Keyboard", NULL, "pointing at row 2");
    click(ROW_X, ROW_Y(2), SDL_BUTTON_LEFT);
    menu_at("Keyboard", "A", "X", "a click opens it");
    click(ROW_X, ROW_Y(1), SDL_BUTTON_LEFT);
    CHECK(pc_menu_capturing() == PC_MENU_CAPTURE_KEY, "a click on B waits for a key");
    tap(SDL_SCANCODE_V);
    menu_at("Keyboard", "B", "V", "the key it got");
    click(ROW_X, ROW_Y(1), SDL_BUTTON_X1);
    menu_at("Settings", "Keyboard", NULL, "the mouse's back button");
    mouse(SDL_MOUSEMOTION, ROW_X, ROW_Y(1), 0);
    click(ROW_X, ROW_Y(1), SDL_BUTTON_LEFT);
    menu_at("Audio", "Volume", "100 %", "Audio");
    mouse(SDL_MOUSEWHEEL, 0, 0, -1);
    menu_at("Audio", "Volume", "100 %", "the wheel doesn't change a value");
    click(ROW_X, ROW_Y(1), SDL_BUTTON_RIGHT);
    menu_at("Settings", "Audio", NULL, "the right button goes back");

    /* a click just after the window got the focus back is the one that brought it: not the overlay's */
    window_event(SDL_WINDOWEVENT_FOCUS_GAINED);
    pump();
    click(4, 4, SDL_BUTTON_LEFT);
    CHECK(pc_menu_is_open(), "the focusing click outside the panel doesn't close it");
    test_now_ns += 300000000u;
    click(4, 4, SDL_BUTTON_LEFT);
    CHECK(!pc_menu_is_open() && !pc_input_paused(), "a click outside on the first page closes it");
    poll_pads();
    pad_is(0, 0, 0, 0, 1, "and doesn't reach the game");
    CHECK(cursor_shown(), "closed: the cursor stays a moment");
    test_now_ns += 2100000000u;
    pump();
    CHECK(!cursor_shown(), "then hides");

    /* About: Copy puts the page on the clipboard (SDL keeps one on the offscreen driver); no Open without a desktop */
    tap(SDL_SCANCODE_F1);
    mouse(SDL_MOUSEMOTION, ROW_X, ROW_Y(5), 0);
    click(ROW_X, ROW_Y(5), SDL_BUTTON_LEFT);
    menu_at("About", "Version", "unknown", "About");
    tap(SDL_SCANCODE_UP);
    menu_at("About", "Copy to the clipboard", "", "its last item: Copy, no Open (offscreen)");
    SDL_SetClipboardText("");
    tap(SDL_SCANCODE_RETURN);
    char *clip = SDL_GetClipboardText();
    CHECK(clip && !strncmp(clip, "Rush 2049 PC\nVersion: unknown\n", 30) && strstr(clip, "\nPlayer 1: keyboard\n") &&
              strstr(clip, "\nGL renderer: "),
          "the clipboard:\n%s", clip ? clip : "(none)");
    SDL_free(clip);
    tap(SDL_SCANCODE_F1);
    stop();
}

int main(int argc, char **argv)
{
    static const struct { const char *name; void (*fn)(void); } cases[] = {
        {"keyboard", test_keyboard}, {"hotkeys", test_hotkeys}, {"focus", test_focus},
        {"controller", test_controller}, {"hotplug", test_hotplug}, {"generic", test_generic},
        {"rumble", test_rumble}, {"menu", test_menu}, {"mouse", test_mouse}, {"script", test_script}};
    if (argc < 3) {
        fprintf(stderr, "usage: frontend_sdl_test keyboard|hotkeys|focus|controller|hotplug|generic|rumble|menu|script|"
                        "all DIR\n");
        return 2;
    }
    g_dir = argv[2];
    mkdir(g_dir, 0755);
    int ran = 0;
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++)
        if (!strcmp(argv[1], "all") || !strcmp(argv[1], cases[i].name)) {
            if (!strcmp(cases[i].name, "script") && strcmp(argv[1], "script"))
                continue;   /* scripts can't be removed once added: only on its own */
            cases[i].fn();
            ran++;
        }
    if (!ran) {
        fprintf(stderr, "no case %s\n", argv[1]);
        return 2;
    }
    printf("%s: %d checks, %d failed\n", argv[1], g_checks, g_fail);
    return g_fail ? 1 : 0;
}
