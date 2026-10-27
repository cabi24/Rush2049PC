/* port/config/config.c: rush2049pc.ini (docs/port/FRONTEND.md): a small INI reader, the settings table, the
 * file's location, the first-run file, and the layers (defaults, file, environment, command line). */
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"

#define FILE_NAME "rush2049pc.ini"

/* ---------------------------------------------------------------- the INI reader */

static char *trim(char *p)
{
    while (*p == ' ' || *p == '\t')
        p++;
    char *e = p + strlen(p);
    while (e > p && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'))
        *--e = 0;
    return p;
}

static void lower(char *p)
{
    for (; *p; p++)
        *p = (char)tolower((unsigned char)*p);
}

int pc_ini_parse(const char *text, PcIniHandler handler, void (*bad)(void *user, int line, const char *why),
                 void *user)
{
    char section[64] = "";
    int nbad = 0, line = 0;
    if ((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
        text += 3;
    while (*text) {
        const char *end = strchr(text, '\n');
        size_t n = end ? (size_t)(end - text) : strlen(text);
        char buf[2048];
        line++;
        if (n >= sizeof buf) {
            nbad++;
            if (bad)
                bad(user, line, "line too long");
            text += n + (end != NULL);
            continue;
        }
        memcpy(buf, text, n);
        buf[n] = 0;
        text += n + (end != NULL);
        char *p = trim(buf);
        if (!*p || *p == ';' || *p == '#')
            continue;
        if (*p == '[') {
            char *close = strchr(p, ']');
            char *rest = close ? trim(close + 1) : NULL;
            if (!close || (*rest && *rest != ';' && *rest != '#')) {
                nbad++;
                if (bad)
                    bad(user, line, "malformed section header");
                continue;
            }
            *close = 0;
            snprintf(section, sizeof section, "%s", trim(p + 1));
            lower(section);
            continue;
        }
        char *eq = strchr(p, '=');
        if (!eq) {
            nbad++;
            if (bad)
                bad(user, line, "expected key = value");
            continue;
        }
        *eq = 0;
        char *key = trim(p), *value = trim(eq + 1);
        if (!*key) {
            nbad++;
            if (bad)
                bad(user, line, "missing key");
            continue;
        }
        lower(key);
        if (*value == '"') {
            char *q = strchr(value + 1, '"');
            char *rest = q ? trim(q + 1) : NULL;
            if (!q || (*rest && *rest != ';' && *rest != '#')) {
                nbad++;
                if (bad)
                    bad(user, line, "unterminated quote");
                continue;
            }
            *q = 0;
            value++;
        } else {
            for (char *c = value; *c; c++)   /* an inline comment needs whitespace before it */
                if ((*c == ';' || *c == '#') && c > value && (c[-1] == ' ' || c[-1] == '\t')) {
                    *c = 0;
                    value = trim(value);
                    break;
                }
        }
        if (handler(user, section, key, value, line) != 0)
            break;
    }
    return nbad;
}

/* ---------------------------------------------------------------- controls and hotkeys */

const char *const pc_control_names[PC_NUM_CONTROLS] = {
    "A", "B", "Z", "START", "DUP", "DDOWN", "DLEFT", "DRIGHT", "L", "R", "CUP", "CDOWN", "CLEFT", "CRIGHT",
    "STICK_UP", "STICK_DOWN", "STICK_LEFT", "STICK_RIGHT"};
const uint16_t pc_control_masks[PC_NUM_CONTROLS] = {
    0x8000, 0x4000, 0x2000, 0x1000, 0x0800, 0x0400, 0x0200, 0x0100, 0x0020, 0x0010, 0x0008, 0x0004, 0x0002,
    0x0001, 0, 0, 0, 0};
const char *const pc_hotkey_names[PC_NUM_HOTKEYS] = {"fullscreen", "screenshot", "pause", "quit"};

int pc_control_find(const char *name)
{
    for (int i = 0; i < PC_NUM_CONTROLS; i++)
        if (!strcasecmp(name, pc_control_names[i]))
            return i;
    return -1;
}

/* Keyboard: v5's keys (Rush2049Recompiled sdl_frontend.cpp). Names are SDL's key names, plus the aliases in
 * port/input for punctuation (Comma, Semicolon...), which a comma-separated list or a comment can't hold. */
static const char *const default_keys[PC_NUM_CONTROLS] = {
    "X", "C", "Z", "Return", "W", "S", "A", "D", "Q", "E", "I", "K", "J", "L", "Up", "Down", "Left", "Right"};
/* Controller (SDL GameController names; an XInput layout): v5's face buttons and sticks, with gas and brake
 * also on the triggers. The game's A and B are accelerate and brake. */
static const char *const default_pads[PC_NUM_CONTROLS] = {
    "a, righttrigger", "x, lefttrigger", "leftshoulder", "start", "dpup", "dpdown", "dpleft", "dpright",
    "back", "rightshoulder", "y, -righty", "b, +righty", "-rightx", "+rightx", "", "", "", ""};
static const char *const default_hotkeys[PC_NUM_HOTKEYS] = {"F11", "F12", "Pause", "Escape"};

/* ---------------------------------------------------------------- the settings table */

typedef enum { T_STR, T_BOOL, T_INT, T_REAL, T_ENUM, T_ASPECT } ItemType;

typedef struct {
    const char *section, *key;
    ItemType type;
    size_t offset, size;
    double min, max;
    const char *const *choices;   /* T_ENUM: value i is choices[i]; NULL-terminated */
    const char *def;              /* the default, as the file writes it */
    const char *comment;
} Item;

static const char *const pak_choices[] = {"none", "memory", "rumble", NULL};
static const char *const api_choices[] = {"gles", "gl", NULL};
static const char *const analog_choices[] = {"left", "right", "none", NULL};

#define F(field) offsetof(PcSettings, field), sizeof(((PcSettings *)0)->field)
static const Item items[] = {
    {"game", "rom", T_STR, F(rom), 0, 0, NULL, "baserom.us.z64",
     "The San Francisco Rush 2049 (USA) ROM: .z64, .v64 or .n64. A relative path is from the working directory."},
    {"game", "rom_check", T_BOOL, F(rom_check), 0, 0, NULL, "on", "Refuse a ROM whose SHA-1 is not the USA ROM's."},
    {"game", "expansion_pak", T_BOOL, F(expansion_pak), 0, 0, NULL, "off",
     "Report 8 MiB of RDRAM (the Expansion Pak). v5 parity runs need it."},
    {"game", "pak", T_ENUM, F(pak), 0, 0, pak_choices, "memory",
     "Controller 1's accessory: memory (the controller pak, for saves), rumble or none."},
    {"game", "pak_file", T_STR, F(pak_file), 0, 0, NULL, "rush2049pc.mpk", "The controller pak image."},

    {"video", "width", T_INT, F(width), 160, 16384, NULL, "640", "Window size. The game draws 320 x 220."},
    {"video", "height", T_INT, F(height), 120, 16384, NULL, "480", NULL},
    {"video", "fullscreen", T_BOOL, F(fullscreen), 0, 0, NULL, "off", "Start in fullscreen (Alt+Enter or F11 toggles)."},
    {"video", "vsync", T_BOOL, F(vsync), 0, 0, NULL, "on", NULL},
    {"video", "scale", T_INT, F(scale), 1, 8, NULL, "1",
     "Internal resolution, a multiple of the N64's (1-8; RUSH_GFX_SCALE)."},
    {"video", "api", T_ENUM, F(api), 0, 0, api_choices, "gles", "gles (OpenGL ES 3.0) or gl (desktop GL; RUSH_GFX_API)."},
    {"video", "widescreen", T_ASPECT, F(widescreen), 4.0 / 3.0, 4, NULL, "off",
     "Widen the picture: off (4:3), auto (the window's aspect), 16:9, 16:10, 21:9, W:H or a ratio up to 4. The 3D view "
     "shows more at the sides; menus and the HUD stay 4:3 (RUSH_GFX_WIDESCREEN)."},
    {"video", "pause_on_focus_loss", T_BOOL, F(pause_on_focus_loss), 0, 0, NULL, "off",
     "Hold the game while the window is in the background."},
    {"video", "screenshot_dir", T_STR, F(screenshot_dir), 0, 0, NULL, "",
     "Where F12 saves screenshots; empty for the working directory."},

    {"audio", "backend", T_STR, F(audio), 0, 0, NULL, "auto",
     "auto (SDL2, then ALSA, then none), sdl, alsa, null or wav (RUSH2049_AUDIO)."},

    {"input", "keyboard_player", T_INT, F(keyboard_player), 0, 4, NULL, "1", "The keyboard's player, 1-4; 0 for none."},
    {"input", "first_controller_player", T_INT, F(first_controller_player), 1, 4, NULL, "1",
     "Controllers take players from this one up, in the order they are plugged in. 2 leaves player 1 to the keyboard."},
    {"input", "deadzone", T_REAL, F(deadzone), 0, 0.9, NULL, "0.15",
     "Stick and trigger dead zone, 0-0.9 of the travel; the rest is rescaled from 0."},
    {"input", "axis_threshold", T_REAL, F(axis_threshold), 0.05, 1, NULL, "0.5",
     "A trigger or stick direction bound to a button presses it past this."},
    {"input", "stick_range", T_INT, F(stick_range), 1, 127, NULL, "80",
     "The N64 stick value at full tilt (an N64 pad reads about 80)."},
    {"input", "stick_diagonal", T_INT, F(stick_diagonal), 0, 127, NULL, "70",
     "Each axis at full diagonal tilt: the N64's octagonal gate. 0 for a round gate."},
    {"input", "rumble", T_REAL, F(rumble), 0, 1, NULL, "1.0",
     "Rumble strength, 0-1, when the rumble pak is in (game.pak = rumble)."},
};
#undef F
#define NUM_ITEMS (sizeof items / sizeof *items)

static const Item *find_item(const char *section, const char *key)
{
    for (size_t i = 0; i < NUM_ITEMS; i++)
        if (!strcasecmp(items[i].section, section) && !strcasecmp(items[i].key, key))
            return &items[i];
    return NULL;
}

static int parse_bool(const char *v, int *out)
{
    static const char *const yes[] = {"on", "true", "yes", "1"}, *const no[] = {"off", "false", "no", "0"};
    for (int i = 0; i < 4; i++) {
        if (!strcasecmp(v, yes[i]))
            return *out = 1, 0;
        if (!strcasecmp(v, no[i]))
            return *out = 0, 0;
    }
    return -1;
}

/* An aspect: off (0), auto (-1), W:H or a ratio, from min to max; 4:3 itself is off. */
static int parse_aspect(const char *v, double min, double max, double *out)
{
    double a, b;
    char tail;
    if (!strcasecmp(v, "off") || !strcmp(v, "0"))
        return *out = 0, 0;
    if (!strcasecmp(v, "auto"))
        return *out = -1, 0;
    if (sscanf(v, "%lf:%lf%c", &a, &b, &tail) == 2 && b > 0)
        a /= b;
    else if (sscanf(v, "%lf%c", &a, &tail) != 1)
        return -1;
    if (!(a >= min - 1e-4 && a <= max + 1e-4))
        return -1;
    *out = a <= min + 1e-4 ? 0 : a;
    return 0;
}

static int set_item(PcSettings *s, const Item *it, const char *v, char *why, size_t why_size)
{
    char *base = (char *)s + it->offset, *end;
    switch (it->type) {
    case T_STR:
        if (strlen(v) >= it->size) {
            snprintf(why, why_size, "longer than %zu characters", it->size - 1);
            return -2;
        }
        memcpy(base, v, strlen(v) + 1);
        return 0;
    case T_BOOL: {
        int b;
        if (parse_bool(v, &b) != 0) {
            snprintf(why, why_size, "'%s' is not on or off", v);
            return -2;
        }
        *(int *)base = b;
        return 0;
    }
    case T_INT: {
        errno = 0;
        long n = strtol(v, &end, 10);
        if (!*v || *end || errno || n < it->min || n > it->max) {
            snprintf(why, why_size, "'%s' is not a whole number from %g to %g", v, it->min, it->max);
            return -2;
        }
        *(int *)base = (int)n;
        return 0;
    }
    case T_REAL: {
        double d = strtod(v, &end);
        if (!*v || *end || !(d >= it->min && d <= it->max)) {
            snprintf(why, why_size, "'%s' is not a number from %g to %g", v, it->min, it->max);
            return -2;
        }
        *(double *)base = d;
        return 0;
    }
    case T_ASPECT: {
        double a;
        if (parse_aspect(v, it->min, it->max, &a) != 0) {
            snprintf(why, why_size, "'%s' is not off, auto or an aspect (16:9, 2.39) from 4:3 to %g", v, it->max);
            return -2;
        }
        *(double *)base = a;
        return 0;
    }
    case T_ENUM:
        for (int i = 0; it->choices[i]; i++)
            if (!strcasecmp(v, it->choices[i])) {
                *(int *)base = i;
                return 0;
            }
        snprintf(why, why_size, "'%s' is not one of", v);
        for (int i = 0; it->choices[i]; i++) {
            size_t n = strlen(why);
            snprintf(why + n, why_size - n, "%s %s", i ? "," : "", it->choices[i]);
        }
        return -2;
    }
    return -2;
}

static void format_item(const PcSettings *s, const Item *it, char *out, size_t n)
{
    const char *base = (const char *)s + it->offset;
    switch (it->type) {
    case T_STR:
        snprintf(out, n, "%s", base);
        break;
    case T_BOOL:
        snprintf(out, n, "%s", *(const int *)base ? "on" : "off");
        break;
    case T_INT:
        snprintf(out, n, "%d", *(const int *)base);
        break;
    case T_REAL:
        snprintf(out, n, "%g", *(const double *)base);
        if (!strpbrk(out, ".e"))
            snprintf(out + strlen(out), n - strlen(out), ".0");
        break;
    case T_ENUM:
        snprintf(out, n, "%s", it->choices[*(const int *)base]);
        break;
    case T_ASPECT: {
        double a = *(const double *)base;
        if (a == 0)
            snprintf(out, n, "off");
        else if (a < 0)
            snprintf(out, n, "auto");
        else {
            /* the usual names first, then W:H when a small one gives it back */
            static const int named[][2] = {{16, 9}, {16, 10}, {21, 9}, {32, 9}};
            snprintf(out, n, "%.4g", a);
            for (size_t i = 0; i < sizeof named / sizeof *named; i++)
                if (a * named[i][1] - named[i][0] < 1e-4 && a * named[i][1] - named[i][0] > -1e-4) {
                    snprintf(out, n, "%d:%d", named[i][0], named[i][1]);
                    return;
                }
            for (int h = 1; h <= 32; h++) {
                int w = (int)(a * h + 0.5);
                double d = a * h - w;
                if (d < 1e-4 * h && d > -1e-4 * h) {
                    snprintf(out, n, "%d:%d", w, h);
                    break;
                }
            }
        }
        break;
    }
    }
}

static int set_binding(char (*table)[PC_CFG_BIND], int index, const char *v, char *why, size_t why_size)
{
    if (strlen(v) >= PC_CFG_BIND) {
        snprintf(why, why_size, "longer than %d characters", PC_CFG_BIND - 1);
        return -2;
    }
    snprintf(table[index], PC_CFG_BIND, "%s", v);
    return 0;
}

void pc_settings_defaults(PcSettings *s)
{
    char why[128];
    memset(s, 0, sizeof *s);
    for (size_t i = 0; i < NUM_ITEMS; i++)
        if (set_item(s, &items[i], items[i].def, why, sizeof why) != 0)
            abort();   /* a bad default in the table */
    for (int i = 0; i < PC_NUM_CONTROLS; i++) {
        snprintf(s->key[i], PC_CFG_BIND, "%s", default_keys[i]);
        snprintf(s->pad[i], PC_CFG_BIND, "%s", default_pads[i]);
    }
    s->analog = PC_CFG_STICK_LEFT;
    for (int i = 0; i < PC_NUM_HOTKEYS; i++)
        snprintf(s->hotkey[i], PC_CFG_BIND, "%s", default_hotkeys[i]);
}

int pc_settings_set(PcSettings *s, const char *section, const char *key, const char *value, char *why,
                    size_t why_size)
{
    char scratch[160];
    if (!why) {
        why = scratch;
        why_size = sizeof scratch;
    }
    why[0] = 0;
    if (!strcasecmp(section, "keyboard") || !strcasecmp(section, "controller")) {
        int c = pc_control_find(key);
        if (!strcasecmp(section, "controller") && !strcasecmp(key, "analog")) {
            for (int i = 0; analog_choices[i]; i++)
                if (!strcasecmp(value, analog_choices[i]))
                    return s->analog = i, 0;
            snprintf(why, why_size, "'%s' is not one of left, right, none", value);
            return -2;
        }
        if (c < 0) {
            snprintf(why, why_size, "no N64 control '%s'", key);
            return -1;
        }
        return set_binding(!strcasecmp(section, "keyboard") ? s->key : s->pad, c, value, why, why_size);
    }
    if (!strcasecmp(section, "hotkeys")) {
        for (int i = 0; i < PC_NUM_HOTKEYS; i++)
            if (!strcasecmp(key, pc_hotkey_names[i]))
                return set_binding(s->hotkey, i, value, why, why_size);
        snprintf(why, why_size, "no hotkey '%s'", key);
        return -1;
    }
    const Item *it = find_item(section, key);
    if (!it) {
        snprintf(why, why_size, "no setting %s.%s", section, key);
        return -1;
    }
    return set_item(s, it, value, why, why_size);
}

int pc_settings_get(const PcSettings *s, const char *section, const char *key, char *out, size_t out_size)
{
    const Item *it = find_item(section, key);
    if (!it)
        return -1;
    format_item(s, it, out, out_size);
    return 0;
}

int pc_settings_assign(PcSettings *s, const char *assignment, char *why, size_t why_size)
{
    char buf[PC_CFG_PATH + 128];
    snprintf(buf, sizeof buf, "%s", assignment);
    char *eq = strchr(buf, '='), *dot = strchr(buf, '.');
    if (!eq || !dot || dot > eq) {
        if (why)
            snprintf(why, why_size, "'%s' is not section.key=value", assignment);
        return -3;
    }
    *eq = 0;
    *dot = 0;
    return pc_settings_set(s, trim(buf), trim(dot + 1), trim(eq + 1), why, why_size);
}

/* ---------------------------------------------------------------- reading */

typedef struct {
    PcSettings *s;
    const char *name;
    int problems;
} LoadCtx;

static int on_entry(void *user, const char *section, const char *key, const char *value, int line)
{
    LoadCtx *c = user;
    char why[200];
    if (pc_settings_set(c->s, section, key, value, why, sizeof why) != 0) {
        fprintf(stderr, "rush2049pc: %s:%d: %s; ignored\n", c->name, line, why);
        c->problems++;
    }
    return 0;
}

static void on_bad(void *user, int line, const char *why)
{
    LoadCtx *c = user;
    fprintf(stderr, "rush2049pc: %s:%d: %s; ignored\n", c->name, line, why);
}

int pc_settings_load_text(PcSettings *s, const char *text, const char *name)
{
    LoadCtx c = {s, name ? name : FILE_NAME, 0};
    c.problems += pc_ini_parse(text, on_entry, on_bad, &c);
    return c.problems;
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    size_t cap = 4096, n = 0;
    char *text = malloc(cap);
    for (;;) {
        if (n + 1024 > cap)
            text = realloc(text, cap *= 2);
        size_t got = fread(text + n, 1, cap - n - 1, f);
        n += got;
        if (got == 0)
            break;
    }
    fclose(f);
    text[n] = 0;
    return text;
}

static void dir_of(const char *path, char *out, size_t n)
{
    const char *slash = strrchr(path, '/');
    if (!slash)
        snprintf(out, n, ".");
    else if (slash == path)
        snprintf(out, n, "/");
    else
        snprintf(out, n, "%.*s", (int)(slash - path), path);
}

int pc_settings_load_file(PcSettings *s, const char *path)
{
    char *text = read_file(path);
    if (!text)
        return -1;
    int r = pc_settings_load_text(s, text, path);
    free(text);
    snprintf(s->source, sizeof s->source, "%s", path);
    dir_of(path, s->source_dir, sizeof s->source_dir);
    return r;
}

int pc_settings_apply_env(PcSettings *s)
{
    static const struct { const char *var, *section, *key; } env[] = {
        {"RUSH2049_AUDIO", "audio", "backend"}, {"RUSH_GFX_SCALE", "video", "scale"}, {"RUSH_GFX_API", "video", "api"},
        {"RUSH_GFX_WIDESCREEN", "video", "widescreen"}};
    int bad = 0;
    for (size_t i = 0; i < sizeof env / sizeof *env; i++) {
        const char *v = getenv(env[i].var);
        char why[160];
        if (!v || !*v)
            continue;
        if (pc_settings_set(s, env[i].section, env[i].key, v, why, sizeof why) != 0) {
            fprintf(stderr, "rush2049pc: %s: %s; ignored\n", env[i].var, why);
            bad++;
        }
    }
    return bad;
}

/* ---------------------------------------------------------------- writing */

typedef struct {
    char *p;
    size_t n, cap;
} Buf;

static void put(Buf *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void put(Buf *b, const char *fmt, ...)
{
    va_list ap;
    for (;;) {
        va_start(ap, fmt);
        int k = vsnprintf(b->p + b->n, b->cap - b->n, fmt, ap);
        va_end(ap);
        if ((size_t)k < b->cap - b->n) {
            b->n += (size_t)k;
            return;
        }
        b->p = realloc(b->p, b->cap = b->cap * 2 + (size_t)k);
    }
}

/* A value that needs quotes to read back the same: empty is fine, but edge spaces or a comment character
 * after whitespace are not. */
static void put_value(Buf *b, const char *v)
{
    int quote = *v == ' ' || *v == '\t' || *v == '"' || (*v && (v[strlen(v) - 1] == ' ' || v[strlen(v) - 1] == '\t'));
    for (const char *c = v; *c && !quote; c++)
        if ((*c == ';' || *c == '#') && c > v && (c[-1] == ' ' || c[-1] == '\t'))
            quote = 1;
    put(b, quote ? "\"%s\"\n" : "%s\n", v);
}

static void put_comment(Buf *b, const char *text)
{
    if (text)
        put(b, "; %s\n", text);
}

char *pc_settings_format(const PcSettings *s)
{
    Buf b = {malloc(8192), 0, 8192};
    char v[PC_CFG_PATH + 8];
    b.p[0] = 0;
    put(&b, "; rush2049pc.ini: settings for the San Francisco Rush 2049 PC port (docs/port/FRONTEND.md).\n"
            "; Command-line options and the environment variables named below override these. Delete a line,\n"
            "; or the whole file, for the default; the file is written with the defaults when it is missing.\n");
    const char *section = "";
    for (size_t i = 0; i < NUM_ITEMS; i++) {
        if (strcmp(section, items[i].section)) {
            section = items[i].section;
            put(&b, "\n[%s]\n", section);
        }
        put_comment(&b, items[i].comment);
        format_item(s, &items[i], v, sizeof v);
        put(&b, "%s = ", items[i].key);
        put_value(&b, v);
    }
    put(&b, "\n; Bindings: N64 control = a comma-separated list; empty for none. The N64 controls are A B Z START,\n"
            "; DUP DDOWN DLEFT DRIGHT (the D-pad), L R, CUP CDOWN CLEFT CRIGHT and STICK_UP STICK_DOWN STICK_LEFT\n"
            "; STICK_RIGHT (the stick at full tilt).\n"
            "\n[keyboard]\n"
            "; SDL key names: letters, digits, Return, Space, Tab, Backspace, Left Shift, Right Ctrl, Up, Keypad 8,\n"
            "; F1... Punctuation by name: Comma, Period, Slash, Semicolon, Apostrophe, Minus, Equals, LeftBracket,\n"
            "; RightBracket, Backslash, Grave, Hash.\n");
    for (int i = 0; i < PC_NUM_CONTROLS; i++) {
        put(&b, "%s = ", pc_control_names[i]);
        put_value(&b, s->key[i]);
    }
    put(&b, "\n[controller]\n"
            "; SDL GameController names (XInput positions): a b x y (south east west north), back, guide, start,\n"
            "; leftshoulder, rightshoulder, leftstick, rightstick (clicks), dpup dpdown dpleft dpright, misc1,\n"
            "; paddle1-4, touchpad; and axes: lefttrigger, righttrigger, +leftx -leftx +lefty -lefty and the same\n"
            "; for rightx, righty (+x is right, +y is down). v5's layout instead: A = a, B = x, Z = lefttrigger,\n"
            "; L = leftshoulder, R = rightshoulder, righttrigger.\n"
            "; analog: the stick that is the N64 stick: left, right or none.\n");
    put(&b, "analog = %s\n", analog_choices[s->analog]);
    for (int i = 0; i < PC_NUM_CONTROLS; i++) {
        put(&b, "%s = ", pc_control_names[i]);
        put_value(&b, s->pad[i]);
    }
    put(&b, "\n[hotkeys]\n"
            "; Keys (names as in [keyboard]). Alt+Enter also toggles fullscreen. A hotkey is not also a game key.\n");
    for (int i = 0; i < PC_NUM_HOTKEYS; i++) {
        put(&b, "%s = ", pc_hotkey_names[i]);
        put_value(&b, s->hotkey[i]);
    }
    return b.p;
}

int pc_settings_write(const PcSettings *s, const char *path)
{
    char tmp[PC_CFG_PATH + 16];
    char *text = pc_settings_format(s);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    int ok = f != NULL;
    if (ok) {
        ok = fwrite(text, 1, strlen(text), f) == strlen(text);
        ok = fclose(f) == 0 && ok;
        ok = ok && rename(tmp, path) == 0;
        if (!ok)
            unlink(tmp);
    }
    free(text);
    return ok ? 0 : -1;
}

/* ---------------------------------------------------------------- the file's location */

static int file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

void pc_config_exe_dir(char *out, size_t out_size, const char *argv0)
{
    char path[PC_CFG_PATH];
    ssize_t n = readlink("/proc/self/exe", path, sizeof path - 1);
    if (n > 0)
        path[n] = 0;
    else if (argv0 && strchr(argv0, '/'))
        snprintf(path, sizeof path, "%s", argv0);
    else {
        out[0] = 0;
        return;
    }
    dir_of(path, out, out_size);
}

static int xdg_dir(char *out, size_t n)
{
    const char *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    if (xdg && xdg[0] == '/')   /* the spec: a relative XDG_CONFIG_HOME is ignored */
        snprintf(out, n, "%s/rush2049pc", xdg);
    else if (home && *home)
        snprintf(out, n, "%s/.config/rush2049pc", home);
    else
        return -1;
    return 0;
}

int pc_config_locate(char *out, size_t out_size, const char *exe_dir, int *exists)
{
    char dir[PC_CFG_PATH];
    if (exe_dir && *exe_dir) {
        snprintf(out, out_size, "%s/%s", exe_dir, FILE_NAME);
        if (file_exists(out))
            return *exists = 1, 0;
    }
    if (xdg_dir(dir, sizeof dir) != 0) {
        out[0] = 0;
        *exists = 0;
        return -1;
    }
    snprintf(out, out_size, "%s/%s", dir, FILE_NAME);
    *exists = file_exists(out);
    return 0;
}

static int mkdirs(const char *dir)
{
    char p[PC_CFG_PATH];
    snprintf(p, sizeof p, "%s", dir);
    for (char *c = p + 1; *c; c++)
        if (*c == '/') {
            *c = 0;
            if (mkdir(p, 0755) != 0 && errno != EEXIST)
                return -1;
            *c = '/';
        }
    return mkdir(p, 0755) != 0 && errno != EEXIST ? -1 : 0;
}

/* ---------------------------------------------------------------- the layers */

int pc_settings_resolve(PcSettings *s, int use_file, const char *path, const char *exe_dir,
                        const PcSettingValue *overrides, int n_overrides)
{
    char located[PC_CFG_PATH], why[200];
    int rc = 0, exists = 0;
    pc_settings_defaults(s);
    if (use_file && path) {
        if (pc_settings_load_file(s, path) < 0)
            fprintf(stderr, "rush2049pc: cannot read %s; using the defaults\n", path);
    } else if (use_file && pc_config_locate(located, sizeof located, exe_dir, &exists) == 0) {
        if (!exists && use_file == PC_CFG_FILE_CREATE) {
            /* first run: write the defaults where the file belongs, so there is one to edit */
            char dir[PC_CFG_PATH];
            dir_of(located, dir, sizeof dir);
            if (mkdirs(dir) == 0 && pc_settings_write(s, located) == 0)
                fprintf(stderr, "rush2049pc: wrote the default settings to %s\n", located);
            else
                fprintf(stderr, "rush2049pc: cannot write %s; using the defaults\n", located);
        }
        if (file_exists(located))
            pc_settings_load_file(s, located);
    }
    pc_settings_apply_env(s);
    for (int i = 0; i < n_overrides; i++) {
        const PcSettingValue *o = &overrides[i];
        if (pc_settings_set(s, o->section, o->key, o->value, why, sizeof why) != 0) {
            fprintf(stderr, "rush2049pc: %s.%s: %s\n", o->section, o->key, why);
            rc = -1;
        }
    }
    return rc;
}
