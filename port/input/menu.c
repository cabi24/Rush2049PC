/* port/input/menu.c: the settings overlay (docs/port/FRONTEND.md "The settings overlay").
 *
 * F1 (hotkeys.menu), the controller's menu button (controller.menu: both stick clicks) or a script's MENU opens it over
 * the game. While it is open the game is held, as with the pause key (pc_input_paused: no retraces, timers or RSP
 * tasks, so the N64 threads stop where they wait), and its pads read idle. It is a small immediate-mode menu: pages
 * of items, each a setting or an action, drawn by port/gfx in window pixels over the last frame (pc_gfx_set_ui_hook)
 * and drawn again after each change (pc_gfx_redraw).
 *
 * - Changes apply at once where the part allows it: the renderer's widescreen, scale and textures (from the game's
 *   next frame) and frame rate, the window's size, fullscreen, its mode and resolution, and vsync, the volume, each
 *   player's accessory (a hot swap: port/ultra/si.c), and everything port/input reads (bindings, hotkeys, sticks,
 *   rumble). The graphics API and the audio backend apply
 *   after a restart; they are marked so.
 * - Save writes the settings changed here (and different from the file) into rush2049pc.ini in place
 *   (pc_settings_save): the file's comments, order and other lines stay. Settings from the command line or the
 *   environment that weren't changed here are not written.
 * - A binding item captures the next key (the keyboard page, the hotkeys) or controller input (the controller
 *   page): input_sdl.c sends its name. Enter or A replaces the binding, Insert or Y adds to it, Delete or X clears
 *   it; Escape cancels, and so does waiting 6 seconds.
 * - Its input arrives as actions (pc_menu_action): keys and controller buttons from input_sdl.c, scripted presses
 *   from input.c; and the mouse (pc_menu_mouse, from input_sdl.c, in the drawable's pixels): pointing at an item
 *   selects it, a click chooses it (on a value: left of its middle the previous one, right of it the next), the
 *   wheel scrolls a long page, and the right button or a click outside the panel goes back.
 * - Help and About are read-only pages, made each time they are opened: Help from the bindings in effect, About from
 *   main's facts (pc_menu_set_about), the GL driver and the players. */
#define _GNU_SOURCE
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "input_internal.h"

/* ---------------------------------------------------------------- the pages */

enum { P_MAIN, P_VIDEO, P_AUDIO, P_KEYBOARD, P_CONTROLLER, P_HELP, P_ABOUT, NUM_PAGES };

/* I_INFO: a read-only row of Help or About, which can be selected (its whole text is in the footer); I_HEADING: one
 * that can't; I_OPEN, I_COPY: About's actions, the data folder in the file manager and the page to the clipboard. */
typedef enum { I_PAGE, I_CHOICE, I_NUMBER, I_WINDOW, I_BIND, I_SAVE, I_DEFAULTS, I_RESUME, I_QUIT, I_PLAYER,
               I_INFO, I_HEADING, I_OPEN, I_COPY } Kind;

typedef struct {
    Kind kind;
    const char *label;
    const char *section, *key;    /* the setting, as the file names it */
    const char *const *choices;   /* I_CHOICE, I_WINDOW: the values in turn, as the file writes them */
    const char *const *labels;    /* I_CHOICE: what each choice shows, when not its value; NULL for all values */
    double min, max, step;        /* I_NUMBER */
    int percent;                  /* I_NUMBER: shown as a percentage, with a bar */
    int page;                     /* I_PAGE: the page it opens; I_PLAYER and an accessory: the player (0-3) */
    int restart;                  /* applies after a restart */
    const char *desc;             /* the footer's line about it (NULL: made from the item, or the setting's name) */
    int row;                      /* I_INFO, I_HEADING: its text in the page's rows */
} Item;

/* An info page's row: a label and one value (About), or the keyboard's and the controller's (Help). */
typedef struct {
    char label[32];
    char a[PC_CFG_PATH + 64];
    char b[2 * PC_CFG_BIND];
    const char *desc;             /* the footer's line; NULL: the values */
    int path;                     /* a, cut, keeps its end */
} Row;

#define MAX_ITEMS 40
#define MAX_ROWS 36
typedef struct {
    const char *title;
    Item item[MAX_ITEMS];
    int n;
    Row *rows;                    /* an info page's text */
    int col_a, col_b;             /* where its value columns start, in units (col_b 0: one value) */
} Page;

static Page pages[NUM_PAGES];
static Row help_rows[MAX_ROWS], about_rows[MAX_ROWS];

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
static const char *const hud_choices[] = {"edges", "centered", NULL};
static const char *const hud_labels[] = {"screen edges", "centered (4:3)", NULL};
static const char *const backend_choices[] = {"auto", "sdl", "alsa", "null", "wav", NULL};
static const char *const analog_choices[] = {"left", "right", "none", NULL};
static const char *const pak_choices[] = {"memory", "rumble", "auto", "none", NULL};
static const char *const pak_labels[] = {"controller pak", "rumble pak", "auto", "none", NULL};
static const char *const pak_keys[4] = {"pak", "pak2", "pak3", "pak4"};   /* game.pak, game.pak2... */
static const char *const window_choices[] = {"640x480",   "960x720",   "1280x960",  "1440x1080", "854x480",
                                             "1280x720",  "1600x900",  "1920x1080", "2560x1440", "2880x1620",
                                             "3840x2160", "auto",      NULL};
