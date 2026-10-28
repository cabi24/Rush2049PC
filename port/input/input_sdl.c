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
 * - Rumble: each player's rumble pak motor (port/ultra/si.c) drives SDL_GameControllerRumble on that player's pad;
 *   a pad that can rumble gives its player a rumble pak where the accessory is auto (PcPad.rumble). */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "input_internal.h"

/* What the window's title ends with: the program and its version, set by main (port/ultra/ultra.h); NULL in the
 * tests. */
const char *pc_input_title_tag;

#ifdef PC_HAVE_SDL2

#include <SDL.h>
#include <math.h>
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
    int can_rumble;          /* SDL says it has a rumble motor */
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
    SDL_JoystickID skipped[16];        /* joysticks not used (no stick or buttons), said once */
    SDL_JoystickID generic[16];        /* joysticks given the generic mapping */
    Slot pending[8];                   /* generic ones waiting for their first button press */
    int nskipped, ngeneric, npending, waiting_said;
    int cursor;                        /* the mouse cursor is shown: while the overlay is open, or just moved */
    int mouse_x, mouse_y, mouse_seen;  /* where it was last, in the window's units */
    uint64_t mouse_ns;                 /* when it last moved; the cursor hides CURSOR_NS after */
    uint64_t focus_ns;                 /* when the window got the focus back (0: not since start) */
} L;

#define CURSOR_NS 2000000000u          /* during play the cursor hides 2 s after it last moved */
#define FOCUS_CLICK_NS 250000000u      /* a click this soon after the focus came back is the one that brought it */

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
    L.shape = (PcStickShape){s->deadzone, s->stick_range, s->stick_diagonal, s->sensitivity};
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

/* A joystick SDL has no GameController mapping for (a DirectInput pad, an N64-style USB pad, a wheel) gets a
 * generic one, so that it plays instead of being passed over: the first two axes are the stick, the next two the
 * right stick, the first hat the D-pad, and the buttons in XInput's order (a b x y, the shoulders, back, start,
 * the stick clicks), then guide and the extras, so every button can be bound in the overlay. Two more axes are
 * the triggers from their middle up, so an axis that rests in the middle reads released. A line for the pad in
 * gamecontrollerdb.txt (next to rush2049pc.ini) is used instead. Only for something with a stick or a hat, and
 * buttons. Returns 1 when it made one. */
static int generic_mapping(int index)
{
    static const char *const buttons[] = {"a", "b", "x", "y", "leftshoulder", "rightshoulder", "back", "start",
                                          "leftstick", "rightstick", "guide", "misc1", "paddle1", "paddle2",
                                          "paddle3", "paddle4", "touchpad"};
    static const char *const axes[] = {"leftx", "lefty", "rightx", "righty", "lefttrigger:+", "righttrigger:+"};
    char map[1024], guid[64], name[128];
    SDL_Joystick *j = SDL_JoystickOpen(index);
    if (!j)
        return 0;
    int na = SDL_JoystickNumAxes(j), nb = SDL_JoystickNumButtons(j), nh = SDL_JoystickNumHats(j);
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(j), guid, sizeof guid);
    snprintf(name, sizeof name, "%s", SDL_JoystickName(j) ? SDL_JoystickName(j) : "joystick");
    SDL_JoystickClose(j);
    for (char *c = name; *c; c++)
        if (*c == ',')
            *c = ' ';
    if ((na < 2 && nh < 1) || nb < 1) {
        fprintf(stderr, "rush2049pc: joystick '%s' (%d axes, %d buttons, %d hats) is not used: no stick or no buttons\n",
                name, na, nb, nh);
        return 0;
    }
    size_t n = (size_t)snprintf(map, sizeof map, "%s,%s,", guid, name);
    for (int b = 0; b < nb && b < (int)(sizeof buttons / sizeof *buttons); b++)
        n += (size_t)snprintf(map + n, sizeof map - n, "%s:b%d,", buttons[b], b);
    for (int a = 0; a < na && a < (int)(sizeof axes / sizeof *axes); a++) {
        const char *colon = strchr(axes[a], ':');
        if (colon)   /* "lefttrigger:+" -> lefttrigger:+a4 */
            n += (size_t)snprintf(map + n, sizeof map - n, "%.*s:+a%d,", (int)(colon - axes[a]), axes[a], a);
        else
            n += (size_t)snprintf(map + n, sizeof map - n, "%s:a%d,", axes[a], a);
    }
    if (nh > 0)
        snprintf(map + n, sizeof map - n, "dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,");
    if (SDL_GameControllerAddMapping(map) < 0) {
        fprintf(stderr, "rush2049pc: joystick '%s': no generic mapping: %s\n", name, SDL_GetError());
        return 0;
    }
    fprintf(stderr,
            "rush2049pc: joystick '%s' (%s; %d axes, %d buttons, %d hats) has no SDL controller mapping: a generic "
            "one (axes 1-2 the stick, hat 1 the D-pad, buttons in order); gamecontrollerdb.txt next to rush2049pc.ini "
            "can give it its own\n", name, guid, na, nb, nh);
    return 1;
}

