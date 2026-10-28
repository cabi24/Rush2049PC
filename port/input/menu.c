/* port/input/menu.c: the settings overlay (docs/port/FRONTEND.md "The settings overlay").
 *
 * F1 (hotkeys.menu), the controller's menu button (controller.menu: both stick clicks) or a script's MENU opens it over
 * the game. While it is open the game is held, as with the pause key (pc_input_paused: no retraces, timers or RSP
 * tasks, so the N64 threads stop where they wait), and its pads read idle. It is a small immediate-mode menu: pages
 * of items, each a setting or an action, drawn by port/gfx in window pixels over the last frame (pc_gfx_set_ui_hook)
 * and drawn again after each change (pc_gfx_redraw).
 *
 * - Changes apply at once where the part allows it: the renderer's widescreen, scale and textures (from the game's
 *   next frame) and frame rate, the window's size, fullscreen, its mode and resolution, and vsync, the volume, and
 *   everything port/input reads (bindings, hotkeys, sticks, rumble). The graphics API and the audio backend apply
 *   after a restart; they are marked so.
 * - Save writes the settings changed here (and different from the file) into rush2049pc.ini in place
 *   (pc_settings_save): the file's comments, order and other lines stay. Settings from the command line or the
 *   environment that weren't changed here are not written.
 * - A binding item captures the next key (the keyboard page, the hotkeys) or controller input (the controller
 *   page): input_sdl.c sends its name. Enter or A replaces the binding, Insert or Y adds to it, Delete or X clears
 *   it; Escape cancels, and so does waiting 6 seconds.
 * - Its input arrives as actions (pc_menu_action): keys and controller buttons from input_sdl.c, scripted presses
 *   from input.c. */
#define _GNU_SOURCE
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "input_internal.h"

/* ---------------------------------------------------------------- the pages */

enum { P_MAIN, P_VIDEO, P_AUDIO, P_KEYBOARD, P_CONTROLLER, NUM_PAGES };

typedef enum { I_PAGE, I_CHOICE, I_NUMBER, I_WINDOW, I_BIND, I_SAVE, I_DEFAULTS, I_RESUME, I_QUIT } Kind;

typedef struct {
    Kind kind;
    const char *label;
    const char *section, *key;    /* the setting, as the file names it */
    const char *const *choices;   /* I_CHOICE, I_WINDOW: the values in turn, as the file writes them */
    const char *const *labels;    /* I_CHOICE: what each choice shows, when not its value; NULL for all values */
    double min, max, step;        /* I_NUMBER */
    int percent;                  /* I_NUMBER: shown as a percentage, with a bar */
    int page;                     /* I_PAGE: the page it opens */
    int restart;                  /* applies after a restart */
} Item;

#define MAX_ITEMS 32
typedef struct {
    const char *title;
    Item item[MAX_ITEMS];
    int n;
} Page;

static Page pages[NUM_PAGES];

static const char *const onoff[] = {"off", "on", NULL};
static const char *const wide_choices[] = {"auto", "off", "16:10", "16:9", "21:9", NULL};
static const char *const scale_choices[] = {"native", "1", "2", "3", "4", "5", "6", "7", "8", "auto", NULL};
static const char *const scale_labels[] = {"native", "N64 (1x)", "2x", "3x", "4x", "5x", "6x", "7x", "8x",
                                           "auto (whole)", NULL};
static const char *const fs_mode_choices[] = {"desktop", "exclusive", NULL};
static const char *const res_choices[] = {"desktop", "720p", "1080p", "1440p", "1620p", "2160p", NULL};
static const char *const res_labels[] = {"desktop", "720p (1280x720)", "1080p (1920x1080)", "1440p (2560x1440)",
                                         "1620p (2880x1620)", "2160p (3840x2160)", NULL};
static const char *const rate_choices[] = {"original", "60", "120", "144", "display", NULL};
static const char *const api_choices[] = {"auto", "gl", "gles", NULL};
static const char *const textures_choices[] = {"original", "upscaled", "pack", NULL};
static const char *const backend_choices[] = {"auto", "sdl", "alsa", "null", "wav", NULL};
static const char *const analog_choices[] = {"left", "right", "none", NULL};
static const char *const window_choices[] = {"640x480",   "960x720",   "1280x960",  "1440x1080", "854x480",
                                             "1280x720",  "1600x900",  "1920x1080", "2560x1440", "2880x1620",
                                             "3840x2160", "auto",      NULL};