static const char *const control_labels[PC_NUM_CONTROLS] = {
    "A", "B", "Z", "Start", "D-pad up", "D-pad down", "D-pad left", "D-pad right", "L", "R",
    "C-up", "C-down", "C-left", "C-right", "Stick up", "Stick down", "Stick left", "Stick right"};
static const char *const hotkey_labels[PC_NUM_HOTKEYS] = {"Fullscreen key", "Screenshot key", "Pause key",
                                                          "Quit key", "Settings key"};
static const char *const hotkey_descs[PC_NUM_HOTKEYS] = {
    "Fullscreen on and off; Alt+Enter does it too", "A PNG of the frame, in the screenshots folder",
    "Holds the game; press it again to go on", "Quits at once (not while this menu is open)",
    "Opens and closes this menu"};

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
    it = add(P_MAIN, I_PAGE, "Video", NULL, NULL);
    it->page = P_VIDEO, it->desc = "Scale, widescreen, fullscreen, frame rate";
    it = add(P_MAIN, I_PAGE, "Audio", NULL, NULL);
    it->page = P_AUDIO, it->desc = "The volume and the sound output";
    it = add(P_MAIN, I_PAGE, "Keyboard", NULL, NULL);
    it->page = P_KEYBOARD, it->desc = "Keys for the N64 controls, and the hotkeys";
    it = add(P_MAIN, I_PAGE, "Controllers", NULL, NULL);
    it->page = P_CONTROLLER, it->desc = "Players, buttons, the stick, dead zone, rumble";
    it = add(P_MAIN, I_PAGE, "Help", NULL, NULL);
    it->page = P_HELP, it->desc = "What each key and button does";
    it = add(P_MAIN, I_PAGE, "About", NULL, NULL);
    it->page = P_ABOUT, it->desc = "Version, graphics, sound, players, files";
    add(P_MAIN, I_SAVE, "Save to rush2049pc.ini", NULL, NULL);
    add(P_MAIN, I_RESUME, "Resume", NULL, NULL)->desc = "Back to the game";
    add(P_MAIN, I_QUIT, "Quit", NULL, NULL)->desc = "Quit Rush 2049 (changes not saved are lost)";

    pages[P_HELP].title = "Help";
    pages[P_HELP].rows = help_rows;
    pages[P_HELP].col_a = 80, pages[P_HELP].col_b = 188;
    pages[P_ABOUT].title = "About";
    pages[P_ABOUT].rows = about_rows;
    pages[P_ABOUT].col_a = 92;

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
    it = add(P_VIDEO, I_CHOICE, "Race HUD (widescreen)", "video", "hud");   /* GFX.md: at the screen's edges */
    it->choices = hud_choices;
    it->labels = hud_labels;

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
    for (int p = 0; p < 4; p++)   /* who plays each player, and the stick it gives: not choosable */
        add(P_CONTROLLER, I_PLAYER, "", NULL, NULL)->page = p;
    for (int c = 0; c < PC_NUM_CONTROLS; c++)
        add(P_CONTROLLER, I_BIND, control_labels[c], "controller", pc_control_names[c]);
    add(P_CONTROLLER, I_BIND, "Settings button", "controller", "menu");
    add(P_CONTROLLER, I_CHOICE, "N64 stick", "controller", "analog")->choices = analog_choices;
    it = add(P_CONTROLLER, I_NUMBER, "Dead zone", "input", "deadzone");
    it->min = 0, it->max = 0.9, it->step = 0.05;
    it = add(P_CONTROLLER, I_NUMBER, "Sensitivity", "input", "sensitivity");
    it->min = 0.5, it->max = 2, it->step = 0.1;
    it = add(P_CONTROLLER, I_NUMBER, "Rumble", "input", "rumble");
    it->min = 0, it->max = 1, it->step = 0.1, it->percent = 1;
    for (int p = 0; p < 4; p++) {   /* each player's accessory: a hot swap */
        static const char *const labels[4] = {"P1 accessory", "P2 accessory", "P3 accessory", "P4 accessory"};
        it = add(P_CONTROLLER, I_CHOICE, labels[p], "game", pak_keys[p]);
        it->choices = pak_choices, it->labels = pak_labels, it->page = p;
        it->desc = "Saves, rumble or none; auto: rumble if its pad can";
    }
    add(P_CONTROLLER, I_DEFAULTS, "Defaults", NULL, NULL);

    /* each setting's line in the footer (a setting without one shows its name in the file) */
    static const struct { const char *section, *key, *desc; } descs[] = {
        {"video", "scale", "Internal resolution; native: the window's pixels"},
        {"video", "widescreen", "A wider 3D view; auto: the window's shape"},
        {"video", "fullscreen", "Fill the screen; F11 and Alt+Enter do it too"},
        {"video", "fullscreen_mode", "Desktop: borderless; exclusive: sets a mode"},
        {"video", "resolution", "Fullscreen's size; desktop: the screen's own"},
        {"video", "width", "The window in pixels; auto: from the next start"},
        {"video", "vsync", "Wait for the display: no tearing"},
        {"video", "frame_rate", "Frames a second; between the game's: blended"},
        {"video", "pause_on_focus_loss", "Hold the game while in the background"},
        {"video", "api", "OpenGL or OpenGL ES; applies after a restart"},
        {"video", "textures", "Original, upscaled 2x or 4x, or a texture pack"},
        {"video", "hud", "Widescreen races: the HUD at the screen's edges"},
        {"audio", "volume", "The master volume"},
        {"audio", "backend", "The sound output; applies after a restart"},
        {"controller", "menu", "Opens this menu; + joins buttons held together"},
        {"controller", "analog", "The controller stick that is the N64 stick"},
        {"input", "deadzone", "How far the stick moves before it counts"},
        {"input", "sensitivity", "Stick speed: at 2, full tilt at half the travel"},
        {"input", "rumble", "Rumble strength, with the rumble pak in"},
    };
    for (int pg = 0; pg < NUM_PAGES; pg++)
        for (int i = 0; i < pages[pg].n; i++) {
            Item *x = &pages[pg].item[i];
            for (size_t d = 0; x->section && !x->desc && d < sizeof descs / sizeof *descs; d++)
                if (!strcasecmp(x->section, descs[d].section) && !strcasecmp(x->key, descs[d].key))
                    x->desc = descs[d].desc;
            for (int h = 0; x->kind == I_BIND && !strcasecmp(x->section, "hotkeys") && h < PC_NUM_HOTKEYS; h++)
                if (!strcasecmp(x->key, pc_hotkey_names[h]))
                    x->desc = hotkey_descs[h];
            if (x->kind == I_DEFAULTS)
                x->desc = "Puts this page's settings back to the defaults";
        }
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
    uint64_t status_ns;              /* when it was set: pointing elsewhere clears it after a moment */
    char players[512];               /* the Controllers page's player rows and accessories as last drawn */
    int saved;                       /* Save wrote the file, and nothing changed since */
    int mouse, mx, my, mw, mh;       /* the mouse's last place over the window (drawable pixels), and its size */
    uint64_t page_click_ns;          /* when a click last changed the page (0: not yet) */
    char data_dir[PC_CFG_PATH];      /* About's data folder, for Open */
} M;