static const char *pad_type(SDL_GameController *gc)
{
#if SDL_VERSION_ATLEAST(2, 0, 12)
    switch (SDL_GameControllerGetType(gc)) {
    case SDL_CONTROLLER_TYPE_XBOX360: return "Xbox 360";
    case SDL_CONTROLLER_TYPE_XBOXONE: return "Xbox One";
    case SDL_CONTROLLER_TYPE_PS3: return "PS3";
    case SDL_CONTROLLER_TYPE_PS4: return "PS4";
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO: return "Switch Pro";
#if SDL_VERSION_ATLEAST(2, 0, 14)
    case SDL_CONTROLLER_TYPE_PS5: return "PS5";
    case SDL_CONTROLLER_TYPE_VIRTUAL: return "virtual";
#endif
    default: break;
    }
#endif
    return "other";
}

static int in_list(const SDL_JoystickID *list, int n, SDL_JoystickID id)
{
    for (int k = 0; k < n; k++)
        if (list[k] == id)
            return 1;
    return 0;
}

static void take_player(int p, SDL_GameController *gc, SDL_JoystickID id, int generic)
{
    char guid[64];
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(gc)), guid, sizeof guid);
#if SDL_VERSION_ATLEAST(2, 0, 18)
    int can_rumble = SDL_GameControllerHasRumble(gc) == SDL_TRUE;
#else
    int can_rumble = 1;   /* SDL can't say: rumble is tried */
#endif
    L.slot[p] = (Slot){.gc = gc, .id = id, .can_rumble = can_rumble};
    L.waiting_said = 0;
    SDL_GameControllerSetPlayerIndex(gc, p);
    fprintf(stderr, "rush2049pc: controller '%s' (%s, %s mapping, GUID %s) -> player %d%s%s\n",
            SDL_GameControllerName(gc), pad_type(gc), generic ? "generic" : "SDL", guid, p + 1,
            can_rumble ? "" : "; no rumble motor",
            L.analog == PC_CFG_STICK_NONE ? "; its sticks are not the N64 stick (controller.analog = none)" : "");
}

static int free_player(void)
{
    int p;
    for (p = L.first_port; p < 4 && L.slot[p].gc; p++)
        ;
    return p;
}

/* Each controller takes the first free player from first_controller_player up, in the order they are found; a
 * joystick without a mapping gets the generic one, and takes a player only at its first button press, since it
 * may be something other than a pad (a phantom device would otherwise hold player 1). Each is logged with its
 * name, kind, mapping, GUID and player. */
