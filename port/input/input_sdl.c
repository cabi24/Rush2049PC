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
 * - Window: Alt+Enter and the fullscreen hotkey toggle fullscreen (port/gfx: desktop or exclusive, as
 *   video.fullscreen_mode says); the screenshot hotkey saves a PNG of
 *   the shown frame; the pause hotkey, and losing focus with pause_on_focus_loss, hold the game (the main loop
 *   asks pc_input_paused); the quit hotkey quits. Hotkeys never reach the game.
 * - The settings overlay (menu.c): the menu hotkey (F1) or the controller's menu button (controller.menu, both
 *   stick clicks; a button alone, or buttons held together) opens it. While
 *   it is open its keys and buttons go to it and not to the game, and a binding item gets the next key or
 *   controller input. When it closes, what is still held is ignored until it is released, so the key or button
 *   that closed it doesn't reach the game.
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

/* The overlay's button: one input, or several held together ("leftstick+rightstick"). */
#define MAX_COMBO 4
typedef struct {
    PadSource part[MAX_COMBO];
    int n;
} Combo;

typedef struct {
    SDL_GameController *gc;
    SDL_JoystickID id;
    double rumble;
    Uint32 rumble_sent;
    uint32_t mute_buttons;   /* held when the overlay closed: ignored until released */
    uint8_t mute_axes;
    uint8_t menu_held;       /* the overlay's combos held on this pad, one bit each */
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
    Combo menu_pad[MAX_SOURCES];       /* controller.menu */
    int nmenu_pad;
    SDL_Scancode hot[PC_NUM_HOTKEYS][MAX_SOURCES];
    int nhot[PC_NUM_HOTKEYS];
    uint8_t down[SDL_NUM_SCANCODES];   /* the game's keys */
    uint8_t held[SDL_NUM_SCANCODES];   /* every key, the overlay's included */
    uint8_t mute[SDL_NUM_SCANCODES];   /* held when the overlay closed: ignored until released */
    Slot slot[4];
    int paused_focus, paused_key;
    int nav_dir;                       /* the overlay: a held direction (PC_MENU_UP...), -1 for none */
    uint64_t nav_next;                 /* when it repeats */
    int stick_dir[2];                  /* where the left stick's x and y were last pushed, -1 for the middle */
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

/* A key's name for the file, the way it reads back: SDL's name, or the alias for punctuation. NULL for a key with
 * no name a list can hold. */
static const char *key_name(SDL_Scancode sc)
{
    const char *name = SDL_GetScancodeName(sc);
    for (size_t i = 0; i < sizeof key_aliases / sizeof *key_aliases && name[0] && !name[1]; i++)
        if (key_aliases[i].sc == sc)
            name = key_aliases[i].name;
    if (!name[0] || strpbrk(name, ",;#\"") || key_from_name(name) != sc)
        return NULL;
    return name;
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

/* "a+b": parts joined by '+', where a '+' that starts a part is an axis sign ("leftstick++rightx"). */
static int add_menu_pad(const char *name, int index, int slot)
{
    Combo *c = &L.menu_pad[slot];
    char part[64];
    c->n = 0;
    for (const char *p = name; *p;) {
        const char *e = strchr(p + 1, '+');
        size_t n = e ? (size_t)(e - p) : strlen(p);
        if (c->n == MAX_COMBO || n >= sizeof part)
            return -1;
        snprintf(part, sizeof part, "%.*s", (int)n, p);
        if (pad_from_name(part, &c->part[c->n++]) != 0)
            return -1;
        p += n + (e != NULL);
    }
    return c->n ? 0 : -1;
}

static int hotkey_of(SDL_Scancode sc)
{
    for (int h = 0; h < PC_NUM_HOTKEYS; h++)
        for (int i = 0; i < L.nhot[h]; i++)
            if (L.hot[h][i] == sc)
                return h;
    return -1;
}

/* The settings live input follows, at start and from the overlay; the players are set at start only. */
static void configure(const PcSettings *s)
{
    L.analog = s->analog;
    L.pause_on_focus = s->pause_on_focus_loss;
    L.threshold = s->axis_threshold;
    L.shape = (PcStickShape){s->deadzone, s->stick_range, s->stick_diagonal};
    snprintf(L.shot_dir, sizeof L.shot_dir, "%s", s->screenshot_dir);
    for (int c = 0; c < PC_NUM_CONTROLS; c++) {
        parse_list(s->key[c], "keyboard", pc_control_names[c], add_key, c, &L.nkeys[c]);
        parse_list(s->pad[c], "controller", pc_control_names[c], add_pad, c, &L.npads[c]);
    }
    parse_list(s->pad_menu, "controller", "menu", add_menu_pad, 0, &L.nmenu_pad);
    for (int h = 0; h < PC_NUM_HOTKEYS; h++)
        parse_list(s->hotkey[h], "hotkeys", pc_hotkey_names[h], add_hotkey, h, &L.nhot[h]);
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
        L.slot[p] = (Slot){.gc = gc, .id = id};
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

static int same_source(const PadSource *a, const PadSource *b)
{
    return a->axis == b->axis && a->id == b->id && (!a->axis || a->sign == b->sign);
}

/* The overlay's button when it is one input alone: not a game button. (A combo's parts stay game buttons; while
 * the whole combo is held the overlay is open and the game sees nothing anyway.) */
static int is_menu_source(const PadSource *s)
{
    for (int i = 0; i < L.nmenu_pad; i++)
        if (L.menu_pad[i].n == 1 && same_source(&L.menu_pad[i].part[0], s))
            return 1;
    return 0;
}

/* After a controller event: did one of the overlay's combos just become held on that pad? */
static int menu_combo_pressed(SDL_JoystickID id)
{
    int pressed = 0;
    for (int p = 0; p < 4; p++) {
        Slot *slot = &L.slot[p];
        uint8_t held = 0;
        if (!slot->gc || slot->id != id)
            continue;
        for (int i = 0; i < L.nmenu_pad && i < 8; i++) {
            int all = 1;
            for (int j = 0; j < L.menu_pad[i].n && all; j++) {
                const PadSource *s = &L.menu_pad[i].part[j];
                all = s->axis ? SDL_GameControllerGetAxis(slot->gc, (SDL_GameControllerAxis)s->id) * s->sign > 19660
                              : SDL_GameControllerGetButton(slot->gc, (SDL_GameControllerButton)s->id);
            }
            held |= (uint8_t)(all << i);
        }
        pressed = (held & ~slot->menu_held) != 0;
        slot->menu_held = held;
    }
    return pressed;
}

static int source_active(const Slot *slot, const PadSource *s)
{
    if (is_menu_source(s))
        return 0;   /* the overlay's button is not a game button */
    if (!s->axis)
        return !(slot->mute_buttons >> s->id & 1) &&
               SDL_GameControllerGetButton(slot->gc, (SDL_GameControllerButton)s->id) != 0;
    return !(slot->mute_axes >> s->id & 1) &&
           pc_axis_deadzone(axis_value(slot->gc, s->id), L.shape.deadzone) * s->sign > L.threshold;
}

/* A pad's buttons and axes still held from the overlay count once they are released (an axis back in its dead
 * zone). */
static void unmute(Slot *slot)
{
    for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX && slot->mute_buttons; b++)
        if (!SDL_GameControllerGetButton(slot->gc, (SDL_GameControllerButton)b))
            slot->mute_buttons &= ~(1u << b);
    for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX && slot->mute_axes; a++)
        if (pc_axis_deadzone(axis_value(slot->gc, a), L.shape.deadzone) == 0)
            slot->mute_axes &= (uint8_t)~(1u << a);
}

/* ---------------------------------------------------------------- window actions */

static void set_title(void)
{
    SDL_SetWindowTitle(L.win, L.paused_focus || L.paused_key || pc_menu_is_open()
                                  ? "San Francisco Rush 2049 (paused)"
                                  : "San Francisco Rush 2049");
}

static void set_fullscreen(int on)
{
    pc_gfx_set_fullscreen(on);   /* port/gfx owns the window's modes (GFX.md "Window and display") */
}

static void toggle_fullscreen(void)
{
    pc_gfx_toggle_fullscreen();
}

static void screenshot(void)
{
    char path[PC_CFG_PATH + 64], stamp[32];
    struct stat st;
    time_t t = time(NULL);
    struct tm tm;
#ifdef _WIN32
    tm = *localtime(&t);   /* no localtime_r; this is the main thread's only caller */
#else
    localtime_r(&t, &tm);
#endif
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

/* ---------------------------------------------------------------- the overlay's input */

/* A step the overlay takes from a direction; held, it repeats (pc_input_live_tick). */
static void nav(int dir)
{
    pc_menu_action(dir);
    L.nav_dir = dir;
    L.nav_next = pc_host_ns() + 400000000u;
}

/* A key while the overlay is open. Its own keys, the menu key, and the fullscreen and screenshot hotkeys work; the
 * game's keys, pause and quit don't (Escape is the overlay's back key). A binding item takes the next key. */
static void menu_key(const SDL_Event *e, SDL_Scancode sc, int first)
{
    int capture = pc_menu_capturing(), h = hotkey_of(sc);
    if (capture) {   /* Escape cancels; a key binding takes any other key, once */
        if (first && sc == SDL_SCANCODE_ESCAPE)
            pc_menu_capture(NULL);
        else if (first && capture == PC_MENU_CAPTURE_KEY && key_name(sc))
            pc_menu_capture(key_name(sc));
        return;
    }
    if ((sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) && (e->key.keysym.mod & KMOD_ALT)) {
        if (first)
            toggle_fullscreen();
        return;
    }
    if (h == PC_HOTKEY_MENU || h == PC_HOTKEY_FULLSCREEN || h == PC_HOTKEY_SCREENSHOT) {
        if (first && h == PC_HOTKEY_MENU)
            pc_menu_action(PC_MENU_CLOSE);
        else if (first && h == PC_HOTKEY_FULLSCREEN)
            toggle_fullscreen();
        else if (first)
            screenshot();
        return;
    }
    switch (sc) {   /* the moving keys repeat while held */
    case SDL_SCANCODE_UP: pc_menu_action(PC_MENU_UP); break;
    case SDL_SCANCODE_DOWN: pc_menu_action(PC_MENU_DOWN); break;
    case SDL_SCANCODE_LEFT: pc_menu_action(PC_MENU_LEFT); break;
    case SDL_SCANCODE_RIGHT: pc_menu_action(PC_MENU_RIGHT); break;
    case SDL_SCANCODE_PAGEUP: pc_menu_action(PC_MENU_PAGE_UP); break;
    case SDL_SCANCODE_PAGEDOWN: pc_menu_action(PC_MENU_PAGE_DOWN); break;
    default:
        if (!first)
            break;
        if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER || sc == SDL_SCANCODE_SPACE)
            pc_menu_action(PC_MENU_ACCEPT);
        else if (sc == SDL_SCANCODE_ESCAPE)
            pc_menu_action(PC_MENU_BACK);
        else if (sc == SDL_SCANCODE_BACKSPACE || sc == SDL_SCANCODE_DELETE)
            pc_menu_action(PC_MENU_CLEAR);
        else if (sc == SDL_SCANCODE_INSERT)
            pc_menu_action(PC_MENU_ADD);
    }
}

/* A controller button while the overlay is open: the D-pad moves, A accepts, B goes back, X clears, Y adds,
 * the shoulders move a page, START and the menu button close. A binding item for the controller takes it. */
static void menu_button(SDL_GameControllerButton b, int menu)
{
    int capture = pc_menu_capturing();
    if (capture) {   /* the menu button cancels, and B a key capture */
        if (menu || (capture != PC_MENU_CAPTURE_PAD && b == SDL_CONTROLLER_BUTTON_B))
            pc_menu_capture(NULL);
        else if (capture == PC_MENU_CAPTURE_PAD)
            pc_menu_capture(SDL_GameControllerGetStringForButton(b));
        return;
    }
    if (menu) {
        pc_menu_action(PC_MENU_CLOSE);
        return;
    }
    switch (b) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: nav(PC_MENU_UP); break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: nav(PC_MENU_DOWN); break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: nav(PC_MENU_LEFT); break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: nav(PC_MENU_RIGHT); break;
    case SDL_CONTROLLER_BUTTON_A: pc_menu_action(PC_MENU_ACCEPT); break;
    case SDL_CONTROLLER_BUTTON_B: pc_menu_action(PC_MENU_BACK); break;
    case SDL_CONTROLLER_BUTTON_X: pc_menu_action(PC_MENU_CLEAR); break;
    case SDL_CONTROLLER_BUTTON_Y: pc_menu_action(PC_MENU_ADD); break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: pc_menu_action(PC_MENU_PAGE_UP); break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: pc_menu_action(PC_MENU_PAGE_DOWN); break;
    case SDL_CONTROLLER_BUTTON_START: pc_menu_action(PC_MENU_CLOSE); break;
    default: break;
    }
}