/* main's facts for the About page; kept across pc_menu_init */
static void (*s_about_fill)(PcAbout *about);

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
    M.status_ns = pc_host_ns();
}

static int selectable(const Item *it)
{
    return it->kind != I_PLAYER && it->kind != I_HEADING;
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
        else if (!strcasecmp(key, "hud"))
            pc_gfx_set_hud(s->hud);
        else if (!strcasecmp(key, "width") || !strcasecmp(key, "height"))
            pc_input_live_window_size(s->width, s->height);
        else if (!strcasecmp(key, "pause_on_focus_loss"))
            pc_input_apply(s);
    } else if (!strcasecmp(section, "audio")) {
        if (!strcasecmp(key, "volume"))
            pc_audio_set_volume(s->volume / 100.0);
    } else if (!strcasecmp(section, "game")) {
        for (int p = 0; p < 4; p++)   /* the player takes the old accessory out and puts the new one in */
            if (!strcasecmp(key, pak_keys[p]))
                pc_si_set_pak(p, s->pak[p]);
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
    M.saved = 1;
    const char *base = M.path + strlen(M.path);
    while (base > M.path && base[-1] != '/' && base[-1] != '\\')
        base--;
    status(C_OK, "Saved %d setting%s to %s", n, n == 1 ? "" : "s", base);
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
        M.mouse = 0;   /* until it moves: a mouse resting where an item now is doesn't select it */
        fprintf(stderr, "rush2049pc: settings open; the game is held\n");
    } else {
        int u = unsaved();
        if (u)
            fprintf(stderr, "rush2049pc: settings closed; %d change%s not saved\n", u, u == 1 ? "" : "s");
        else
            fprintf(stderr, "rush2049pc: settings closed\n");
    }
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

/* The player rows and the headings can't be chosen: the selection steps over them, the way it moved (down when it
 * stopped). */
static void skip_rows(int dir, int wrap)
{
    const Page *p = &pages[M.page];
    int *sel = &M.sel[M.page];
    for (int k = 0; k < p->n && !selectable(&p->item[*sel]); k++) {
        int next = *sel + (dir < 0 ? -1 : 1);
        if (!wrap && (next < 0 || next >= p->n))
            dir = -dir, next = *sel + (dir < 0 ? -1 : 1);
        *sel = (next % p->n + p->n) % p->n;
    }
}

static int first_selectable(void)
{
    const Page *p = &pages[M.page];
    int i = 0;
    while (i < p->n - 1 && !selectable(&p->item[i]))
        i++;
    return i;
}

static void move(int by, int wrap)
{
    int n = pages[M.page].n, *sel = &M.sel[M.page];
    if (wrap)
        *sel = ((*sel + by) % n + n) % n;
    else
        *sel = *sel + by < 0 ? 0 : *sel + by >= n ? n - 1 : *sel + by;
    skip_rows(by, wrap);
    scroll();
    if (M.sel[M.page] <= first_selectable())   /* at the top: the player rows or a heading above it show */
        M.top[M.page] = 0;
}

/* The wheel and the scroll arrows: the list moves by rows; the selection stays on screen. 1 when it moved. */
static int scroll_by(int rows)
{
    const Page *p = &pages[M.page];
    int *top = &M.top[M.page], *sel = &M.sel[M.page], most = p->n > ROWS ? p->n - ROWS : 0;
    int t = *top + rows < 0 ? 0 : *top + rows > most ? most : *top + rows;
    if (t == *top)
        return 0;
    *top = t;
    M.dirty = 1;
    if (*sel < t) {
        *sel = t;
        skip_rows(1, 0);
    } else if (*sel >= t + ROWS) {
        *sel = t + ROWS - 1;
        skip_rows(-1, 0);
    }
    return 1;
}

static void fill_help(void);
static void fill_about(void);