static void assign_pads(void)
{
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(i);
        int p, taken = 0, generic = in_list(L.generic, L.ngeneric, id);
        for (p = 0; p < 4; p++)
            taken |= L.slot[p].gc && L.slot[p].id == id;
        for (int k = 0; k < L.npending; k++)
            taken |= L.pending[k].id == id;
        if (taken || in_list(L.skipped, L.nskipped, id))
            continue;
        if (!SDL_IsGameController(i)) {
            int room = L.nskipped < (int)(sizeof L.skipped / sizeof *L.skipped) &&
                       L.ngeneric < (int)(sizeof L.generic / sizeof *L.generic);
            if (!room || !generic_mapping(i) || !SDL_IsGameController(i)) {
                if (L.nskipped < (int)(sizeof L.skipped / sizeof *L.skipped))
                    L.skipped[L.nskipped++] = id;   /* said once */
                continue;
            }
            L.generic[L.ngeneric++] = id;
            generic = 1;
        }
        if (generic && L.npending == (int)(sizeof L.pending / sizeof *L.pending))
            continue;
        p = free_player();
        if (p == 4 && !generic) {
            if (!L.waiting_said)
                fprintf(stderr, "rush2049pc: controller '%s' waits: players %d-4 each have one\n",
                        SDL_GameControllerNameForIndex(i), L.first_port + 1);
            L.waiting_said = 1;
            return;   /* every player has one: it waits until one is unplugged */
        }
        SDL_GameController *gc = SDL_GameControllerOpen(i);
        if (!gc) {
            fprintf(stderr, "rush2049pc: controller %d: %s\n", i, SDL_GetError());
            continue;
        }
        if (generic) {
            L.pending[L.npending++] = (Slot){.gc = gc, .id = id};
            fprintf(stderr, "rush2049pc: controller '%s' takes a player at its first button press\n",
                    SDL_GameControllerName(gc));
        } else {
            take_player(p, gc, id, 0);
        }
    }
}

/* A generic-mapped pad with a button down takes the first free player (before the pads are read, so that press
 * counts). */
static void claim_pending(void)
{
    for (int k = 0; k < L.npending; k++) {
        int down = 0, p = free_player();
        for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX && !down; b++)
            down = SDL_GameControllerGetButton(L.pending[k].gc, (SDL_GameControllerButton)b);
        if (!down || p == 4)
            continue;
        take_player(p, L.pending[k].gc, L.pending[k].id, 1);
        L.pending[k--] = L.pending[--L.npending];
    }
}

