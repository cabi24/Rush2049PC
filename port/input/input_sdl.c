/* port/input/input_sdl.c: live input and the window's keys, in SDL2 builds (docs/port/FRONTEND.md).
 *
 * port/gfx owns the SDL window and pumps its events in pc_gfx_poll_events; each event reaches
 * pc_input_live_event through pc_gfx_set_event_hook, so there is one event loop. Keys are tracked from those
 * events (not SDL_GetKeyboardState), which also lets tests inject them with SDL_PushEvent. Game controllers
 * (SDL's GameController API: any XInput-style pad) are read at each retrace from SDL's state.
 *
 * - Players: the keyboard is one player (keyboard_player); controllers take players from first_controller_player
 *   up, in the order they are plugged in, and keep their player until unplugged (hot-plug). A player's keyboard
 *   and controller are merged: buttons OR'ed, the stronger stick per axis.
 * - The N64 stick: the bound controller stick with a radial dead zone, rescaled, shaped to the octagonal gate
 *   (stick.c); keys and buttons bound to STICK_* give full tilt.
 * - Window: Alt+Enter and the fullscreen hotkey toggle desktop fullscreen; the screenshot hotkey saves a PNG of
 *   the shown frame; the pause hotkey, and losing focus with pause_on_focus_loss, hold the game (the main loop
 *   asks pc_input_paused); the quit hotkey quits. Hotkeys never reach the game.
 * - Rumble: the rumble pak's motor (port/ultra/si.c) drives SDL_GameControllerRumble on that player's pad. */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "input_internal.h"

#ifdef PC_HAVE_SDL2

#include <SDL.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_SOURCES 8

typedef struct {
    uint8_t axis;   /* 0: button */
    uint8_t id;     /* SDL_GameControllerButton, or SDL_GameControllerAxis */
    int8_t sign;    /* axis direction that presses */
} PadSource;

typedef struct {
    SDL_GameController *gc;
    SDL_JoystickID id;
    double rumble;
    Uint32 rumble_sent;
} Slot;

static struct {
    int on;
    SDL_Window *win;
    int kb_port, first_port, analog, pause_on_focus;
    double threshold;
    PcStickShape shape;
    char shot_dir[PC_CFG_PATH];
    SDL_Scancode keys[PC_NUM_CONTROLS][MAX_SOURCES];
    int nkeys[PC_NUM_CONTROLS];
    PadSource pads[PC_NUM_CONTROLS][MAX_SOURCES];
    int npads[PC_NUM_CONTROLS];
    SDL_Scancode hot[PC_NUM_HOTKEYS][MAX_SOURCES];
    int nhot[PC_NUM_HOTKEYS];
    uint8_t down[SDL_NUM_SCANCODES];
    Slot slot[4];
    int paused_focus, paused_key;
} L;

/* ---------------------------------------------------------------- binding names */

/* Names for keys whose SDL name is punctuation, which a comma-separated list or a comment can't hold. */
static const struct { const char *name; SDL_Scancode sc; } key_aliases[] = {
    {"Comma", SDL_SCANCODE_COMMA},        {"Period", SDL_SCANCODE_PERIOD},
    {"Slash", SDL_SCANCODE_SLASH},        {"Semicolon", SDL_SCANCODE_SEMICOLON},
    {"Apostrophe", SDL_SCANCODE_APOSTROPHE}, {"Minus", SDL_SCANCODE_MINUS},
    {"Equals", SDL_SCANCODE_EQUALS},      {"LeftBracket", SDL_SCANCODE_LEFTBRACKET},
    {"RightBracket", SDL_SCANCODE_RIGHTBRACKET}, {"Backslash", SDL_SCANCODE_BACKSLASH},
    {"Grave", SDL_SCANCODE_GRAVE},        {"Hash", SDL_SCANCODE_NONUSHASH},
    {"Enter", SDL_SCANCODE_RETURN},       {"Esc", SDL_SCANCODE_ESCAPE},
};

static SDL_Scancode key_from_name(const char *name)
{
    for (size_t i = 0; i < sizeof key_aliases / sizeof *key_aliases; i++)
        if (!strcasecmp(name, key_aliases[i].name))
            return key_aliases[i].sc;
    return SDL_GetScancodeFromName(name);
}

static int pad_from_name(const char *name, PadSource *out)
{
    int sign = 1, signed_axis = *name == '+' || *name == '-';
    if (signed_axis)
        sign = *name++ == '-' ? -1 : 1;
    SDL_GameControllerButton b = SDL_GameControllerGetButtonFromString(name);
    if (b != SDL_CONTROLLER_BUTTON_INVALID && !signed_axis) {
        *out = (PadSource){0, (uint8_t)b, 1};
        return 0;
    }
    SDL_GameControllerAxis a = SDL_GameControllerGetAxisFromString(name);
    if (a == SDL_CONTROLLER_AXIS_INVALID)
        return -1;
    if ((a == SDL_CONTROLLER_AXIS_TRIGGERLEFT || a == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) && sign < 0)
        return -1;
    *out = (PadSource){1, (uint8_t)a, (int8_t)sign};
    return 0;
}