static const char *const control_labels[PC_NUM_CONTROLS] = {
    "A", "B", "Z", "Start", "D-pad up", "D-pad down", "D-pad left", "D-pad right", "L", "R",
    "C-up", "C-down", "C-left", "C-right", "Stick up", "Stick down", "Stick left", "Stick right"};
static const char *const hotkey_labels[PC_NUM_HOTKEYS] = {"Fullscreen key", "Screenshot key", "Pause key",
                                                          "Quit key", "Settings key"};

static Item *add(int page, Kind kind, const char *label, const char *section, const char *key)
{
    Item *it = &pages[page].item[pages[page].n++];
    *it = (Item){.kind = kind, .label = label, .section = section, .key = key};
    return it;
}

static void build(void)
{
    Item *it;
    memset(pages, 0, sizeof pages);
    pages[P_MAIN].title = "Settings";
    add(P_MAIN, I_PAGE, "Video", NULL, NULL)->page = P_VIDEO;
    add(P_MAIN, I_PAGE, "Audio", NULL, NULL)->page = P_AUDIO;
    add(P_MAIN, I_PAGE, "Keyboard", NULL, NULL)->page = P_KEYBOARD;
    add(P_MAIN, I_PAGE, "Controllers", NULL, NULL)->page = P_CONTROLLER;
    add(P_MAIN, I_SAVE, "Save to rush2049pc.ini", NULL, NULL);
    add(P_MAIN, I_RESUME, "Resume", NULL, NULL);
    add(P_MAIN, I_QUIT, "Quit", NULL, NULL);

    pages[P_VIDEO].title = "Video";
    it = add(P_VIDEO, I_CHOICE, "Render scale", "video", "scale");
    it->choices = scale_choices;
    it->labels = scale_labels;
    add(P_VIDEO, I_CHOICE, "Widescreen", "video", "widescreen")->choices = wide_choices;
    add(P_VIDEO, I_CHOICE, "Fullscreen", "video", "fullscreen")->choices = onoff;
    add(P_VIDEO, I_CHOICE, "Fullscreen mode", "video", "fullscreen_mode")->choices = fs_mode_choices;
    it = add(P_VIDEO, I_CHOICE, "Resolution", "video", "resolution");
    it->choices = res_choices;
    it->labels = res_labels;
    add(P_VIDEO, I_WINDOW, "Window size", "video", "width")->choices = window_choices;
    add(P_VIDEO, I_CHOICE, "Vsync", "video", "vsync")->choices = onoff;
    add(P_VIDEO, I_CHOICE, "Frame rate", "video", "frame_rate")->choices = rate_choices;
    add(P_VIDEO, I_CHOICE, "Pause in background", "video", "pause_on_focus_loss")->choices = onoff;
    it = add(P_VIDEO, I_CHOICE, "Graphics API", "video", "api");
    it->choices = api_choices;
    it->restart = 1;
    add(P_VIDEO, I_CHOICE, "Textures", "video", "textures")->choices = textures_choices;   /* TEXTURES.md */

    pages[P_AUDIO].title = "Audio";
    it = add(P_AUDIO, I_NUMBER, "Volume", "audio", "volume");
    it->min = 0, it->max = 100, it->step = 5, it->percent = 1;
    it = add(P_AUDIO, I_CHOICE, "Output", "audio", "backend");
    it->choices = backend_choices;
    it->restart = 1;

    pages[P_KEYBOARD].title = "Keyboard";
    for (int c = 0; c < PC_NUM_CONTROLS; c++)
        add(P_KEYBOARD, I_BIND, control_labels[c], "keyboard", pc_control_names[c]);
    for (int h = PC_NUM_HOTKEYS - 1; h >= 0; h--)   /* the settings key first */
        add(P_KEYBOARD, I_BIND, hotkey_labels[h], "hotkeys", pc_hotkey_names[h]);
    add(P_KEYBOARD, I_DEFAULTS, "Defaults", NULL, NULL);

    pages[P_CONTROLLER].title = "Controllers (all players)";
    for (int c = 0; c < PC_NUM_CONTROLS; c++)
        add(P_CONTROLLER, I_BIND, control_labels[c], "controller", pc_control_names[c]);
    add(P_CONTROLLER, I_BIND, "Settings button", "controller", "menu");
    add(P_CONTROLLER, I_CHOICE, "N64 stick", "controller", "analog")->choices = analog_choices;
    it = add(P_CONTROLLER, I_NUMBER, "Dead zone", "input", "deadzone");
    it->min = 0, it->max = 0.9, it->step = 0.05;
    it = add(P_CONTROLLER, I_NUMBER, "Rumble", "input", "rumble");
    it->min = 0, it->max = 1, it->step = 0.1, it->percent = 1;
    add(P_CONTROLLER, I_DEFAULTS, "Defaults", NULL, NULL);
}