/* Open a page; Help and About are made again from what is in effect now. */
static void open_page(int page)
{
    M.page = page;
    if (page == P_HELP)
        fill_help();
    else if (page == P_ABOUT)
        fill_about();
    if (M.sel[page] >= pages[page].n)
        M.sel[page] = 0;
    skip_rows(1, 0);
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
        open_page(it->page);
        break;
    case I_OPEN:
        if (pc_input_live_open_folder(M.data_dir) == 0)
            status(C_OK, "Opening the data folder");
        else
            status(C_WARN, "No file manager to open it with");
        break;
    case I_COPY: {
        static char text[MAX_ROWS * (PC_CFG_PATH + 96)];
        pc_menu_about_text(text, sizeof text);
        if (pc_input_live_clipboard(text) == 0)
            status(C_OK, "Copied: paste it into a bug report");
        else
            status(C_WARN, "No clipboard");
        break;
    }
    case I_INFO:
    case I_HEADING:
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
    case I_PLAYER:
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

/* A player row: "P1 keyboard + Xbox One" and its stick. */
static void player_row(int p, char *label, size_t ln, char *value, size_t vn)
{
    char who[96];
    int8_t x, y;
    int what = pc_input_live_describe(p, who, sizeof who, &x, &y);
    snprintf(label, ln, "P%d %s", p + 1, what ? who : "-");
    if (what == 2)
        snprintf(value, vn, "stick %+4d %+4d", x, y);
    else
        value[0] = 0;
}

/* ---------------------------------------------------------------- Help and About */

static Row *row(int page, Kind kind, const char *label, const char *a, const char *b)
{
    static Row spare;   /* past the end: not shown */
    Page *p = &pages[page];
    if (p->n >= MAX_ROWS || p->n >= MAX_ITEMS)
        return &spare;
    Row *r = &p->rows[p->n];
    memset(r, 0, sizeof *r);
    snprintf(r->label, sizeof r->label, "%s", label);
    snprintf(r->a, sizeof r->a, "%s", a && *a ? a : kind == I_HEADING ? "" : "-");
    snprintf(r->b, sizeof r->b, "%s", b ? b : "");
    Item *it = add(page, kind, r->label, NULL, NULL);
    it->row = (int)(r - p->rows);
    return r;
}

/* A controller binding as a player reads it: Xbox names ("a, righttrigger" -> "A, RT"; "-righty" -> "RS up"), and
 * a combination's parts joined by '+' ("leftstick+rightstick" -> "L3+R3"). */
static void pad_words(const char *list, char *out, size_t n)
{
    static const char *const names[][2] = {
        {"a", "A"}, {"b", "B"}, {"x", "X"}, {"y", "Y"}, {"back", "Back"}, {"guide", "Guide"}, {"start", "Start"},
        {"leftstick", "L3"}, {"rightstick", "R3"}, {"leftshoulder", "LB"}, {"rightshoulder", "RB"},
        {"dpup", "D-pad up"}, {"dpdown", "D-pad down"}, {"dpleft", "D-pad left"}, {"dpright", "D-pad right"},
        {"misc1", "Share"}, {"paddle1", "P1"}, {"paddle2", "P2"}, {"paddle3", "P3"}, {"paddle4", "P4"},
        {"touchpad", "Touchpad"}, {"lefttrigger", "LT"}, {"righttrigger", "RT"},
        {"+leftx", "LS right"}, {"leftx", "LS right"}, {"-leftx", "LS left"}, {"+lefty", "LS down"},
        {"lefty", "LS down"}, {"-lefty", "LS up"}, {"+rightx", "RS right"}, {"rightx", "RS right"},
        {"-rightx", "RS left"}, {"+righty", "RS down"}, {"righty", "RS down"}, {"-righty", "RS up"}};
    char buf[PC_CFG_BIND], *save = NULL;
    size_t len = 0;
    out[0] = 0;
    snprintf(buf, sizeof buf, "%s", list);
    for (char *tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        while (*tok == ' ' || *tok == '\t')
            tok++;
        char *e = tok + strlen(tok);
        while (e > tok && (e[-1] == ' ' || e[-1] == '\t'))
            *--e = 0;
        if (!*tok)
            continue;
        if (len && len < n)
            len += (size_t)snprintf(out + len, n - len, ", ");
        for (char *p = tok; *p && len < n;) {   /* the parts of a combination; a '+' that starts one is a sign */
            char *plus = strchr(p + 1, '+');
            size_t k = plus ? (size_t)(plus - p) : strlen(p);
            const char *word = NULL;
            for (size_t i = 0; i < sizeof names / sizeof *names && !word; i++)
                if (strlen(names[i][0]) == k && !strncasecmp(p, names[i][0], k))
                    word = names[i][1];
            len += (size_t)snprintf(out + len, n - len, "%s%.*s", p == tok ? "" : "+", word ? (int)strlen(word) : (int)k,
                                    word ? word : p);
            p += k + (plus != NULL);
        }
    }
    if (len >= n && n > 0)
        out[n - 1] = 0;
}

/* Help: each N64 control and what plays it, from the bindings in effect; the hotkeys; this menu's own keys. */
static void fill_help(void)
{
    static const char *const stick_dir[4] = {"up", "down", "left", "right"};
    const PcSettings *s = &M.eff;
    char head[32], pad[2 * PC_CFG_BIND], keys[PC_CFG_BIND + 16];
    pages[P_HELP].n = 0;
    if (s->keyboard_player >= 1 && s->keyboard_player <= 4)
        snprintf(head, sizeof head, "Keyboard (P%d)", s->keyboard_player);
    else
        snprintf(head, sizeof head, "Keyboard (off)");
    row(P_HELP, I_HEADING, "N64", head, "Controller");
    for (int c = 0; c < PC_NUM_CONTROLS; c++) {
        pad_words(s->pad[c], pad, sizeof pad);
        if (c >= PC_CTL_STICK_UP && s->analog != PC_CFG_STICK_NONE) {   /* the N64 stick's own stick first */
            char more[2 * PC_CFG_BIND];
            snprintf(more, sizeof more, "%s", pad);
            snprintf(pad, sizeof pad, "%s %s%s%.*s", s->analog == PC_CFG_STICK_LEFT ? "LS" : "RS",
                     stick_dir[c - PC_CTL_STICK_UP], more[0] ? ", " : "", (int)sizeof pad - 16, more);
        }
        row(P_HELP, I_INFO, control_labels[c], s->key[c], pad);
    }
    row(P_HELP, I_HEADING, "Hotkeys", "", "");
    pad_words(s->pad_menu, pad, sizeof pad);
    row(P_HELP, I_INFO, "Settings", s->hotkey[PC_HOTKEY_MENU], pad)->desc = "Opens and closes this menu";
    snprintf(keys, sizeof keys, "%s%sAlt+Enter", s->hotkey[PC_HOTKEY_FULLSCREEN],
             s->hotkey[PC_HOTKEY_FULLSCREEN][0] ? ", " : "");
    row(P_HELP, I_INFO, "Fullscreen", keys, "")->desc = "Fullscreen on and off";
    row(P_HELP, I_INFO, "Screenshot", s->hotkey[PC_HOTKEY_SCREENSHOT], "")->desc =
        "A PNG of the frame in the screenshots folder";
    row(P_HELP, I_INFO, "Pause", s->hotkey[PC_HOTKEY_PAUSE], "")->desc = "Holds the game; again to go on";
    row(P_HELP, I_INFO, "Quit", s->hotkey[PC_HOTKEY_QUIT], "")->desc = "Quits at once";
    row(P_HELP, I_HEADING, "This menu", "", "");
    row(P_HELP, I_INFO, "Move", "arrows, PgUp/PgDn", "D-pad, LS, LB/RB")->desc =
        "Held, they repeat; a page with PgUp/PgDn, LB/RB";
    row(P_HELP, I_INFO, "Choose", "Enter, Space", "A")->desc = "Open, change, or set a binding";
    row(P_HELP, I_INFO, "Back", "Escape", "B")->desc = "Back a page; on the first one, close";
    row(P_HELP, I_INFO, "Bindings", "Insert, Delete", "Y, X")->desc = "Add to a binding (Ins, Y), clear it (Del, X)";
    row(P_HELP, I_INFO, "Mouse", "point, click", "wheel: scroll")->desc =
        "Right button or outside the panel: back";
}

/* The players, as About shows them; 1 when that changed. */
static int about_players(void)
{
    int changed_any = 0;
    for (int i = 0; i < pages[P_ABOUT].n; i++) {
        Row *r = &about_rows[pages[P_ABOUT].item[i].row];
        int p;
        char who[96], now[sizeof r->a];
        int8_t x, y;
        if (pages[P_ABOUT].item[i].kind != I_INFO || sscanf(r->label, "Player %d", &p) != 1 || p < 1 || p > 4)
            continue;
        snprintf(now, sizeof now, "%s", pc_input_live_describe(p - 1, who, sizeof who, &x, &y) ? who : "-");
        if (strcmp(now, r->a)) {
            snprintf(r->a, sizeof r->a, "%s", now);
            changed_any = 1;
        }
    }
    return changed_any;
}

/* About: the version, the graphics driver, the sound, the players, the ROM and where the files are. */
static void fill_about(void)
{
    static PcAbout a;
    const char *vendor, *renderer, *version, *glsl;
    char line[PC_CFG_PATH + 64];
    memset(&a, 0, sizeof a);
    if (s_about_fill)
        s_about_fill(&a);
    pages[P_ABOUT].n = 0;
    snprintf(line, sizeof line, "%s%s%s%s", a.version[0] ? a.version : "unknown", a.build_type[0] ? " (" : "",
             a.build_type, a.build_type[0] ? ")" : "");
    row(P_ABOUT, I_INFO, "Version", line, NULL);
    row(P_ABOUT, I_INFO, "Commit", a.commit, NULL);
    row(P_ABOUT, I_INFO, "Build", a.build, NULL);
    pc_gfx_driver(&vendor, &renderer, &version, &glsl);
    row(P_ABOUT, I_INFO, "GL vendor", vendor, NULL);
    row(P_ABOUT, I_INFO, "GL renderer", renderer, NULL);
    snprintf(line, sizeof line, "%s%s%s%s", version, glsl[0] ? " (GLSL " : "", glsl, glsl[0] ? ")" : "");
    row(P_ABOUT, I_INFO, "GL version", version[0] ? line : "", NULL);
    row(P_ABOUT, I_INFO, "Audio", a.audio, NULL);
    row(P_ABOUT, I_INFO, "Audio buffer", a.audio_buffer, NULL);
    for (int p = 0; p < 4; p++) {
        snprintf(line, sizeof line, "Player %d", p + 1);
        row(P_ABOUT, I_INFO, line, "", NULL);
    }
    about_players();
    row(P_ABOUT, I_INFO, "ROM", a.rom, NULL)->path = 1;
    row(P_ABOUT, I_INFO, "Settings file", M.path[0] ? M.path : "none (--no-config)", NULL)->path = 1;
    row(P_ABOUT, I_INFO, "Saves", a.saves, NULL)->path = 1;
    row(P_ABOUT, I_INFO, "Screenshots", M.eff.screenshot_dir, NULL)->path = 1;
    row(P_ABOUT, I_INFO, "Log", a.log, NULL)->path = 1;
    row(P_ABOUT, I_INFO, "Data folder", a.data, NULL)->path = 1;
    snprintf(M.data_dir, sizeof M.data_dir, "%s", a.data);
    int desktop = pc_input_live_desktop();
    if (desktop >= 2 && M.data_dir[0])   /* a window on a real display: not headless, nor SDL's offscreen driver */
        add(P_ABOUT, I_OPEN, "Open the data folder", NULL, NULL)->desc = "In the file manager";
    if (desktop >= 1)
        add(P_ABOUT, I_COPY, "Copy to the clipboard", NULL, NULL)->desc = "This page as text, for a bug report";
}

void pc_menu_set_about(void (*fill)(PcAbout *about))
{
    s_about_fill = fill;
}

size_t pc_menu_about_text(char *out, size_t size)
{
    size_t n = 0;
    if (!size)
        return 0;
    out[0] = 0;
    fill_about();
    n += (size_t)snprintf(out, size, "Rush 2049 PC\n");
    for (int i = 0; i < pages[P_ABOUT].n && n < size; i++) {
        const Item *it = &pages[P_ABOUT].item[i];
        if (it->kind == I_INFO)
            n += (size_t)snprintf(out + n, size - n, "%s: %s\n", about_rows[it->row].label, about_rows[it->row].a);
    }
    return n < size ? n : size - 1;
}

int pc_menu_tick(void)
{
    if (M.capture && pc_host_ns() >= M.capture_until)
        pc_menu_capture(NULL);
    if (M.open && M.page == P_ABOUT && about_players())   /* About's players follow the controllers live */
        M.dirty = 1;
    if (M.open && M.page == P_CONTROLLER) {   /* the player rows and auto accessories follow the controllers live */
        char rows[512] = "", l[64], v[32];
        for (int p = 0; p < 4; p++) {
            player_row(p, l, sizeof l, v, sizeof v);
            snprintf(rows + strlen(rows), sizeof rows - strlen(rows), "%s|%s|%d|", l, v, pc_si_pak(p));
        }
        if (strcmp(rows, M.players)) {
            snprintf(M.players, sizeof M.players, "%s", rows);
            M.dirty = 1;
        }
    }
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
#define LESS_X (VALUE_X - 8)                        /* the selected value's arrows, "<" and ">" */
#define MORE_X (VALUE_X + VALUE_CHARS * 6 + 1)
#define SCROLL_X (PANEL_W - 8)                      /* the list's "^" and "v" */
#define BAR_W 60
#define FOOT_CHARS 49                               /* a footer line */
#define WHEEL_ROWS 3                                /* rows a wheel notch or a scroll arrow moves */
#define DOUBLE_CLICK_NS 500000000u                  /* Windows' double-click time */

static int g_k, g_ox, g_oy;

/* The panel's place in a w x h window (pixels): the unit k and its top left corner. */
static void layout(int w, int h, int *k, int *ox, int *oy)
{
    *k = w / 320 < h / 240 ? w / 320 : h / 240;
    if (*k < 1)
        *k = 1;
    *ox = (w - PANEL_W * *k) / 2;
    *oy = (h - PANEL_H * *k) / 2;
}

static int has_value(const Item *it)
{
    return it->kind == I_CHOICE || it->kind == I_WINDOW || it->kind == I_NUMBER;
}

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
        else if (M.saved)
            snprintf(out, n, "saved");
        break;
    }
    case I_INFO:
    case I_HEADING: {
        const Row *r = &pages[M.page].rows[it->row];
        snprintf(out, n, "%s%s%s", r->a, pages[M.page].col_b ? " | " : "", pages[M.page].col_b ? r->b : "");
        break;
    }
    case I_OPEN:
    case I_COPY:
        out[0] = 0;
        break;
    default:
        snprintf(out, n, "%s", v);
        for (int i = 0; it->labels && it->choices[i]; i++)
            if (!strcasecmp(v, it->choices[i]))
                snprintf(out, n, "%s", it->labels[i]);
        if (it->labels == pak_labels && !strcasecmp(v, "auto"))   /* what auto is now */
            snprintf(out, n, "auto: %s", pc_si_pak(it->page) == PC_CFG_PAK_RUMBLE ? "rumble pak" : "none");
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
    case I_INFO:
        return "Up/Down: move   Esc/B: back";
    default:
        return M.page == P_MAIN ? "Enter/A: choose   Esc/B: close" : "Enter/A: choose   Esc/B: back";
    }
}

