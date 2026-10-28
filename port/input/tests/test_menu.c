/* port/input/tests/test_menu.c: the settings overlay, headless (docs/port/FRONTEND.md "The settings overlay").
 *
 *   frontend_menu_test nav|live|save|script SCRATCH_DIR
 *
 * port/gfx renders headless (EGL, or WGL's hidden window): its window is an offscreen image that
 * pc_gfx_read_window reads. port/input runs as rush2049pc runs it, without a window: the overlay gets its input as
 * actions (what input_sdl.c sends for keys and buttons) and from --press scripts, and pc_gfx_poll_events gives it
 * its turn as the main loop does.
 *   nav     opening holds the game and empties its pads; moving, wrapping, pages, scrolling a long page, back
 *           and close; Quit asks the loop to stop; the overlay is drawn (a panel over the frame) and gone again
 *   live    widescreen and the scale change the next frame's render target; the window size, the volume; a
 *           binding captured; the restart-only settings change only the settings
 *   save    pc_ini_update on its own (values replaced in place, comments and spacing kept, new keys and sections,
 *           CRLF, quotes, the last of duplicates); then the overlay's Save over a hand-written file, which keeps
 *           every other line; nothing to save; a file that doesn't exist yet is written whole
 *   script  MENU and the script's buttons drive the overlay over time; the game's pads stay idle meanwhile, and
 *           scripted game presses work again after it closes
 * pictures (not a CTest) writes each page as a PNG into SCRATCH_DIR, at 1280 x 960, to look at. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "input_internal.h"
#include "pc_compat.h"
#include "test_gbi.h"

int pc_input_init(const PcSettings *s);
void pc_input_shutdown(void);
int pc_input_paused(void);
int pc_input_add_spec(const char *spec);
int pc_gfx_read_framebuffer(const void *cimg, uint8_t *rgba, int *w, int *h);
int pc_gfx_read_window(uint8_t *rgba, int *w, int *h);
extern uint64_t test_now_ns;
extern double test_volume;

static int g_fail, g_checks;
static const char *g_dir;

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
    gsDPSetFillColor(0x07C107C1u);   /* green */
    gsDPFillRectangle(0, 0, 319, 239);
    gsDPFullSync();
    gsSPEndDisplayList();
    pc_gfx_run_dl(s_list);
    pc_gfx_present(s_fb);
}

static int start(PcSettings *s)
{
    if (pc_gfx_init(1, 640, 480) != 0) {
        fprintf(stderr, "FAIL: no renderer\n");
        return -1;
    }
    pc_input_init(s);   /* no window: -1, scripted input and the overlay only */
    return 0;
}

static void stop(void)
{
    pc_input_shutdown();
    pc_gfx_shutdown();
}

static int pump(void)
{
    return pc_gfx_poll_events();
}

static void act(int action)
{
    pc_menu_action(action);
    pump();
}

static void act_n(int action, int n)
{
    while (n--)
        act(action);
}

/* The overlay is on page `page`, with `item` selected; `value` what it shows (NULL: anything). */
static void at(const char *page, const char *item, const char *value, const char *what)
{
    const char *p, *i;
    char v[256];
    pc_menu_state(&p, &i, v, sizeof v);
    CHECK(!strcmp(p, page) && !strcmp(i, item) && (!value || !strcmp(v, value)),
          "%s: on '%s' / '%s' = '%s'; expected '%s' / '%s' = '%s'", what, p, i, v, page, item, value ? value : "*");
}

static void target_size(int *w, int *h)
{
    pc_gfx_read_framebuffer(s_fb, NULL, w, h);
}

static char *read_text(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    static char buf[65536];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    return buf;
}

static void write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL, "cannot write %s", path);
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

/* ---------------------------------------------------------------- nav */