/* ---------------------------------------------------------------- state */

#define MAX_TOUCHED 64
#define ROWS 15                 /* items shown at once */
#define CAPTURE_NS 6000000000u  /* a capture gives up after 6 s */

static struct {
    int inited, open, page, sel[NUM_PAGES], top[NUM_PAGES];
    int capture, capture_add;
    uint64_t capture_until;
    int dirty, quit;
    PcSettings eff;                  /* the settings in effect, with the changes made here */
    PcSettings file;                 /* the defaults with the file over them: what Save compares with */
    char path[PC_CFG_PATH];          /* where Save writes */
    char touched[MAX_TOUCHED][48];   /* "section.key" of each setting changed here */
    int ntouched;
    char status[160];
    uint32_t status_color;
} M;

#define C_DIM 0x00000060u
#define C_PANEL 0x0C1018C8u
#define C_EDGE 0x4A6EA8FFu
#define C_TITLE 0x1C2C4CFFu
#define C_SELECT 0x2E5AA0FFu
#define C_WHITE 0xFFFFFFFFu
#define C_LABEL 0xD0D4DCFFu
#define C_VALUE 0x8CC8FFFFu
#define C_CHANGED 0xFFD24AFFu
#define C_CAPTURE 0xFF8C5AFFu
#define C_HINT 0x9098A8FFu
#define C_OK 0x7CE07CFFu
#define C_WARN 0xFFA050FFu
#define C_BAR 0x3A4250FFu