/* The footer's line about the selected item: what it does, or its whole text; the setting's name for one without
 * a line of its own. A long About value takes the line under it too (*wrap). */
static void description(const Item *it, char *out, size_t n, int *wrap)
{
    char v[PC_CFG_BIND + 8];
    *wrap = 0;
    out[0] = 0;
    if (it->kind == I_INFO) {
        const Row *r = &pages[M.page].rows[it->row];
        if (r->desc)
            snprintf(out, n, "%s", r->desc);
        else if (pages[M.page].col_b)
            snprintf(out, n, "Keys: %.*s   Controller: %s", PC_CFG_BIND + 16, r->a, r->b[0] ? r->b : "-");
        else {
            snprintf(out, n, "%s", r->a);
            *wrap = strlen(out) > FOOT_CHARS;
        }
        return;
    }
    if (it->kind == I_SAVE) {
        snprintf(out, n, "To %s", M.path[0] ? M.path : "nowhere: no settings file");
        fit(out, FOOT_CHARS, 1);
        return;
    }
    if (it->kind == I_BIND && !strcasecmp(it->section, "hotkeys") && it->desc) {
        snprintf(out, n, "%s", it->desc);
        return;
    }
    if (it->kind == I_BIND && strcasecmp(it->key, "menu")) {   /* the N64 control and the whole list */
        value_of(&M.eff, it, v, sizeof v);
        snprintf(out, n, "N64 %s: %s", it->label, v[0] ? v : "none");
        return;
    }
    if (it->desc)
        snprintf(out, n, "%s", it->desc);
    else if (it->section)
        snprintf(out, n, "%s.%s in rush2049pc.ini", it->section, it->key);
}