/* Split a comma-separated list; each name goes to add(), which returns -1 for an unknown name. */
static void parse_list(const char *list, const char *section, const char *key,
                       int (*add)(const char *name, int index, int slot), int index, int *count)
{
    char buf[PC_CFG_BIND], *save = NULL;
    snprintf(buf, sizeof buf, "%s", list);
    *count = 0;
    for (char *tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        while (*tok == ' ' || *tok == '\t')
            tok++;
        char *e = tok + strlen(tok);
        while (e > tok && (e[-1] == ' ' || e[-1] == '\t'))
            *--e = 0;
        if (!*tok)
            continue;
        if (*count == MAX_SOURCES) {
            fprintf(stderr, "rush2049pc: [%s] %s: more than %d bindings; '%s' ignored\n", section, key,
                    MAX_SOURCES, tok);
            continue;
        }
        if (add(tok, index, *count) == 0)
            ++*count;
        else
            fprintf(stderr, "rush2049pc: [%s] %s: unknown name '%s'; ignored\n", section, key, tok);
    }
}

static int add_key(const char *name, int index, int slot)
{
    SDL_Scancode sc = key_from_name(name);
    if (sc == SDL_SCANCODE_UNKNOWN)
        return -1;
    L.keys[index][slot] = sc;
    return 0;
}

static int add_hotkey(const char *name, int index, int slot)
{
    SDL_Scancode sc = key_from_name(name);
    if (sc == SDL_SCANCODE_UNKNOWN)
        return -1;
    L.hot[index][slot] = sc;
    return 0;
}

static int add_pad(const char *name, int index, int slot)
{
    return pad_from_name(name, &L.pads[index][slot]);
}

static int hotkey_of(SDL_Scancode sc)
{
    for (int h = 0; h < PC_NUM_HOTKEYS; h++)
        for (int i = 0; i < L.nhot[h]; i++)
            if (L.hot[h][i] == sc)
                return h;
    return -1;
}

/* ---------------------------------------------------------------- controllers */

static void assign_pads(void)
{
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (!SDL_IsGameController(i))
            continue;
        SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(i);
        int p, taken = 0;
        for (p = 0; p < 4; p++)
            taken |= L.slot[p].gc && L.slot[p].id == id;
        if (taken)
            continue;
        for (p = L.first_port; p < 4 && L.slot[p].gc; p++)
            ;
        if (p == 4)
            return;   /* every player has one: it waits until one is unplugged */
        SDL_GameController *gc = SDL_GameControllerOpen(i);
        if (!gc) {
            fprintf(stderr, "rush2049pc: controller %d: %s\n", i, SDL_GetError());
            continue;
        }
        L.slot[p] = (Slot){gc, id, 0, 0};
        SDL_GameControllerSetPlayerIndex(gc, p);
        fprintf(stderr, "rush2049pc: controller -> player %d: %s\n", p + 1, SDL_GameControllerName(gc));
    }
}

static void remove_pad(SDL_JoystickID id)
{
    for (int p = 0; p < 4; p++)
        if (L.slot[p].gc && L.slot[p].id == id) {
            fprintf(stderr, "rush2049pc: controller left player %d\n", p + 1);
            SDL_GameControllerClose(L.slot[p].gc);
            memset(&L.slot[p], 0, sizeof L.slot[p]);
        }
}

static double axis_value(SDL_GameController *gc, int axis)
{
    double v = SDL_GameControllerGetAxis(gc, (SDL_GameControllerAxis)axis) / 32767.0;
    return v < -1 ? -1 : v;
}

static int source_active(SDL_GameController *gc, const PadSource *s)
{
    if (!s->axis)
        return SDL_GameControllerGetButton(gc, (SDL_GameControllerButton)s->id) != 0;
    return pc_axis_deadzone(axis_value(gc, s->id), L.shape.deadzone) * s->sign > L.threshold;
}

/* ---------------------------------------------------------------- window actions */

static void set_title(void)
{
    SDL_SetWindowTitle(L.win, L.paused_focus || L.paused_key ? "San Francisco Rush 2049 (paused)"
                                                              : "San Francisco Rush 2049");
}