static void remove_pad(SDL_JoystickID id)
{
    for (int k = 0; k < L.npending; k++)
        if (L.pending[k].id == id) {
            SDL_GameControllerClose(L.pending[k].gc);
            L.pending[k--] = L.pending[--L.npending];
        }
    for (int p = 0; p < 4; p++)
        if (L.slot[p].gc && L.slot[p].id == id) {
            fprintf(stderr, "rush2049pc: controller '%s' left player %d\n", SDL_GameControllerName(L.slot[p].gc), p + 1);
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
    char title[160];
    if (!L.win)
        return;
    snprintf(title, sizeof title, "San Francisco Rush 2049%s%s%s",
             L.paused_focus || L.paused_key || pc_menu_is_open() ? " (paused)" : "",
             pc_input_title_tag ? " - " : "", pc_input_title_tag ? pc_input_title_tag : "");
    SDL_SetWindowTitle(L.win, title);
}

static void set_fullscreen(int on)
{
    pc_gfx_set_fullscreen(on);   /* port/gfx owns the window's modes (GFX.md "Window and display"); none headless */
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

/* ---------------------------------------------------------------- the mouse */

/* The cursor shows while the overlay is open, and during play only for a moment after the mouse moves: a cursor
 * resting over the race is in the way. */
static void show_cursor(int on)
{
    if (!L.win || on == L.cursor)
        return;
    SDL_ShowCursor(on ? SDL_ENABLE : SDL_DISABLE);
    L.cursor = on;
}

/* A mouse event while the overlay is open goes to it, in the drawable's pixels; the game never gets the mouse. */
static void menu_mouse(int what, int x, int y, int amount)
{
    int ww = 0, wh = 0, dw = 0, dh = 0, px, py;
    SDL_GetWindowSize(L.win, &ww, &wh);
    SDL_GL_GetDrawableSize(L.win, &dw, &dh);
    pc_menu_mouse_pixels(x, y, ww, wh, dw, dh, &px, &py);
    pc_menu_mouse(what, px, py, dw, dh, amount);
}

static void mouse_event(const SDL_Event *e)
{
    uint64_t now = pc_host_ns();
    switch (e->type) {
    case SDL_MOUSEMOTION:
        /* Not a move: SDL's first event for the mouse, which has no relative motion (Windows sends one when the
         * window opens or is raised under a cursor at rest), or one at the same place. */
        if ((!e->motion.xrel && !e->motion.yrel) ||
            (L.mouse_seen && e->motion.x == L.mouse_x && e->motion.y == L.mouse_y)) {
            L.mouse_x = e->motion.x, L.mouse_y = e->motion.y, L.mouse_seen = 1;
            break;
        }
        L.mouse_x = e->motion.x, L.mouse_y = e->motion.y, L.mouse_seen = 1;
        L.mouse_ns = now;
        show_cursor(1);
        if (pc_menu_is_open())
            menu_mouse(PC_MOUSE_MOVE, e->motion.x, e->motion.y, 0);
        break;
    case SDL_MOUSEBUTTONDOWN:
        L.mouse_x = e->button.x, L.mouse_y = e->button.y, L.mouse_seen = 1;
        if (!pc_menu_is_open() || (L.focus_ns && now >= L.focus_ns && now - L.focus_ns < FOCUS_CLICK_NS))
            break;
        if (e->button.button == SDL_BUTTON_LEFT) {
            menu_mouse(PC_MOUSE_MOVE, e->button.x, e->button.y, 0);   /* a click without a move first (touch) */
            menu_mouse(PC_MOUSE_LEFT, e->button.x, e->button.y, 0);
        } else if (e->button.button == SDL_BUTTON_RIGHT || e->button.button == SDL_BUTTON_X1)
            menu_mouse(PC_MOUSE_RIGHT, e->button.x, e->button.y, 0);
        break;
    case SDL_MOUSEWHEEL: {
        int y = e->wheel.y;
#if SDL_VERSION_ATLEAST(2, 0, 4)
        if (e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
            y = -y;
#endif
        if (pc_menu_is_open() && y)
            menu_mouse(PC_MOUSE_WHEEL, L.mouse_x, L.mouse_y, y);
        break;
    }
    }
}

/* ---------------------------------------------------------------- the About page's desktop */

int pc_input_live_desktop(void)
{
    const char *driver = SDL_GetCurrentVideoDriver();
    if (!L.on || !L.win)
        return 0;
    return driver && (!strcmp(driver, "offscreen") || !strcmp(driver, "dummy")) ? 1 : 2;
}

int pc_input_live_clipboard(const char *text)
{
    if (pc_input_live_desktop() < 1 || SDL_SetClipboardText(text) != 0)
        return -1;
    fprintf(stderr, "rush2049pc: the About page is on the clipboard\n");
    return 0;
}

int pc_input_live_open_folder(const char *path)
{
    if (pc_input_live_desktop() < 2 || !path || !path[0])
        return -1;
#if SDL_VERSION_ATLEAST(2, 0, 14)
    /* Windows: ShellExecute's "open"; elsewhere xdg-open (or open on macOS), started apart from this process */
    if (SDL_OpenURL(path) != 0) {
        fprintf(stderr, "rush2049pc: cannot open %s: %s\n", path, SDL_GetError());
        return -1;
    }
    fprintf(stderr, "rush2049pc: opening %s\n", path);
    return 0;
#else
    return -1;
#endif
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
    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEWHEEL:
        if (L.win)
            mouse_event(e);   /* the overlay's, or the cursor's: never the game's */
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
            L.focus_ns = pc_host_ns() ? pc_host_ns() : 1;   /* 0 is "not since start" */
            break;
        case SDL_WINDOWEVENT_EXPOSED:
        case SDL_WINDOWEVENT_SIZE_CHANGED:
            if (L.paused_focus || L.paused_key || pc_menu_is_open())
                pc_gfx_redraw();   /* the main loop is holding: show the last frame again */
            break;
        }
        break;
    case SDL_CONTROLLERDEVICEADDED:
    case SDL_JOYDEVICEADDED:   /* a joystick without a mapping gets no controller event */
        assign_pads();
        break;
    case SDL_CONTROLLERDEVICEREMOVED:
        remove_pad(e->cdevice.which);
        assign_pads();   /* a waiting controller takes the free player */
        break;
    case SDL_JOYDEVICEREMOVED:
        remove_pad(e->jdevice.which);
        assign_pads();
        break;
    }
    return 0;
}

/* ---------------------------------------------------------------- the scripts through SDL (--press-via) */

/* --press-via controller|joystick|keyboard[:SECONDS] (tests, docs/port/FRONTEND.md): the press scripts go the way
 * live input does instead of into the pads.
 * - controller: an SDL virtual GameController for each port the scripts use; joystick: a virtual joystick SDL
 *   has no mapping for, which gets the generic one. Both are plugged in at SECONDS on the N64 clock: 0 (the
 *   default) before input starts, so they are found at start; later, as a hot-plug. Each N64 control the script
 *   holds presses the first input bound to it; the script's stick tilts the controller.analog stick by the same
 *   share of its travel (80 is full tilt), or presses the STICK_* bindings with analog = none; MENU holds the
 *   parts of the first controller.menu. The joystick state goes in with SDL_JoystickUpdate before the pads are
 *   read, as a device's report would have.
 * - keyboard: key events for the first key bound to each control, for the keyboard's player, handled before the
 *   pads are read; MENU is the menu hotkey's first key. */
static const char *const generic_buttons[] = {"a", "b", "x", "y", "leftshoulder", "rightshoulder", "back", "start",
                                              "leftstick", "rightstick", "guide", "misc1", "paddle1", "paddle2",
                                              "paddle3", "paddle4", "touchpad"};
#define VIA_BUTTONS (int)(sizeof generic_buttons / sizeof *generic_buttons)

static struct {
    int kind;                      /* PC_VIA_* */
    double at;
    int nports, attached, warned;
    SDL_Joystick *joy[4];
    Uint16 rumble[4][2];
    uint8_t key[SDL_NUM_SCANCODES];   /* keys the script holds */
} V;

void pc_input_live_via_setup(int kind, double at, int nports)
{
    V.kind = kind;
    V.at = at;
    V.nports = nports < 1 ? 1 : nports > 4 ? 4 : nports;
}

#if SDL_VERSION_ATLEAST(2, 24, 0)
static int SDLCALL via_rumble(void *user, Uint16 low, Uint16 high)
{
    int i = (int)(intptr_t)user;
    if (V.rumble[i][0] != low || V.rumble[i][1] != high)
        fprintf(stderr, "rush2049pc: virtual controller %d rumble %u %u\n", i + 1, low, high);
    V.rumble[i][0] = low, V.rumble[i][1] = high;
    return 0;
}
#endif

static void via_attach(void)
{
#if SDL_VERSION_ATLEAST(2, 24, 0)
    static char names[4][48];   /* SDL keeps the pointer */
    for (int i = V.attached; i < V.nports; i++) {
        char *name = names[i];
        SDL_VirtualJoystickDesc d;
        SDL_zero(d);
        snprintf(name, sizeof names[i], "rush2049pc virtual %s %d",
                 V.kind == PC_VIA_JOYSTICK ? "joystick" : "controller", i + 1);
        d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
        d.type = V.kind == PC_VIA_JOYSTICK ? SDL_JOYSTICK_TYPE_UNKNOWN : SDL_JOYSTICK_TYPE_GAMECONTROLLER;
        d.naxes = SDL_CONTROLLER_AXIS_MAX;
        d.nbuttons = V.kind == PC_VIA_JOYSTICK ? VIA_BUTTONS : SDL_CONTROLLER_BUTTON_MAX;
        d.nhats = V.kind == PC_VIA_JOYSTICK;
        d.name = name;
        d.userdata = (void *)(intptr_t)i;
        d.Rumble = via_rumble;
        int index = SDL_JoystickAttachVirtualEx(&d);
        if (index < 0 || !(V.joy[i] = SDL_JoystickOpen(index))) {
            fprintf(stderr, "rush2049pc: --press-via: no virtual %s: %s\n", name, SDL_GetError());
            break;
        }
        for (int a = SDL_CONTROLLER_AXIS_TRIGGERLEFT; a < SDL_CONTROLLER_AXIS_MAX && V.kind == PC_VIA_CONTROLLER; a++)
            SDL_JoystickSetVirtualAxis(V.joy[i], a, SDL_JOYSTICK_AXIS_MIN);   /* a released trigger */
        V.attached = i + 1;
    }
    SDL_JoystickUpdate();
#else
    fprintf(stderr, "rush2049pc: --press-via needs SDL 2.24 or later for virtual joysticks\n");
    V.attached = V.nports;
#endif
}

/* One input pressed: a button, an axis at full travel its way, or a trigger pulled. */
static void via_press(const PadSource *s, Uint8 *buttons, Sint16 *axes, Uint8 *hat)
{
    if (s->axis) {
        axes[s->id] = s->id >= SDL_CONTROLLER_AXIS_TRIGGERLEFT || s->sign > 0 ? 32767 : -32767;
        return;
    }
    if (V.kind == PC_VIA_CONTROLLER) {
        buttons[s->id] = 1;
        return;
    }
    switch (s->id) {   /* the generic mapping: the D-pad is hat 1, the buttons in generic_buttons' order */
    case SDL_CONTROLLER_BUTTON_DPAD_UP: *hat |= SDL_HAT_UP; return;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: *hat |= SDL_HAT_RIGHT; return;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: *hat |= SDL_HAT_DOWN; return;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: *hat |= SDL_HAT_LEFT; return;
    }
    for (int b = 0; b < VIA_BUTTONS; b++)
        if (!strcmp(generic_buttons[b], SDL_GameControllerGetStringForButton((SDL_GameControllerButton)s->id)))
            buttons[b] = 1;
}

static Sint16 via_tilt(int v)
{
    long n = lround(v * 32767.0 / 80);
    return (Sint16)(n > 32767 ? 32767 : n < -32767 ? -32767 : n);
}

static void via_pad(int i, const PcPad *p, int menu)
{
#if SDL_VERSION_ATLEAST(2, 24, 0)
    Uint8 buttons[SDL_CONTROLLER_BUTTON_MAX > VIA_BUTTONS ? SDL_CONTROLLER_BUTTON_MAX : VIA_BUTTONS] = {0}, hat = 0;
    Sint16 axes[SDL_CONTROLLER_AXIS_MAX] = {0};
    int nb = V.kind == PC_VIA_JOYSTICK ? VIA_BUTTONS : SDL_CONTROLLER_BUTTON_MAX;
    if (V.kind == PC_VIA_CONTROLLER)
        axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT] = axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] = SDL_JOYSTICK_AXIS_MIN;
    if (!V.joy[i])
        return;
    if (L.analog != PC_CFG_STICK_NONE) {
        int ox = L.analog == PC_CFG_STICK_LEFT ? SDL_CONTROLLER_AXIS_LEFTX : SDL_CONTROLLER_AXIS_RIGHTX;
        axes[ox] = via_tilt(p->stick_x);
        axes[ox + 1] = via_tilt(-p->stick_y);   /* SDL's +y is down */
    } else {
        int dir[4] = {p->stick_y > 0, p->stick_y < 0, p->stick_x < 0, p->stick_x > 0};   /* STICK_UP... */
        for (int d = 0; d < 4; d++)
            if (dir[d] && L.npads[PC_CTL_STICK_UP + d])
                via_press(&L.pads[PC_CTL_STICK_UP + d][0], buttons, axes, &hat);
    }
    for (int c = 0; c < PC_NUM_CONTROLS; c++)
        if ((p->buttons & pc_control_masks[c]) && pc_control_masks[c] && L.npads[c])
            via_press(&L.pads[c][0], buttons, axes, &hat);
    for (int k = 0; menu && L.nmenu_pad && k < L.menu_pad[0].n; k++)
        via_press(&L.menu_pad[0].part[k], buttons, axes, &hat);
    for (int b = 0; b < nb; b++)
        SDL_JoystickSetVirtualButton(V.joy[i], b, buttons[b]);
    for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; a++)
        SDL_JoystickSetVirtualAxis(V.joy[i], a, axes[a]);
    if (V.kind == PC_VIA_JOYSTICK)
        SDL_JoystickSetVirtualHat(V.joy[i], 0, hat);