static void draw(int w, int h)
{
    const Page *p = &pages[M.page];
    const Item *sel = current();
    char line[PC_CFG_PATH + 64], v[PC_CFG_PATH + 64];
    int wrap = 0;
    layout(w, h, &g_k, &g_ox, &g_oy);
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
        if (it->kind == I_PLAYER) {
            player_row(it->page, line, sizeof line, v, sizeof v);
            fit(line, 26, 0);
            text(8, y + 2, C_HINT, line);
            text(VALUE_X, y + 2, C_HINT, v);
            continue;
        }
        if (it->kind == I_INFO || it->kind == I_HEADING) {   /* Help's and About's columns */
            const Row *r = &p->rows[it->row];
            int head = it->kind == I_HEADING, end_a = p->col_b ? p->col_b : PANEL_W - 4;
            uint32_t lc = head ? C_HINT : here ? C_WHITE : C_LABEL, vc = head ? C_HINT : here ? C_WHITE : C_VALUE;
            snprintf(line, sizeof line, "%s", r->label);
            fit(line, (size_t)(p->col_a - 12) / 6, 0);
            text(8, y + 2, lc, line);
            snprintf(v, sizeof v, "%s", r->a);
            fit(v, (size_t)(end_a - p->col_a - 4) / 6, r->path);
            text(p->col_a, y + 2, vc, v);
            if (p->col_b) {
                snprintf(v, sizeof v, "%s", r->b);
                fit(v, (size_t)(PANEL_W - 4 - p->col_b) / 6, 0);
                text(p->col_b, y + 2, vc, v);
            }
            continue;
        }
        snprintf(line, sizeof line, "%s%s", it->label, it->restart ? " *" : "");
        text(8, y + 2, here ? C_WHITE : C_LABEL, line);
        if (here && M.capture)
            snprintf(v, sizeof v, M.capture == PC_MENU_CAPTURE_KEY ? "press a key..." : "press a button...");
        else
            shown(it, v, sizeof v);
        if (it->kind == I_SAVE && !M.capture)   /* yellow while there is something to save, green once saved */
            color = unsaved() ? C_CHANGED : M.saved ? C_OK : color;
        if (it->kind == I_NUMBER && it->percent) {   /* a bar, then the number */
            double f = strtod(v, NULL) / 100;
            rect(VALUE_X, y + 4, BAR_W, 4, C_BAR);
            rect(VALUE_X, y + 4, (int)(BAR_W * f + 0.5), 4, color);
            text(VALUE_X + BAR_W + 6, y + 2, color, v);
        } else {
            fit(v, VALUE_CHARS, 0);
            text(VALUE_X, y + 2, color, v);
        }
        if (here && !M.capture && has_value(it)) {   /* arrows to click */
            text(LESS_X, y + 2, C_HINT, "<");
            text(MORE_X, y + 2, C_HINT, ">");
        }
    }
    if (M.top[M.page] > 0)
        text(SCROLL_X, LIST_Y + 2, C_HINT, "^");
    if (M.top[M.page] + ROWS < p->n)
        text(SCROLL_X, LIST_Y + (ROWS - 1) * ROW_H + 2, C_HINT, "v");

    /* the footer: a message or what the selected item is, then the keys */
    rect(4, LIST_Y + ROWS * ROW_H + 1, PANEL_W - 8, 1, C_EDGE);
    if (M.status[0]) {
        snprintf(line, sizeof line, "%s", M.status);
        fit(line, FOOT_CHARS, 0);
        text(6, PANEL_H - 22, M.status_color, line);
    } else if (sel) {
        description(sel, line, sizeof line, &wrap);
        if (sel->restart && !strstr(line, "restart"))
            snprintf(line, sizeof line, "* applies after a restart");
        if (wrap) {   /* a long value over both lines, its end kept */
            snprintf(v, sizeof v, "%s", line + FOOT_CHARS);
            fit(v, FOOT_CHARS, 1);
            line[FOOT_CHARS] = 0;
            text(6, PANEL_H - 11, C_HINT, v);
        }
        fit(line, FOOT_CHARS, 0);
        text(6, PANEL_H - 22, C_HINT, line);
    }
    if (sel && !(wrap && !M.status[0]))
        text(6, PANEL_H - 11, C_HINT, hint(sel));
}