static void draw(int w, int h);
static void status(uint32_t color, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void status(uint32_t color, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(M.status, sizeof M.status, fmt, ap);
    va_end(ap);
    M.status_color = color;
}

static Item *current(void)
{
    Page *p = &pages[M.page];
    return p->n ? &p->item[M.sel[M.page]] : NULL;
}

/* ---------------------------------------------------------------- values */

static void value_of(const PcSettings *s, const Item *it, char *out, size_t n)
{
    out[0] = 0;
    if (it->kind == I_WINDOW && (s->width <= 0 || s->height <= 0))
        snprintf(out, n, "auto");
    else if (it->kind == I_WINDOW)
        snprintf(out, n, "%dx%d", s->width, s->height);
    else if (it->section)
        pc_settings_get(s, it->section, it->key, out, n);
}

/* Changed here and not saved: different from the file. */
static int changed(const Item *it)
{
    char a[PC_CFG_BIND + 8], b[PC_CFG_BIND + 8];
    if (!it->section)
        return 0;
    value_of(&M.eff, it, a, sizeof a);
    value_of(&M.file, it, b, sizeof b);
    return strcmp(a, b) != 0;
}

static void touch(const char *section, const char *key)
{
    char name[48];
    snprintf(name, sizeof name, "%s.%s", section, key);
    for (int i = 0; i < M.ntouched; i++)
        if (!strcasecmp(M.touched[i], name))
            return;
    if (M.ntouched < MAX_TOUCHED)
        snprintf(M.touched[M.ntouched++], sizeof M.touched[0], "%s", name);
}

/* A setting changed: to the part that reads it, at once where it can. */
static void apply(const char *section, const char *key)
{
    const PcSettings *s = &M.eff;
    if (!strcasecmp(section, "video")) {
        if (!strcasecmp(key, "widescreen"))
            pc_gfx_set_widescreen((float)s->widescreen);
        else if (!strcasecmp(key, "scale"))
            pc_gfx_set_scale(s->scale);
        else if (!strcasecmp(key, "fullscreen"))
            pc_input_live_fullscreen(s->fullscreen);
        else if (!strcasecmp(key, "fullscreen_mode"))
            pc_gfx_set_fullscreen_mode(s->fullscreen_mode);
        else if (!strcasecmp(key, "resolution")) {
            int w, h;
            if (pc_settings_resolution(s->resolution, &w, &h) == 0)
                pc_gfx_set_resolution(w, h);
        }
        else if (!strcasecmp(key, "vsync"))
            pc_input_live_vsync(s->vsync);
        else if (!strcasecmp(key, "frame_rate"))
            pc_gfx_set_frame_rate(s->frame_rate);
        else if (!strcasecmp(key, "textures"))
            pc_gfx_set_textures(s->textures);
        else if (!strcasecmp(key, "width") || !strcasecmp(key, "height"))
            pc_input_live_window_size(s->width, s->height);
        else if (!strcasecmp(key, "pause_on_focus_loss"))
            pc_input_apply(s);
    } else if (!strcasecmp(section, "audio")) {
        if (!strcasecmp(key, "volume"))
            pc_audio_set_volume(s->volume / 100.0);
    } else
        pc_input_apply(s);   /* [input], [keyboard], [controller], [hotkeys] */
}

static int set_value(const Item *it, const char *v)
{
    char why[160];
    if (it->kind == I_WINDOW) {
        char w[16], h[16], tail;
        unsigned uw, uh;
        if (!strcasecmp(v, "auto"))   /* two thirds of the desktop, from the next start */
            snprintf(w, sizeof w, "auto"), snprintf(h, sizeof h, "auto");
        else if (sscanf(v, "%ux%u%c", &uw, &uh, &tail) != 2)
            return -1;
        else {
            snprintf(w, sizeof w, "%u", uw);
            snprintf(h, sizeof h, "%u", uh);
        }
        if (pc_settings_set(&M.eff, "video", "width", w, why, sizeof why) != 0 ||
            pc_settings_set(&M.eff, "video", "height", h, why, sizeof why) != 0) {
            status(C_WARN, "%s", why);
            return -1;
        }
        touch("video", "width");
        touch("video", "height");
        apply("video", "width");
        return 0;
    }
    if (pc_settings_set(&M.eff, it->section, it->key, v, why, sizeof why) != 0) {
        status(C_WARN, "%s", why);
        return -1;
    }
    touch(it->section, it->key);
    apply(it->section, it->key);
    return 0;
}

static void cycle(const Item *it, int dir)
{
    char v[64];
    int n = 0, i;
    value_of(&M.eff, it, v, sizeof v);
    while (it->choices[n])
        n++;
    for (i = 0; i < n && strcasecmp(v, it->choices[i]); i++)
        ;
    i = i == n ? (dir > 0 ? 0 : n - 1) : (i + dir + n) % n;   /* a value not in the list: from either end */
    set_value(it, it->choices[i]);
}

static void step(const Item *it, int dir)
{
    char v[64];
    value_of(&M.eff, it, v, sizeof v);
    double x = floor(strtod(v, NULL) / it->step + 0.5) * it->step + dir * it->step;
    x = x < it->min ? it->min : x > it->max ? it->max : x;
    snprintf(v, sizeof v, it->step >= 1 ? "%.0f" : "%.2f", x);
    set_value(it, v);
}

static int in_list(const char *list, const char *name)
{
    char buf[PC_CFG_BIND], *save = NULL;
    snprintf(buf, sizeof buf, "%s", list);
    for (char *tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        while (*tok == ' ' || *tok == '\t')
            tok++;
        char *e = tok + strlen(tok);
        while (e > tok && (e[-1] == ' ' || e[-1] == '\t'))
            *--e = 0;
        if (!strcasecmp(tok, name))
            return 1;
    }
    return 0;
}

/* After a capture: say where else the input is bound, or that a hotkey keeps it from the game. */
static void note_binding(const Item *it, const char *name)
{
    int pad = !strcasecmp(it->section, "controller"), hot = !strcasecmp(it->section, "hotkeys");
    for (int h = 0; !pad && h < PC_NUM_HOTKEYS; h++)
        if (in_list(M.eff.hotkey[h], name) && !(hot && !strcasecmp(it->key, pc_hotkey_names[h]))) {
            status(C_WARN, "%s is the %s too%s", name, hotkey_labels[h], hot ? "" : ": the game won't see it");
            return;
        }
    for (int c = 0; c < PC_NUM_CONTROLS; c++)
        if ((hot || strcasecmp(it->key, pc_control_names[c])) && in_list(pad ? M.eff.pad[c] : M.eff.key[c], name)) {
            status(C_WARN, "%s is also bound to %s", name, control_labels[c]);
            return;
        }
    char v[PC_CFG_BIND];
    value_of(&M.eff, it, v, sizeof v);
    status(C_OK, "%s: %s", it->label, v);
}

static void defaults(void)
{
    static PcSettings d;
    char v[PC_CFG_BIND + 8];
    pc_settings_defaults(&d);
    for (int i = 0; i < pages[M.page].n; i++) {
        const Item *it = &pages[M.page].item[i];
        if (!it->section)
            continue;
        value_of(&d, it, v, sizeof v);
        set_value(it, v);
    }
    status(C_OK, "Defaults set on this page (not saved yet)");
}

static int unsaved(void)
{
    char a[PC_CFG_BIND + 8], b[PC_CFG_BIND + 8], section[16];
    int n = 0;
    for (int i = 0; i < M.ntouched; i++) {
        const char *dot = strchr(M.touched[i], '.');
        snprintf(section, sizeof section, "%.*s", (int)(dot - M.touched[i]), M.touched[i]);
        pc_settings_get(&M.eff, section, dot + 1, a, sizeof a);
        pc_settings_get(&M.file, section, dot + 1, b, sizeof b);
        n += strcmp(a, b) != 0;
    }
    return n;
}

static void save(void)
{
    static char section[MAX_TOUCHED][16], value[MAX_TOUCHED][PC_CFG_BIND + 8];
    PcSettingValue ch[MAX_TOUCHED];
    char was[PC_CFG_BIND + 8], why[PC_CFG_PATH + 64];
    int n = 0;
    if (!M.path[0]) {
        status(C_WARN, "No place for rush2049pc.ini");
        return;
    }
    for (int i = 0; i < M.ntouched; i++) {
        const char *dot = strchr(M.touched[i], '.');
        snprintf(section[n], sizeof section[n], "%.*s", (int)(dot - M.touched[i]), M.touched[i]);
        pc_settings_get(&M.eff, section[n], dot + 1, value[n], sizeof value[n]);
        pc_settings_get(&M.file, section[n], dot + 1, was, sizeof was);
        if (strcmp(value[n], was)) {
            ch[n] = (PcSettingValue){section[n], dot + 1, value[n]};
            n++;
        }
    }
    if (!n) {
        status(C_HINT, "Nothing to save");
        return;
    }
    if (pc_settings_save(M.path, ch, n, why, sizeof why) != 0) {
        fprintf(stderr, "rush2049pc: settings not saved: %s\n", why);
        status(C_WARN, "Not saved: %s", why);
        return;
    }
    fprintf(stderr, "rush2049pc: saved %d setting%s to %s\n", n, n == 1 ? "" : "s", M.path);
    pc_settings_defaults(&M.file);
    pc_settings_read_quiet(&M.file, M.path);
    M.ntouched = 0;
    status(C_OK, "Saved %d setting%s", n, n == 1 ? "" : "s");
}

/* ---------------------------------------------------------------- opening, moving, choosing */

static void set_open(int on)
{
    if (on == M.open)
        return;
    M.open = on;
    M.capture = 0;
    M.status[0] = 0;
    M.dirty = 1;
    if (on) {
        int fs = pc_input_live_is_fullscreen();   /* F11 may have changed it */
        if (fs >= 0)
            M.eff.fullscreen = fs;
        M.page = P_MAIN;
        fprintf(stderr, "rush2049pc: settings open; the game is held\n");
    } else
        fprintf(stderr, "rush2049pc: settings closed\n");
    pc_gfx_set_ui_hook(on ? draw : NULL);
    pc_input_live_menu(on);
}

static void scroll(void)
{
    int *top = &M.top[M.page], sel = M.sel[M.page];
    if (sel < *top)
        *top = sel;
    if (sel >= *top + ROWS)
        *top = sel - ROWS + 1;
}

static void move(int by, int wrap)
{
    int n = pages[M.page].n, *sel = &M.sel[M.page];
    if (wrap)
        *sel = ((*sel + by) % n + n) % n;
    else
        *sel = *sel + by < 0 ? 0 : *sel + by >= n ? n - 1 : *sel + by;
    scroll();
}

static void start_capture(int add_to)
{
    const Item *it = current();
    M.capture = !strcasecmp(it->section, "controller") ? PC_MENU_CAPTURE_PAD : PC_MENU_CAPTURE_KEY;
    M.capture_add = add_to;
    M.capture_until = pc_host_ns() + CAPTURE_NS;
}

static void accept(Item *it)
{
    switch (it->kind) {
    case I_PAGE:
        M.page = it->page;
        break;
    case I_CHOICE:
    case I_WINDOW:
        cycle(it, 1);
        break;
    case I_NUMBER:
        step(it, 1);
        break;
    case I_BIND:
        start_capture(0);
        break;
    case I_SAVE:
        save();
        break;
    case I_DEFAULTS:
        defaults();
        break;
    case I_RESUME:
        set_open(0);
        break;
    case I_QUIT:
        M.quit = 1;
        break;
    }
}

void pc_menu_action(int action)
{
    if (!M.inited)
        return;
    if (action == PC_MENU_TOGGLE) {
        set_open(!M.open);
        return;
    }
    if (!M.open)
        return;
    M.dirty = 1;
    if (M.capture) {   /* only cancelling: the input itself comes through pc_menu_capture */
        if (action == PC_MENU_BACK)
            pc_menu_capture(NULL);
        else if (action == PC_MENU_CLOSE)
            set_open(0);
        return;
    }
    M.status[0] = 0;
    Item *it = current();
    switch (action) {
    case PC_MENU_UP:
    case PC_MENU_DOWN:
        move(action == PC_MENU_UP ? -1 : 1, 1);
        break;
    case PC_MENU_PAGE_UP:
    case PC_MENU_PAGE_DOWN:
        move(action == PC_MENU_PAGE_UP ? -ROWS : ROWS, 0);
        break;
    case PC_MENU_LEFT:
    case PC_MENU_RIGHT:
        if (it->kind == I_CHOICE || it->kind == I_WINDOW)
            cycle(it, action == PC_MENU_RIGHT ? 1 : -1);
        else if (it->kind == I_NUMBER)
            step(it, action == PC_MENU_RIGHT ? 1 : -1);
        break;
    case PC_MENU_ACCEPT:
        accept(it);
        break;
    case PC_MENU_BACK:
        if (M.page != P_MAIN)
            M.page = P_MAIN;
        else
            set_open(0);
        break;
    case PC_MENU_CLEAR:
        if (it->kind == I_BIND && set_value(it, "") == 0)
            status(C_OK, "%s: none", it->label);
        break;
    case PC_MENU_ADD:
        if (it->kind == I_BIND)
            start_capture(1);
        break;
    case PC_MENU_CLOSE:
        set_open(0);
        break;
    }
}

void pc_menu_capture(const char *name)
{
    char list[PC_CFG_BIND], v[2 * PC_CFG_BIND];
    const Item *it = current();
    if (!M.open || !M.capture || !it || it->kind != I_BIND)
        return;
    M.capture = 0;
    M.dirty = 1;
    if (!name) {
        status(C_HINT, "%s: not changed", it->label);
        return;
    }
    value_of(&M.eff, it, list, sizeof list);
    if (M.capture_add && list[0] && !in_list(list, name))
        snprintf(v, sizeof v, "%s, %s", list, name);
    else
        snprintf(v, sizeof v, "%s", M.capture_add && list[0] ? list : name);
    if (strlen(v) >= PC_CFG_BIND) {
        status(C_WARN, "%s: too many bindings", it->label);
        return;
    }
    if (set_value(it, v) == 0)
        note_binding(it, name);
}

int pc_menu_capturing(void)
{
    return M.open ? M.capture : 0;
}

int pc_menu_is_open(void)
{
    return M.open;
}

int pc_menu_tick(void)
{
    if (M.capture && pc_host_ns() >= M.capture_until)
        pc_menu_capture(NULL);
    if (M.dirty) {   /* while the game is held nothing else draws */
        M.dirty = 0;
        pc_gfx_redraw();
    }
    return M.quit;
}

/* ---------------------------------------------------------------- drawing */

/* The panel is 304 x 224 units, centered; a unit is the largest whole number of window pixels that fits 320 x 240
 * in the window, so the overlay grows with the window and stays sharp. */
#define PANEL_W 304
#define PANEL_H 224
#define ROW_H 12
#define LIST_Y 18
#define VALUE_X 168
#define VALUE_CHARS 20

static int g_k, g_ox, g_oy;

static void rect(int x, int y, int w, int h, uint32_t color)
{
    pc_gfx_ui_rect(g_ox + x * g_k, g_oy + y * g_k, w * g_k, h * g_k, color);
}

static void text(int x, int y, uint32_t color, const char *s)
{
    pc_gfx_ui_text(g_ox + x * g_k, g_oy + y * g_k, g_k, color, s);
}

/* At most chars characters, ending in ".." when cut; from_left keeps the end (a path). */
static void fit(char *s, size_t chars, int from_left)
{
    size_t n = strlen(s);
    if (n <= chars)
        return;
    if (from_left) {
        memmove(s + 2, s + n - (chars - 2), chars - 1);
        s[0] = s[1] = '.';
    } else {
        s[chars - 2] = s[chars - 1] = '.';
        s[chars] = 0;
    }
}

static void shown(const Item *it, char *out, size_t n)
{
    char v[PC_CFG_BIND + 8];
    value_of(&M.eff, it, v, sizeof v);
    switch (it->kind) {
    case I_PAGE:
        snprintf(out, n, ">");
        break;
    case I_NUMBER:
        if (it->percent)
            snprintf(out, n, "%.0f %%", strtod(v, NULL) * (it->max <= 1 ? 100 : 1));
        else
            snprintf(out, n, "%s", v);
        break;
    case I_BIND:
        snprintf(out, n, "%s", v[0] ? v : "-");
        break;
    case I_SAVE: {
        int u = unsaved();
        out[0] = 0;
        if (u)
            snprintf(out, n, "%d change%s", u, u == 1 ? "" : "s");
        break;
    }
    default:
        snprintf(out, n, "%s", v);
        for (int i = 0; it->labels && it->choices[i]; i++)
            if (!strcasecmp(v, it->choices[i]))
                snprintf(out, n, "%s", it->labels[i]);
    }
}

static const char *hint(const Item *it)
{
    if (M.capture == PC_MENU_CAPTURE_KEY)
        return "Press a key   Esc: cancel";
    if (M.capture)
        return "Press a button or move a stick   Esc: cancel";
    switch (it->kind) {
    case I_PAGE:
        return "Enter/A: open   Esc/B: close";
    case I_CHOICE:
    case I_NUMBER:
    case I_WINDOW:
        return "Left/Right: change   Esc/B: back";
    case I_BIND:
        return "Enter/A set  Ins/Y add  Del/X clear  Esc/B back";
    default:
        return M.page == P_MAIN ? "Enter/A: choose   Esc/B: close" : "Enter/A: choose   Esc/B: back";
    }
}

static void draw(int w, int h)
{
    const Page *p = &pages[M.page];
    const Item *sel = current();
    char line[PC_CFG_PATH + 64], v[PC_CFG_BIND + 8];
    g_k = w / 320 < h / 240 ? w / 320 : h / 240;
    if (g_k < 1)
        g_k = 1;
    g_ox = (w - PANEL_W * g_k) / 2;
    g_oy = (h - PANEL_H * g_k) / 2;
    pc_gfx_ui_rect(0, 0, w, h, C_DIM);
    rect(0, 0, PANEL_W, PANEL_H, C_PANEL);
    rect(0, 0, PANEL_W, 14, C_TITLE);
    rect(0, 0, PANEL_W, 1, C_EDGE);
    rect(0, PANEL_H - 1, PANEL_W, 1, C_EDGE);
    rect(0, 0, 1, PANEL_H, C_EDGE);
    rect(PANEL_W - 1, 0, 1, PANEL_H, C_EDGE);
    if (M.page == P_MAIN)
        snprintf(line, sizeof line, "RUSH 2049 SETTINGS");
    else
        snprintf(line, sizeof line, "SETTINGS > %s", p->title);
    text(6, 3, C_WHITE, line);
    text(PANEL_W - 6 - 6 * 6, 3, C_HINT, "paused");

    for (int i = 0; i < ROWS && M.top[M.page] + i < p->n; i++) {
        const Item *it = &p->item[M.top[M.page] + i];
        int y = LIST_Y + i * ROW_H, here = it == sel;
        uint32_t color = here && M.capture ? C_CAPTURE : changed(it) ? C_CHANGED : here ? C_WHITE : C_VALUE;
        if (here)
            rect(2, y, PANEL_W - 4, ROW_H, C_SELECT);
        snprintf(line, sizeof line, "%s%s", it->label, it->restart ? " *" : "");
        text(8, y + 2, here ? C_WHITE : C_LABEL, line);
        if (here && M.capture)
            snprintf(v, sizeof v, M.capture == PC_MENU_CAPTURE_KEY ? "press a key..." : "press a button...");
        else
            shown(it, v, sizeof v);
        if (it->kind == I_NUMBER && it->percent) {   /* a bar, then the number */
            double f = strtod(v, NULL) / 100;
            rect(VALUE_X, y + 4, 60, 4, C_BAR);
            rect(VALUE_X, y + 4, (int)(60 * f + 0.5), 4, color);
            text(VALUE_X + 66, y + 2, color, v);
        } else {
            fit(v, VALUE_CHARS, 0);
            text(VALUE_X, y + 2, color, v);
        }
    }
    if (M.top[M.page] > 0)
        text(PANEL_W - 10, LIST_Y + 2, C_HINT, "^");
    if (M.top[M.page] + ROWS < p->n)
        text(PANEL_W - 10, LIST_Y + (ROWS - 1) * ROW_H + 2, C_HINT, "v");

    /* the footer: a message, then the keys */
    rect(4, LIST_Y + ROWS * ROW_H + 1, PANEL_W - 8, 1, C_EDGE);
    if (M.status[0]) {
        snprintf(line, sizeof line, "%s", M.status);
        fit(line, 49, 0);
        text(6, PANEL_H - 22, M.status_color, line);
    } else if (sel && sel->restart)
        text(6, PANEL_H - 22, C_HINT, "* applies after a restart");
    else if (M.page == P_MAIN) {
        snprintf(line, sizeof line, "%s", M.path[0] ? M.path : "no settings file");
        fit(line, 49, 1);
        text(6, PANEL_H - 22, C_HINT, line);
    }
    if (sel)
        text(6, PANEL_H - 11, C_HINT, hint(sel));
}

/* ---------------------------------------------------------------- setup */

void pc_menu_init(const PcSettings *s)
{
    build();
    memset(&M, 0, sizeof M);
    M.eff = *s;
    snprintf(M.path, sizeof M.path, "%s", s->save_path);
    pc_settings_defaults(&M.file);
    if (M.path[0])
        pc_settings_read_quiet(&M.file, M.path);
    M.inited = 1;
}

void pc_menu_shutdown(void)
{
    if (M.open)
        pc_gfx_set_ui_hook(NULL);
    M.open = M.inited = 0;
}

void pc_menu_state(const char **page, const char **item, char *value, size_t value_size)
{
    const Item *it = current();
    *page = pages[M.page].title;
    *item = it ? it->label : "";
    if (value && it)
        shown(it, value, value_size);
    else if (value)
        value[0] = 0;
}

const PcSettings *pc_menu_settings(void)
{
    return &M.eff;
}