#endif
}

static void via_key(SDL_Scancode sc, int down)
{
    if (V.key[sc] == down)
        return;
    SDL_Event e;
    SDL_zero(e);
    e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    e.key.windowID = L.win ? SDL_GetWindowID(L.win) : 0;
    e.key.keysym.scancode = sc;
    e.key.keysym.sym = SDL_GetKeyFromScancode(sc);
    SDL_PushEvent(&e);
    V.key[sc] = (uint8_t)down;
}

static void via_keys(const PcPad pads[4], int menu)
{
    uint8_t want[SDL_NUM_SCANCODES] = {0};
    for (int port = 0; port < 4; port++) {
        const PcPad *p = &pads[port];
        if (port == L.kb_port || (!p->buttons && !p->stick_x && !p->stick_y) || V.warned)
            continue;
        fprintf(stderr, "rush2049pc: --press-via keyboard: player %d is not the keyboard's; its presses are not sent\n",
                port + 1);
        V.warned = 1;
    }
    if (L.kb_port >= 0 && L.kb_port < 4) {
        const PcPad *p = &pads[L.kb_port];
        int dir[4] = {p->stick_y > 0, p->stick_y < 0, p->stick_x < 0, p->stick_x > 0};
        for (int c = 0; c < PC_NUM_CONTROLS; c++) {
            int on = c >= PC_CTL_STICK_UP ? dir[c - PC_CTL_STICK_UP] : (p->buttons & pc_control_masks[c]) != 0;
            if (on && L.nkeys[c])
                want[L.keys[c][0]] = 1;
        }
    }
    if (menu && L.nhot[PC_HOTKEY_MENU])
        want[L.hot[PC_HOTKEY_MENU][0]] = 1;
    for (int sc = 0; sc < SDL_NUM_SCANCODES; sc++)
        via_key((SDL_Scancode)sc, want[sc]);
    SDL_Event e;   /* handled now, as port/gfx's event loop would at the next retrace */
    while (SDL_PeepEvents(&e, 1, SDL_GETEVENT, SDL_KEYDOWN, SDL_KEYUP) > 0)
        pc_input_live_event(&e);
}