static void test_nav(void)
{
    PcSettings s;
    PcPad pads[4];
    int w, h;
    pc_settings_defaults(&s);
    if (start(&s) != 0) {
        g_fail++;
        return;
    }
    show_frame();
    uint8_t *before = malloc(640 * 480 * 4), *open = malloc(640 * 480 * 4), *after = malloc(640 * 480 * 4);
    CHECK(pc_gfx_read_window(before, &w, &h) == 0 && w == 640 && h == 480, "window %dx%d", w, h);
    CHECK(!pc_menu_is_open() && !pc_input_paused(), "closed at start");

    act(PC_MENU_TOGGLE);
    CHECK(pc_menu_is_open() && pc_input_paused(), "open: the game is held");
    at("Settings", "Video", ">", "first item");
    pc_input_poll(pads);
    CHECK(pads[0].buttons == 0 && pads[0].stick_x == 0 && pads[0].connected, "the game's pads are idle");
    pc_gfx_read_window(open, &w, &h);
    /* the panel over the frame: its middle is the panel's color blended over green, its corner the frame dimmed */
    const uint8_t *mid = open + ((size_t)400 * 640 + 320) * 4, *corner = open + 4 * 4;
    CHECK(mid[0] < 40 && mid[1] < 60 && mid[2] < 40 && memcmp(open, before, 640 * 480 * 4),
          "the panel shows: %d,%d,%d", mid[0], mid[1], mid[2]);
    CHECK(corner[1] > 100 && corner[1] < before[4 * 4 + 1], "outside the panel the frame is dimmed: %d (was %d)",
          corner[1], before[4 * 4 + 1]);
    /* the selected row is highlighted: the panel (at 16, 16) in units of 2 pixels, its first row x 2-302, y 18-30;
     * the label starts at x 8, its text at y 20 */
    const uint8_t *row = open + ((size_t)(16 + 2 * 19) * 640 + 16 + 2 * 3) * 4;
    CHECK(row[0] == 0x2E && row[1] == 0x5A && row[2] == 0xA0, "highlight %02x%02x%02x", row[0], row[1], row[2]);

    act(PC_MENU_DOWN);
    at("Settings", "Audio", NULL, "down");
    act_n(PC_MENU_DOWN, 2);
    at("Settings", "Controllers", NULL, "down to Controllers");
    act_n(PC_MENU_DOWN, 4);
    at("Settings", "Video", NULL, "wrapped past Quit");
    act(PC_MENU_UP);
    at("Settings", "Quit", NULL, "up from the top wraps");
    act(PC_MENU_DOWN);
    act(PC_MENU_ACCEPT);
    at("Video", "Render scale", "native", "Video page");
    act(PC_MENU_BACK);
    at("Settings", "Video", NULL, "back to the main page, on Video");
    act_n(PC_MENU_DOWN, 2);
    act(PC_MENU_ACCEPT);
    at("Keyboard", "A", "X", "Keyboard page");
    act(PC_MENU_PAGE_DOWN);
    at("Keyboard", "Stick down", "Down", "a page down: 15 items on");
    act(PC_MENU_PAGE_DOWN);
    at("Keyboard", "Defaults", NULL, "the last item");
    act(PC_MENU_DOWN);
    at("Keyboard", "A", NULL, "wraps to the first");
    act(PC_MENU_PAGE_UP);
    at("Keyboard", "A", NULL, "a page up stops at the first");
    act(PC_MENU_BACK);
    act(PC_MENU_BACK);
    CHECK(!pc_menu_is_open() && !pc_input_paused(), "back from the main page closes");
    pc_gfx_read_window(after, &w, &h);
    CHECK(!memcmp(after, before, 640 * 480 * 4), "closed: the window is the frame again, byte for byte");
    show_frame();
    pc_gfx_read_window(after, &w, &h);
    CHECK(!memcmp(after, before, 640 * 480 * 4), "and the next frame is unchanged");

    act(PC_MENU_TOGGLE);
    at("Settings", "Keyboard", NULL, "reopened where it was");
    act(PC_MENU_CLOSE);
    CHECK(!pc_menu_is_open(), "close");
    act(PC_MENU_TOGGLE);
    act_n(PC_MENU_DOWN, 4);
    at("Settings", "Quit", NULL, "Quit");
    pc_menu_action(PC_MENU_ACCEPT);
    CHECK(pump() != 0, "Quit stops the main loop");
    free(before);
    free(open);
    free(after);
    stop();
}

/* ---------------------------------------------------------------- live */