static void set_fullscreen(int on)
{
    if (SDL_SetWindowFullscreen(L.win, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0)
        fprintf(stderr, "rush2049pc: fullscreen: %s\n", SDL_GetError());
}

static void toggle_fullscreen(void)
{
    set_fullscreen(!(SDL_GetWindowFlags(L.win) & SDL_WINDOW_FULLSCREEN));
}

static void screenshot(void)
{
    char path[PC_CFG_PATH + 64], stamp[32];
    struct stat st;
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tm);
    const char *dir = L.shot_dir[0] ? L.shot_dir : ".";
    snprintf(path, sizeof path, "%s/rush2049pc-%s.png", dir, stamp);
    for (int n = 2; stat(path, &st) == 0 && n < 1000; n++)
        snprintf(path, sizeof path, "%s/rush2049pc-%s-%d.png", dir, stamp, n);
    if (pc_gfx_save_screenshot(path) == 0)
        fprintf(stderr, "rush2049pc: screenshot: %s\n", path);
    else
        fprintf(stderr, "rush2049pc: no screenshot (%s; nothing shown yet, or the directory is missing)\n", path);
}

static void set_pause(int *flag, int on, const char *why)
{
    int was = L.paused_focus || L.paused_key;
    *flag = on;
    int now = L.paused_focus || L.paused_key;
    if (was != now) {
        fprintf(stderr, "rush2049pc: %s (%s)\n", now ? "paused" : "running", why);
        set_title();
    }
}

/* ---------------------------------------------------------------- the interface */

int pc_input_live_event(const void *event)
{
    const SDL_Event *e = event;
    if (!L.on)
        return 0;
    switch (e->type) {
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
        SDL_Scancode sc = e->key.keysym.scancode;
        int down = e->type == SDL_KEYDOWN, first = down && !e->key.repeat;
        if ((unsigned)sc >= SDL_NUM_SCANCODES)
            break;
        if ((sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) && (e->key.keysym.mod & KMOD_ALT)) {
            if (first)
                toggle_fullscreen();
            break;   /* Alt+Enter is not START */
        }
        int h = hotkey_of(sc);
        if (h >= 0) {
            if (first) {
                if (h == PC_HOTKEY_FULLSCREEN)
                    toggle_fullscreen();
                else if (h == PC_HOTKEY_SCREENSHOT)
                    screenshot();
                else if (h == PC_HOTKEY_PAUSE)
                    set_pause(&L.paused_key, !L.paused_key, "pause key");
                else if (h == PC_HOTKEY_QUIT)
                    return 1;
            }
            break;
        }
        L.down[sc] = (uint8_t)down;
        break;
    }
    case SDL_WINDOWEVENT:
        switch (e->window.event) {
        case SDL_WINDOWEVENT_FOCUS_LOST:
            memset(L.down, 0, sizeof L.down);   /* key-up events go to the new focus */
            if (L.pause_on_focus)
                set_pause(&L.paused_focus, 1, "focus lost");
            break;
        case SDL_WINDOWEVENT_FOCUS_GAINED:
            set_pause(&L.paused_focus, 0, "focus back");
            break;
        case SDL_WINDOWEVENT_EXPOSED:
        case SDL_WINDOWEVENT_SIZE_CHANGED:
            if (L.paused_focus || L.paused_key)
                pc_gfx_present(NULL);   /* the main loop is holding: show the last frame again */
            break;
        }
        break;
    case SDL_CONTROLLERDEVICEADDED:
        assign_pads();
        break;
    case SDL_CONTROLLERDEVICEREMOVED:
        remove_pad(e->cdevice.which);
        assign_pads();   /* a waiting controller takes the free player */
        break;
    }
    return 0;
}

int pc_input_live_init(const PcSettings *s)
{
    char path[PC_CFG_PATH + 32];
    struct stat st;
    memset(&L, 0, sizeof L);
    if (!SDL_WasInit(SDL_INIT_VIDEO) || pc_gfx_set_event_hook(pc_input_live_event) != 0)
        return -1;   /* no window: scripted input only */
    L.win = SDL_GL_GetCurrentWindow();
    if (!L.win) {
        pc_gfx_set_event_hook(NULL);
        return -1;
    }
    L.kb_port = s->keyboard_player - 1;
    L.first_port = s->first_controller_player - 1;
    L.analog = s->analog;
    L.pause_on_focus = s->pause_on_focus_loss;
    L.threshold = s->axis_threshold;
    L.shape = (PcStickShape){s->deadzone, s->stick_range, s->stick_diagonal};
    snprintf(L.shot_dir, sizeof L.shot_dir, "%s", s->screenshot_dir);
    for (int c = 0; c < PC_NUM_CONTROLS; c++) {
        parse_list(s->key[c], "keyboard", pc_control_names[c], add_key, c, &L.nkeys[c]);
        parse_list(s->pad[c], "controller", pc_control_names[c], add_pad, c, &L.npads[c]);
    }
    for (int h = 0; h < PC_NUM_HOTKEYS; h++)
        parse_list(s->hotkey[h], "hotkeys", pc_hotkey_names[h], add_hotkey, h, &L.nhot[h]);

    /* As v5: controllers keep working while the window is in the background, and Nintendo-style pads keep
     * their buttons' positions (south is a), not their labels. */
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0)
        fprintf(stderr, "rush2049pc: no game controllers: %s\n", SDL_GetError());
    else if (s->source_dir[0]) {
        snprintf(path, sizeof path, "%s/gamecontrollerdb.txt", s->source_dir);
        if (stat(path, &st) == 0) {
            int n = SDL_GameControllerAddMappingsFromFile(path);
            fprintf(stderr, "rush2049pc: %s: %d controller mappings\n", path, n);
        }
    }
    L.on = 1;
    set_title();
    SDL_SetWindowMinimumSize(L.win, 320, 240);
    if (s->fullscreen)
        set_fullscreen(1);
    if (SDL_GL_SetSwapInterval(s->vsync ? 1 : 0) != 0)
        fprintf(stderr, "rush2049pc: vsync %s: %s\n", s->vsync ? "on" : "off", SDL_GetError());
    assign_pads();
    return 0;
}