int pc_input_live_via(const PcPad script[4], int menu, double now)
{
    if (!L.on || V.kind == PC_VIA_PADS)
        return -1;
    if (V.kind == PC_VIA_KEYBOARD) {
        via_keys(script, menu);
        return 0;
    }
    if (V.attached < V.nports && now >= V.at)
        via_attach();
    for (int i = 0; i < V.attached; i++)
        via_pad(i, &script[i], menu && i == 0);
    SDL_JoystickUpdate();
    return 0;
}

static void via_detach(void)
{
#if SDL_VERSION_ATLEAST(2, 24, 0)
    for (int i = 0; i < V.attached; i++)
        if (V.joy[i]) {
            SDL_JoystickID id = SDL_JoystickInstanceID(V.joy[i]);
            SDL_JoystickClose(V.joy[i]);
            for (int k = 0; k < SDL_NumJoysticks(); k++)
                if (SDL_JoystickGetDeviceInstanceID(k) == id)
                    SDL_JoystickDetachVirtual(k);
            V.joy[i] = NULL;
        }
#endif
    V.attached = 0;
}

int pc_input_live_init(const PcSettings *s)
{
    char path[PC_CFG_PATH + 32];
    struct stat st;
    memset(&L, 0, sizeof L);
    L.nav_dir = L.stick_dir[0] = L.stick_dir[1] = -1;
    if (SDL_WasInit(SDL_INIT_VIDEO) && pc_gfx_set_event_hook(pc_input_live_event) == 0)
        L.win = SDL_GL_GetCurrentWindow();
    if (!L.win) {
        /* No window: scripted input only, unless --press-via asks for SDL's controllers and events alone
         * (tests, --headless): pc_input_live_tick pumps their events then, as port/gfx's loop would. */
        pc_gfx_set_event_hook(NULL);
        if (V.kind == PC_VIA_PADS || SDL_InitSubSystem(SDL_INIT_EVENTS) != 0)
            return -1;
    }
    L.kb_port = s->keyboard_player - 1;
    L.first_port = s->first_controller_player - 1;
    configure(s);

    /* As v5: controllers keep working while the window is in the background, and Nintendo-style pads keep
     * their buttons' positions (south is a), not their labels. */
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
#ifdef _WIN32
    /* Xbox-style pads through XInput, which is polled: SDL's raw input driver takes their sticks and buttons from
     * window messages to a hidden window, and only the triggers and guide from XInput, so a pad whose raw input
     * doesn't arrive has working triggers and dead sticks. SDL_JOYSTICK_RAWINPUT=1 in the environment brings raw
     * input back (an environment variable wins over this). Other pads are unchanged: HIDAPI (PlayStation,
     * Switch) or DirectInput. */
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
#endif
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
    if (L.win) {
        set_title();
        SDL_SetWindowMinimumSize(L.win, 320, 240);
        SDL_RaiseWindow(L.win);   /* the keys go to it, not the console it was started from */
        if (s->fullscreen)
            set_fullscreen(1);
        pc_gfx_set_vsync(s->vsync);
        L.cursor = -1;
        show_cursor(0);   /* no cursor over the game until the mouse moves */
    }
    if (V.kind != PC_VIA_PADS && V.kind != PC_VIA_KEYBOARD && V.at <= 0)
        via_attach();   /* there at start */
    assign_pads();
    int n = L.npending;
    for (int p = 0; p < 4; p++)
        n += L.slot[p].gc != NULL;
    if (!n)
        fprintf(stderr, "rush2049pc: no controllers yet (%d joysticks); one plugged in later is found then\n",
                SDL_WasInit(SDL_INIT_JOYSTICK) ? SDL_NumJoysticks() : 0);
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
    for (int k = 0; k < L.npending; k++)
        SDL_GameControllerClose(L.pending[k].gc);
    show_cursor(1);
    via_detach();
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    if (!L.win)
        SDL_QuitSubSystem(SDL_INIT_EVENTS);
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
    L.mouse_ns = pc_host_ns();   /* closed, the cursor stays a moment, then hides (pc_input_live_tick) */
    if (open) {
        memset(L.down, 0, sizeof L.down);   /* the game's keys are released while it is open */
        show_cursor(1);
    } else {
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
    SDL_Event e;
    while (L.on && !L.win && SDL_PollEvent(&e))   /* no window: port/gfx pumps no SDL events */
        pc_input_live_event(&e);
    if (L.on && L.win) {   /* the cursor: shown while the overlay is open, else for a moment after a move */
        uint64_t now = pc_host_ns();
        if (now < L.mouse_ns)   /* the N64 clock was set back after a hold */
            L.mouse_ns = now;
        if (pc_menu_is_open())
            show_cursor(1);
        else if (L.cursor && now - L.mouse_ns >= CURSOR_NS)
            show_cursor(0);
    }
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
    return L.on && L.win ? (SDL_GetWindowFlags(L.win) & SDL_WINDOW_FULLSCREEN) != 0 : -1;
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
    claim_pending();
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
        digital.gain = 1;
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
        pads[p].rumble = gc && slot->can_rumble;
    }
}