/* ---------------------------------------------------------------- the mouse */

void pc_menu_mouse_pixels(int x, int y, int win_w, int win_h, int draw_w, int draw_h, int *px, int *py)
{
    if (win_w <= 0 || win_h <= 0 || draw_w <= 0 || draw_h <= 0) {
        *px = x, *py = y;
        return;
    }
    /* the middle of the window unit, in the drawable's pixels: the same pixel at 1:1, the right one at 2x */
    *px = (int)floor((x + 0.5) * draw_w / win_w);
    *py = (int)floor((y + 0.5) * draw_h / win_h);
}

int pc_menu_hit(int w, int h, int x, int y, PcMenuHit *hit)
{
    const Page *p = &pages[M.page];
    int k, ox, oy, top = M.top[M.page];
    layout(w, h, &k, &ox, &oy);
    *hit = (PcMenuHit){PC_HIT_OUTSIDE, -1, PC_PART_LABEL, 0};
    if (x < ox || y < oy || x >= ox + PANEL_W * k || y >= oy + PANEL_H * k)
        return hit->area;
    int ux = (x - ox) / k, uy = (y - oy) / k, i = (uy - LIST_Y) / ROW_H;
    hit->area = PC_HIT_PANEL;
    if (uy < LIST_Y || i >= ROWS)
        return hit->area;
    if (ux >= SCROLL_X - 2 && i == 0 && top > 0)
        return hit->area = PC_HIT_SCROLL_UP;
    if (ux >= SCROLL_X - 2 && i == ROWS - 1 && top + ROWS < p->n)
        return hit->area = PC_HIT_SCROLL_DOWN;
    if (top + i >= p->n)
        return hit->area;
    hit->area = PC_HIT_ITEM;
    hit->item = top + i;
    const Item *it = &p->item[top + i];
    if (!has_value(it) || ux < LESS_X - 2)
        return hit->area;
    if (it->kind == I_NUMBER && it->percent) {   /* the bar sets the value where it is clicked */
        int bx = x - ox - VALUE_X * k;
        if (bx < 0)
            hit->part = PC_PART_LESS;
        else if (bx <= BAR_W * k) {
            hit->part = PC_PART_BAR;
            hit->bar = (int)((bx * 1000LL + BAR_W * k / 2) / (BAR_W * k));
        } else
            hit->part = PC_PART_MORE;
    } else   /* left of the value's middle the previous one, right of it the next */
        hit->part = ux < (LESS_X + MORE_X + 6) / 2 ? PC_PART_LESS : PC_PART_MORE;
    return hit->area;
}