void pc_input_live_shutdown(void)
{
    if (!L.on)
        return;
    pc_gfx_set_event_hook(NULL);
    for (int p = 0; p < 4; p++)
        if (L.slot[p].gc) {
            SDL_GameControllerRumble(L.slot[p].gc, 0, 0, 0);
            SDL_GameControllerClose(L.slot[p].gc);
        }
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    memset(&L, 0, sizeof L);
}

static int8_t stronger(int8_t a, int8_t b)
{
    return (a < 0 ? -a : a) >= (b < 0 ? -b : b) ? a : b;
}

void pc_input_live_poll(PcPad pads[4])
{
    if (!L.on)
        return;
    for (int p = 0; p < 4; p++) {
        SDL_GameController *gc = L.slot[p].gc;
        int kb = p == L.kb_port, held[PC_NUM_CONTROLS] = {0};
        if (!kb && !gc)
            continue;
        for (int c = 0; c < PC_NUM_CONTROLS; c++) {
            for (int i = 0; kb && i < L.nkeys[c] && !held[c]; i++)
                held[c] = L.down[L.keys[c][i]];
            for (int i = 0; gc && i < L.npads[c] && !held[c]; i++)
                held[c] = source_active(gc, &L.pads[c][i]);
            if (held[c])
                pads[p].buttons |= pc_control_masks[c];
        }
        /* keys and buttons on the stick give full tilt, without a dead zone */
        PcStickShape digital = L.shape;
        digital.deadzone = 0;
        int8_t x = 0, y = 0, ax = 0, ay = 0;
        pc_stick_shape(held[PC_CTL_STICK_RIGHT] - held[PC_CTL_STICK_LEFT],
                       held[PC_CTL_STICK_UP] - held[PC_CTL_STICK_DOWN], &digital, &x, &y);
        if (gc && L.analog != PC_CFG_STICK_NONE) {
            int ox = L.analog == PC_CFG_STICK_LEFT ? SDL_CONTROLLER_AXIS_LEFTX : SDL_CONTROLLER_AXIS_RIGHTX;
            pc_stick_shape(axis_value(gc, ox), -axis_value(gc, ox + 1), &L.shape, &ax, &ay);
        }
        pads[p].stick_x = stronger(x, ax);
        pads[p].stick_y = stronger(y, ay);
        pads[p].connected = 1;
    }
}

void pc_input_live_rumble(int port, double level)
{
    Slot *s = port >= 0 && port < 4 ? &L.slot[port] : NULL;
    if (!L.on || !s || !s->gc)
        return;
    if (level < 0.01)
        level = 0;
    Uint32 now = SDL_GetTicks();
    /* a change goes out at once; a steady rumble is renewed twice a second (each lasts one), so it stops by
     * itself if the game stops polling */
    if (level == s->rumble && (level == 0 || now - s->rumble_sent < 500))
        return;
    Uint16 v = (Uint16)(level > 1 ? 0xFFFF : level * 0xFFFF);
    SDL_GameControllerRumble(s->gc, v, v, level > 0 ? 1000 : 0);
    s->rumble = level;
    s->rumble_sent = now;
}

int pc_input_live_paused(void)
{
    return L.on && (L.paused_focus || L.paused_key);
}

#else /* no SDL2: scripted input only */

int pc_input_live_init(const PcSettings *s)
{
    (void)s;
    return -1;
}

void pc_input_live_shutdown(void) {}
void pc_input_live_poll(PcPad pads[4]) { (void)pads; }
void pc_input_live_rumble(int port, double level) { (void)port, (void)level; }
int pc_input_live_paused(void) { return 0; }
int pc_input_live_event(const void *event)
{
    (void)event;
    return 0;
}

#endif