static void test_live(void)
{
    PcSettings s;
    int w, h;
    pc_settings_defaults(&s);
    if (start(&s) != 0) {
        g_fail++;
        return;
    }
    show_frame();
    target_size(&w, &h);
    CHECK(w == 320 && h == 240, "4:3 at 1x: %dx%d", w, h);
    act(PC_MENU_TOGGLE);
    act(PC_MENU_ACCEPT);
    at("Video", "Render scale", "native", "the first item: the render scale");
    act(PC_MENU_DOWN);
    at("Video", "Widescreen", "auto", "widescreen auto");
    act_n(PC_MENU_RIGHT, 3);
    at("Video", "Widescreen", "16:9", "widescreen 16:9");
    act(PC_MENU_CLOSE);
    show_frame();
    target_size(&w, &h);
    CHECK(w == 426 && h == 240, "16:9: the next frame's target is %dx%d", w, h);

    act(PC_MENU_TOGGLE);
    act(PC_MENU_ACCEPT);
    act(PC_MENU_UP);
    act_n(PC_MENU_RIGHT, 2);
    at("Video", "Render scale", "2x", "scale 2");
    act(PC_MENU_LEFT);
    at("Video", "Render scale", "N64 (1x)", "scale 1: the N64's");
    act(PC_MENU_LEFT);
    at("Video", "Render scale", "native", "scale native");
    act(PC_MENU_CLOSE);
    show_frame();   /* native in the 640 x 480 window: 426 x 240 at 640 / 426 */
    target_size(&w, &h);
    CHECK(w == 640 && h == 361, "native, 16:9: %dx%d", w, h);
    act(PC_MENU_TOGGLE);
    act(PC_MENU_ACCEPT);
    act(PC_MENU_LEFT);
    at("Video", "Render scale", "auto (whole)", "scale auto (before native)");
    act_n(PC_MENU_RIGHT, 3);
    at("Video", "Render scale", "2x", "scale 2 again");
    act_n(PC_MENU_DOWN, 3);
    at("Video", "Fullscreen mode", "desktop", "fullscreen mode");
    act(PC_MENU_RIGHT);
    CHECK(pc_menu_settings()->fullscreen_mode == 1, "exclusive");
    act(PC_MENU_LEFT);
    act(PC_MENU_DOWN);
    at("Video", "Resolution", "desktop", "resolution");
    act_n(PC_MENU_RIGHT, 4);
    at("Video", "Resolution", "1620p (2880x1620)", "resolution 1620p");
    CHECK(!strcmp(pc_menu_settings()->resolution, "1620p"), "the setting: %s", pc_menu_settings()->resolution);
    act_n(PC_MENU_LEFT, 4);
    act_n(PC_MENU_DOWN, 3);
    at("Video", "Frame rate", "original", "frame rate");
    act(PC_MENU_RIGHT);
    at("Video", "Frame rate", "60", "frame rate 60");
    CHECK(pc_gfx_frame_rate() == 60, "the renderer's frame rate: %d", pc_gfx_frame_rate());
    act(PC_MENU_LEFT);
    CHECK(pc_gfx_frame_rate() == 0, "the game's own again: %d", pc_gfx_frame_rate());
    act_n(PC_MENU_UP, 2);
    at("Video", "Window size", "auto", "window size");
    act(PC_MENU_RIGHT);
    at("Video", "Window size", "640x480", "the first size after auto");
    act(PC_MENU_RIGHT);
    at("Video", "Window size", "960x720", "a bigger window");
    pc_gfx_read_window(NULL, &w, &h);
    CHECK(w == 960 && h == 720, "the window follows: %dx%d", w, h);
    act_n(PC_MENU_DOWN, 4);
    act(PC_MENU_RIGHT);
    at("Video", "Graphics API", "gl", "the API: a restart");
    CHECK(pc_menu_settings()->api == PC_CFG_API_GL, "the setting changed");
    act(PC_MENU_CLOSE);
    show_frame();
    target_size(&w, &h);
    CHECK(w == 852 && h == 480, "2x, 16:9: %dx%d (the API unchanged until a restart)", w, h);

    act(PC_MENU_TOGGLE);
    act(PC_MENU_DOWN);
    act(PC_MENU_ACCEPT);
    at("Audio", "Volume", "100 %", "volume");
    act_n(PC_MENU_LEFT, 10);
    at("Audio", "Volume", "50 %", "volume 50");
    CHECK(test_volume == 0.5, "port/audio's volume: %g", test_volume);
    act(PC_MENU_DOWN);
    act(PC_MENU_RIGHT);
    at("Audio", "Output", "sdl", "the backend: a restart");
    act(PC_MENU_BACK);

    /* a binding: Enter waits for a key, which replaces it; Insert adds one; Delete clears */
    act(PC_MENU_DOWN);
    act(PC_MENU_ACCEPT);
    at("Keyboard", "A", "X", "keyboard A");
    act(PC_MENU_ACCEPT);
    CHECK(pc_menu_capturing() == PC_MENU_CAPTURE_KEY, "waiting for a key");
    act(PC_MENU_DOWN);
    at("Keyboard", "A", NULL, "moving keys don't act while it waits");
    pc_menu_capture("Space");
    pump();
    at("Keyboard", "A", "Space", "captured");
    CHECK(!strcmp(pc_menu_settings()->key[PC_CTL_A], "Space"), "the setting");
    act(PC_MENU_ADD);
    pc_menu_capture("Left Shift");
    at("Keyboard", "A", "Space, Left Shift", "added");
    act(PC_MENU_ACCEPT);
    act(PC_MENU_BACK);
    at("Keyboard", "A", "Space, Left Shift", "cancelled");
    act(PC_MENU_CLEAR);
    at("Keyboard", "A", "-", "cleared");
    act(PC_MENU_BACK);
    act(PC_MENU_DOWN);
    act(PC_MENU_ACCEPT);
    at("Controllers (all players)", "A", "a, righttrigger", "controller A");
    act(PC_MENU_ACCEPT);
    CHECK(pc_menu_capturing() == PC_MENU_CAPTURE_PAD, "waiting for a controller input");
    pc_menu_capture("+leftx");
    at("Controllers (all players)", "A", "+leftx", "a stick direction");
    /* a capture gives up after 6 s */
    act(PC_MENU_ACCEPT);
    test_now_ns += 7000000000u;
    pump();
    CHECK(!pc_menu_capturing(), "timed out");
    at("Controllers (all players)", "A", "+leftx", "unchanged");
    act(PC_MENU_CLOSE);
    stop();
}