/* Pointing at an item selects it (not while a binding waits: the key goes to the item chosen). */
static void hover(const PcMenuHit *hit)
{
    const Page *p = &pages[M.page];
    if (M.capture || hit->area != PC_HIT_ITEM || !selectable(&p->item[hit->item]) || M.sel[M.page] == hit->item)
        return;
    M.sel[M.page] = hit->item;
    M.dirty = 1;
    if (M.status[0] && pc_host_ns() - M.status_ns > 1500000000u)   /* an old message gives way to the item's line */
        M.status[0] = 0;
}

static void set_bar(const Item *it, int permille)
{
    char v[32];
    double x = it->min + (it->max - it->min) * permille / 1000.0;
    x = floor(x / it->step + 0.5) * it->step;
    x = x < it->min ? it->min : x > it->max ? it->max : x;
    snprintf(v, sizeof v, it->step >= 1 ? "%.0f" : "%.2f", x);
    set_value(it, v);
}

void pc_menu_mouse(int what, int x, int y, int w, int h, int amount)
{
    PcMenuHit hit;
    if (!M.inited || !M.open)
        return;
    if (what != PC_MOUSE_WHEEL)
        M.mouse = 1, M.mx = x, M.my = y, M.mw = w, M.mh = h;
    switch (what) {
    case PC_MOUSE_MOVE:
        pc_menu_hit(w, h, x, y, &hit);
        hover(&hit);
        break;
    case PC_MOUSE_RIGHT:   /* back, or cancel a capture */
        pc_menu_action(PC_MENU_BACK);
        break;
    case PC_MOUSE_LEFT: {
        uint64_t now = pc_host_ns();
        int page = M.page;
        if (M.capture) {   /* a click cancels a capture */
            pc_menu_capture(NULL);
            break;
        }
        /* a double click's second press, on the page the first one opened (or went back to), is no click there */
        if (M.page_click_ns && now >= M.page_click_ns && now - M.page_click_ns < DOUBLE_CLICK_NS)
            break;
        pc_menu_hit(w, h, x, y, &hit);
        if (hit.area == PC_HIT_OUTSIDE)
            pc_menu_action(PC_MENU_BACK);
        else if (hit.area == PC_HIT_SCROLL_UP || hit.area == PC_HIT_SCROLL_DOWN)
            scroll_by(hit.area == PC_HIT_SCROLL_UP ? -WHEEL_ROWS : WHEEL_ROWS);
        else if (hit.area == PC_HIT_ITEM && selectable(&pages[M.page].item[hit.item])) {
            hover(&hit);
            M.sel[M.page] = hit.item;
            if (hit.part == PC_PART_LESS || hit.part == PC_PART_MORE)
                pc_menu_action(hit.part == PC_PART_LESS ? PC_MENU_LEFT : PC_MENU_RIGHT);
            else if (hit.part == PC_PART_BAR) {
                M.status[0] = 0;
                set_bar(current(), hit.bar);
                M.dirty = 1;
            } else
                pc_menu_action(PC_MENU_ACCEPT);
        }
        if (M.page != page)
            M.page_click_ns = now ? now : 1;
        break;
    }
    case PC_MOUSE_WHEEL:   /* scrolls a long page; the item under the mouse is then the selected one */
        if (M.capture || !amount)
            break;
        if (scroll_by(-amount * WHEEL_ROWS) && M.mouse) {
            pc_menu_hit(M.mw, M.mh, M.mx, M.my, &hit);
            hover(&hit);
        }
        break;
    }
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

const char *pc_menu_message(void)
{
    return M.status;
}