/* A held direction on any controller: the D-pad, or the left stick past half its travel. */
static int pad_direction(void)
{
    for (int p = 0; p < 4; p++) {
        SDL_GameController *gc = L.slot[p].gc;
        if (!gc)
            continue;
        double x = axis_value(gc, SDL_CONTROLLER_AXIS_LEFTX), y = axis_value(gc, SDL_CONTROLLER_AXIS_LEFTY);
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_UP) || y < -0.5)
            return PC_MENU_UP;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || y > 0.5)
            return PC_MENU_DOWN;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || x < -0.5)
            return PC_MENU_LEFT;
        if (SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || x > 0.5)
            return PC_MENU_RIGHT;
    }
    return -1;
}

/* A controller axis while the overlay is open: the left stick moves (once per push; holding it repeats), and a
 * binding item for the controller takes a stick direction or a trigger pushed past 0.6. */
static void menu_axis(SDL_GameControllerAxis a, int value, int menu)
{
    double v = value / 32767.0;
    int capture = pc_menu_capturing();
    if (menu) {
        pc_menu_action(capture ? PC_MENU_BACK : PC_MENU_CLOSE);
        return;
    }
    if (capture == PC_MENU_CAPTURE_PAD) {
        char name[32];
        if (v > 0.6 || v < -0.6) {
            int trigger = a == SDL_CONTROLLER_AXIS_TRIGGERLEFT || a == SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
            snprintf(name, sizeof name, "%s%s", trigger ? "" : v < 0 ? "-" : "+",
                     SDL_GameControllerGetStringForAxis(a));
            pc_menu_capture(name);
        }
        return;
    }
    if (capture || (a != SDL_CONTROLLER_AXIS_LEFTX && a != SDL_CONTROLLER_AXIS_LEFTY))
        return;
    int y = a == SDL_CONTROLLER_AXIS_LEFTY, dir = L.stick_dir[y];
    if (v < -0.5)
        dir = y ? PC_MENU_UP : PC_MENU_LEFT;
    else if (v > 0.5)
        dir = y ? PC_MENU_DOWN : PC_MENU_RIGHT;
    else if (v > -0.3 && v < 0.3)
        dir = -1;   /* back to the middle; between 0.3 and 0.5 it keeps its direction */
    if (dir >= 0 && dir != L.stick_dir[y])
        nav(dir);
    L.stick_dir[y] = dir;
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
        L.held[sc] = (uint8_t)down;
        if (!down)
            L.mute[sc] = 0;
        if (pc_menu_is_open()) {
            if (down)
                menu_key(e, sc, first);
            break;
        }
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
                else if (h == PC_HOTKEY_MENU)
                    pc_menu_action(PC_MENU_TOGGLE);
            }
            break;
        }
        if (down && L.mute[sc])
            break;   /* held since the overlay closed */
        L.down[sc] = (uint8_t)down;
        break;
    }
    case SDL_CONTROLLERBUTTONDOWN: {
        int menu = menu_combo_pressed(e->cbutton.which);
        if (pc_menu_is_open())
            menu_button((SDL_GameControllerButton)e->cbutton.button, menu);
        else if (menu)
            pc_menu_action(PC_MENU_TOGGLE);
        break;
    }
    case SDL_CONTROLLERBUTTONUP:
        menu_combo_pressed(e->cbutton.which);   /* a combo let go can be pressed again */
        break;
    case SDL_CONTROLLERAXISMOTION: {
        int menu = menu_combo_pressed(e->caxis.which);
        if (pc_menu_is_open())
            menu_axis((SDL_GameControllerAxis)e->caxis.axis, e->caxis.value, menu);
        else if (menu)
            pc_menu_action(PC_MENU_TOGGLE);
        break;
    }
    case SDL_WINDOWEVENT:
        switch (e->window.event) {
        case SDL_WINDOWEVENT_FOCUS_LOST:
            memset(L.down, 0, sizeof L.down);   /* key-up events go to the new focus */
            memset(L.held, 0, sizeof L.held);
            memset(L.mute, 0, sizeof L.mute);
            if (L.pause_on_focus)
                set_pause(&L.paused_focus, 1, "focus lost");
            break;
        case SDL_WINDOWEVENT_FOCUS_GAINED:
            set_pause(&L.paused_focus, 0, "focus back");
            break;
        case SDL_WINDOWEVENT_EXPOSED:
        case SDL_WINDOWEVENT_SIZE_CHANGED:
            if (L.paused_focus || L.paused_key || pc_menu_is_open())
                pc_gfx_redraw();   /* the main loop is holding: show the last frame again */
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
    L.nav_dir = L.stick_dir[0] = L.stick_dir[1] = -1;
    if (!SDL_WasInit(SDL_INIT_VIDEO) || pc_gfx_set_event_hook(pc_input_live_event) != 0)
        return -1;   /* no window: scripted input only */
    L.win = SDL_GL_GetCurrentWindow();
    if (!L.win) {
        pc_gfx_set_event_hook(NULL);
        return -1;
    }
    L.kb_port = s->keyboard_player - 1;
    L.first_port = s->first_controller_player - 1;
    configure(s);

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
    pc_gfx_set_vsync(s->vsync);
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

void pc_input_live_apply(const PcSettings *s)
{
    if (L.on)
        configure(s);
}

void pc_input_live_menu(int open)
{
    if (!L.on)
        return;
    L.nav_dir = L.stick_dir[0] = L.stick_dir[1] = -1;
    if (open)
        memset(L.down, 0, sizeof L.down);   /* the game's keys are released while it is open */
    else {
        memcpy(L.mute, L.held, sizeof L.mute);
        for (int p = 0; p < 4; p++) {
            Slot *slot = &L.slot[p];
            if (!slot->gc)
                continue;
            for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++)
                if (SDL_GameControllerGetButton(slot->gc, (SDL_GameControllerButton)b))
                    slot->mute_buttons |= 1u << b;
            for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; a++)
                if (pc_axis_deadzone(axis_value(slot->gc, a), L.shape.deadzone) != 0)
                    slot->mute_axes |= (uint8_t)(1u << a);
        }
    }
    set_title();
}