/* ---------------------------------------------------------------- save */

static void update(const char *text, const PcSettingValue *ch, int n, const char *want, const char *what)
{
    char *got = pc_ini_update(text, ch, n);
    CHECK(!strcmp(got, want), "%s:\n--- got ---\n%s--- expected ---\n%s", what, got, want);
    free(got);
}

static void test_save(void)
{
    /* in place: the value's text replaced; spacing, case and an inline comment kept */
    PcSettingValue scale = {"video", "scale", "3"}, wide = {"video", "widescreen", "16:9"};
    update("; top\n[Video]  ; v\n  Scale   =  1   ; one\nwidth=640\n", &scale, 1,
           "; top\n[Video]  ; v\n  Scale   =  3   ; one\nwidth=640\n", "replaced in place");
    update("[video]\nscale = 1\nscale = 2\n", &scale, 1, "[video]\nscale = 1\nscale = 3\n", "the last one counts");
    update("[video]\nwidth = 640\n; about audio\n[audio]\nbackend = null\n", &wide, 1,
           "[video]\nwidth = 640\nwidescreen = 16:9\n; about audio\n[audio]\nbackend = null\n",
           "a new key after its section's last");
    update("[video]\n", &wide, 1, "[video]\nwidescreen = 16:9\n", "an empty section");
    update("[audio]\nbackend = null", &wide, 1, "[audio]\nbackend = null\n\n[video]\nwidescreen = 16:9\n",
           "a new section, after a last line without its end");
    update("[video]\r\nscale = 1\r\n", (PcSettingValue[]){scale, wide}, 2,
           "[video]\r\nscale = 3\r\nwidescreen = 16:9\r\n", "CRLF kept");
    update("[keyboard]\nA = X\n", (PcSettingValue[]){{"keyboard", "a", "Space, Comma"}}, 1,
           "[keyboard]\nA = Space, Comma\n", "a binding");
    update("[video]\nscreenshot_dir = \"old\" ; q\n",
           (PcSettingValue[]){{"video", "screenshot_dir", " a dir ; x"}}, 1,
           "[video]\nscreenshot_dir = \" a dir ; x\" ; q\n", "quoted, the old quotes replaced");
    update("\xEF\xBB\xBF[video]\nbad line\n[broken\nscale = 1\n", &scale, 1,
           "\xEF\xBB\xBF[video]\nbad line\n[broken\nscale = 3\n", "a BOM and bad lines kept");
    update("[video]\nscale = 1\n", NULL, 0, "[video]\nscale = 1\n", "no changes");

    /* the overlay's Save over a hand-written file */
    char path[PC_CFG_PATH - 64], *text;
    PcSettings s;
    snprintf(path, sizeof path, "%s/menu.ini", g_dir);
    const char *original = "; my settings\n"
                           "[game]\n"
                           "rom = /roms/rush.z64   ; the ROM\n"
                           "\n"
                           "[video]\n"
                           "width = 800\n"
                           "height = 600\n"
                           "vsync = yes   # spelled my way\n"
                           "frobnicate = 7\n"
                           "\n"
                           "[keyboard]\n"
                           "B = C\n";
    write_text(path, original);
    PcSettingValue cli = {"video", "scale", "4"};   /* from the command line: not saved unless changed here */
    pc_settings_resolve(&s, PC_CFG_FILE_READ, path, NULL, &cli, 1);
    CHECK(!strcmp(s.save_path, path) && s.scale == 4 && s.width == 800, "resolved: %s %d", s.save_path, s.scale);
    if (start(&s) != 0) {
        g_fail++;
        return;
    }
    act(PC_MENU_TOGGLE);
    act(PC_MENU_ACCEPT);
    act(PC_MENU_DOWN);
    act_n(PC_MENU_RIGHT, 3);   /* widescreen 16:9 */
    act_n(PC_MENU_DOWN, 5);
    act(PC_MENU_RIGHT);        /* vsync off, then on again: the same as the file, so not written */
    act(PC_MENU_RIGHT);
    act(PC_MENU_UP);           /* the window size */
    act(PC_MENU_RIGHT);        /* 800x600 isn't a choice: from the start of the list, 640x480 */
    at("Video", "Window size", "640x480", "window size");
    act(PC_MENU_BACK);
    act_n(PC_MENU_DOWN, 2);
    act(PC_MENU_ACCEPT);
    act(PC_MENU_ACCEPT);
    pc_menu_capture("Space");  /* keyboard A, which the file lacks */
    act(PC_MENU_BACK);
    act_n(PC_MENU_DOWN, 2);
    at("Settings", "Save to rush2049pc.ini", "4 changes", "four to save");
    act(PC_MENU_ACCEPT);
    at("Settings", "Save to rush2049pc.ini", "", "saved");
    text = read_text(path);
    const char *want = "; my settings\n"
                       "[game]\n"
                       "rom = /roms/rush.z64   ; the ROM\n"
                       "\n"
                       "[video]\n"
                       "width = 640\n"
                       "height = 480\n"
                       "vsync = yes   # spelled my way\n"
                       "frobnicate = 7\n"
                       "widescreen = 16:9\n"
                       "\n"
                       "[keyboard]\n"
                       "B = C\n"
                       "A = Space\n";
    CHECK(text && !strcmp(text, want), "saved file:\n%s", text ? text : "(none)");
    act(PC_MENU_ACCEPT);
    CHECK(text && !strcmp(read_text(path), want), "nothing more to save: unchanged");
    PcSettings back;
    pc_settings_defaults(&back);
    CHECK(pc_settings_load_file(&back, path) == 1, "reads back with its one unknown line");
    CHECK(back.widescreen > 1.7 && back.width == 640 && back.vsync && back.scale == -1 &&
              !strcmp(back.key[PC_CTL_A], "Space") && !strcmp(back.rom, "/roms/rush.z64"),
          "the saved values; the command line's scale not among them");
    act(PC_MENU_CLOSE);
    stop();

    /* no file yet (its directory either): a whole one, the defaults with the change */
    snprintf(path, sizeof path, "%s/new/sub/rush2049pc.ini", g_dir);
    unlink(path);
    pc_settings_defaults(&s);
    snprintf(s.save_path, sizeof s.save_path, "%s", path);
    if (start(&s) != 0) {
        g_fail++;
        return;
    }
    act(PC_MENU_TOGGLE);
    act(PC_MENU_DOWN);
    act(PC_MENU_ACCEPT);
    act(PC_MENU_LEFT);
    act(PC_MENU_BACK);
    act_n(PC_MENU_DOWN, 3);
    act(PC_MENU_ACCEPT);
    pc_settings_defaults(&back);
    back.volume = 95;
    char *full = pc_settings_format(&back);
    text = read_text(path);
    CHECK(text && !strcmp(text, full), "a new file, whole:\n%s", text ? text : "(none)");
    free(full);
    stop();
}