/* The overlay's view of a player: who plays it ("keyboard", a controller's name, both) and, with a controller, its
 * N64 stick as the game would get it now. 0 when nobody does. */
int pc_input_live_describe(int port, char *who, size_t n, int8_t *x, int8_t *y)
{
    *x = *y = 0;
    who[0] = 0;
    if (!L.on || port < 0 || port > 3)
        return 0;
    SDL_GameController *gc = L.slot[port].gc;
    snprintf(who, n, "%s%s%s", port == L.kb_port ? "keyboard" : "", port == L.kb_port && gc ? " + " : "",
             gc ? SDL_GameControllerName(gc) : "");
    if (gc && L.analog != PC_CFG_STICK_NONE) {
        int ox = L.analog == PC_CFG_STICK_LEFT ? SDL_CONTROLLER_AXIS_LEFTX : SDL_CONTROLLER_AXIS_RIGHTX;
        pc_stick_shape(axis_value(gc, ox), -axis_value(gc, ox + 1), &L.shape, x, y);
    }
    return gc ? 2 : who[0] != 0;
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
int pc_input_live_describe(int port, char *who, size_t n, int8_t *x, int8_t *y)
{
    (void)port, (void)n;
    *x = *y = 0;
    who[0] = 0;
    return 0;
}
void pc_input_live_via_setup(int kind, double at, int nports) { (void)kind, (void)at, (void)nports; }
int pc_input_live_via(const PcPad script[4], int menu, double now)
{
    (void)script, (void)menu, (void)now;
    return -1;
}
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
int pc_input_live_desktop(void) { return 0; }
int pc_input_live_clipboard(const char *text) { (void)text; return -1; }
int pc_input_live_open_folder(const char *path) { (void)path; return -1; }

#endif