void pc_input_live_tick(void)
{
    if (!L.on || !pc_menu_is_open() || pc_menu_capturing()) {
        L.nav_dir = -1;
        return;
    }
    int dir = pad_direction();
    uint64_t now = pc_host_ns();
    if (dir < 0 || dir != L.nav_dir)
        L.nav_dir = -1;   /* the first step comes from the event */
    else if (now >= L.nav_next) {
        pc_menu_action(dir);
        L.nav_next = now + 100000000u;
    }
}

void pc_input_live_fullscreen(int on)
{
    if (L.on)
        set_fullscreen(on);
}

int pc_input_live_is_fullscreen(void)
{
    return L.on ? (SDL_GetWindowFlags(L.win) & SDL_WINDOW_FULLSCREEN) != 0 : -1;
}

void pc_input_live_vsync(int on)
{
    if (L.on)
        pc_gfx_set_vsync(on);
}

void pc_input_live_window_size(int w, int h)
{
    pc_gfx_set_window_pixels(w, h);   /* pixels at any display scaling; headless, the size the scales follow */
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
        Slot *slot = &L.slot[p];
        SDL_GameController *gc = slot->gc;
        int kb = p == L.kb_port, held[PC_NUM_CONTROLS] = {0};
        if (!kb && !gc)
            continue;
        if (gc)
            unmute(slot);
        for (int c = 0; c < PC_NUM_CONTROLS; c++) {
            for (int i = 0; kb && i < L.nkeys[c] && !held[c]; i++)
                held[c] = L.down[L.keys[c][i]];
            for (int i = 0; gc && i < L.npads[c] && !held[c]; i++)
                held[c] = source_active(slot, &L.pads[c][i]);
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
            if (!(slot->mute_axes & (3u << ox)))
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
void pc_input_live_apply(const PcSettings *s) { (void)s; }
void pc_input_live_menu(int open) { (void)open; }
void pc_input_live_tick(void) {}
void pc_input_live_fullscreen(int on) { (void)on; }
int pc_input_live_is_fullscreen(void) { return -1; }
void pc_input_live_vsync(int on) { (void)on; }
void pc_input_live_window_size(int w, int h) { pc_gfx_set_window_size(w, h); }

#endif