/* ---------------------------------------------------------------- script */

static void at_time(double t)
{
    test_now_ns = (uint64_t)(t * 1e9);
    pump();
}

static void test_script(void)
{
    PcSettings s;
    PcPad pads[4];
    pc_settings_defaults(&s);
    CHECK(pc_input_add_spec("0.9:A:0.5,1:MENU,1.5:DOWN,2:A,2.5:LEFT,3:B,3.5:STICKUP,4:START,5:A") == 0, "script");
    CHECK(pc_input_add_spec("1:MENUS") != 0, "a bad name");
    if (start(&s) != 0) {
        g_fail++;
        return;
    }
    show_frame();
    at_time(0.95);
    pc_input_poll(pads);
    CHECK(pads[0].buttons == 0x8000 && !pc_menu_is_open(), "the game's A before the overlay");
    at_time(1.05);
    CHECK(pc_menu_is_open() && pc_input_paused(), "MENU opens it");
    at("Settings", "Video", NULL, "A held across the opening didn't act");
    pc_input_poll(pads);
    CHECK(pads[0].buttons == 0, "the game's pads are idle while it is open: %04x", pads[0].buttons);
    at_time(1.55);
    at("Settings", "Audio", NULL, "DOWN");
    at_time(2.05);
    at("Audio", "Volume", "100 %", "A opens Audio");
    at_time(2.55);
    at("Audio", "Volume", "95 %", "LEFT");
    CHECK(test_volume > 0.949 && test_volume < 0.951, "volume %g", test_volume);
    at_time(3.05);
    at("Settings", "Audio", NULL, "B goes back");
    at_time(3.55);
    at("Settings", "Video", NULL, "the stick moves too");
    at_time(4.05);
    CHECK(!pc_menu_is_open() && !pc_input_paused(), "START closes");
    at_time(5.05);
    pc_input_poll(pads);
    CHECK(pads[0].buttons == 0x8000 && !pc_menu_is_open(), "scripted game input after it closed");
    stop();
}

/* ---------------------------------------------------------------- pictures (not a test) */

int pcgfx_png_write(const char *path, const uint8_t *rgba, int w, int h);   /* port/gfx/gfx_png.c */

/* Each page as it looks over a frame, as PNGs in the scratch directory: for looking at, not checked. */
static void pictures(void)
{
    static const struct { const char *name; int down; } shots[] = {
        {"main", 0}, {"video", 0}, {"audio", 1}, {"keyboard", 2}, {"controllers", 3}};
    static uint8_t px[1280 * 960 * 4];
    PcSettings s;
    char path[PC_CFG_PATH];
    int w, h;
    pc_settings_defaults(&s);
    if (start(&s) != 0)
        return;
    pc_gfx_set_window_size(1280, 960);
    show_frame();
    for (size_t i = 0; i < sizeof shots / sizeof *shots; i++) {
        act(PC_MENU_TOGGLE);
        act_n(PC_MENU_DOWN, shots[i].down);
        if (i)
            act(PC_MENU_ACCEPT);
        if (i == 3) {   /* a capture in progress, and the list scrolled */
            act_n(PC_MENU_DOWN, 17);
            act(PC_MENU_ACCEPT);
        }
        pc_gfx_read_window(px, &w, &h);
        snprintf(path, sizeof path, "%s/menu-%s.png", g_dir, shots[i].name);
        pcgfx_png_write(path, px, w, h);
        printf("%s\n", path);
        if (i == 3)
            pc_menu_capture(NULL);
        if (i)
            act(PC_MENU_BACK);
        act_n(PC_MENU_UP, shots[i].down);
        act(PC_MENU_CLOSE);
    }
    stop();
}

int main(int argc, char **argv)
{
    static const struct { const char *name; void (*fn)(void); } cases[] = {
        {"nav", test_nav}, {"live", test_live}, {"save", test_save}, {"script", test_script},
        {"pictures", pictures}};
    if (argc < 3) {
        fprintf(stderr, "usage: frontend_menu_test nav|live|save|script|pictures SCRATCH_DIR\n");
        return 2;
    }
    g_dir = argv[2];
    mkdir(g_dir, 0755);
    int ran = 0;
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++)
        if (!strcmp(argv[1], cases[i].name)) {
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
