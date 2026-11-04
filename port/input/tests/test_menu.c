/* port/input/tests/test_menu.c: the settings overlay, headless (docs/port/FRONTEND.md "The settings overlay").
 *
 *   frontend_menu_test nav|live|save|script|mouse|pages SCRATCH_DIR
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
 *   mouse   hit-testing at ten window and drawable sizes (DPI scales of 1, 1.25, 1.5 and 2, widescreen, the
 *           smallest window): window units to pixels, rows, the panel's edges, a value's sides, the scroll arrow;
 *           then the overlay driven by the mouse: pointing, clicks on values, labels and the volume's bar, the
 *           wheel, the scroll arrow, a binding, the right button and outside; the highlight drawn where it points;
 *           nothing when closed
 *   pages   About's content from main's facts, the GL driver, the players and the files, made each time it opens;
 *           Help from the bindings in effect
 * pictures (not a CTest) writes each page as a PNG into SCRATCH_DIR, at 1280 x 960, to look at. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "pc_game_fps.h"
#include "input_internal.h"
#include "pc_compat.h"
#include "pc_game_types.h"   /* pc_race_cars, its latch (port/config/race_cars.c) */
#include "test_gbi.h"

int pc_input_init(const PcSettings *s);
void pc_input_shutdown(void);
int pc_input_paused(void);
int pc_input_add_spec(const char *spec);
int pc_gfx_read_framebuffer(const void *cimg, uint8_t *rgba, int *w, int *h);
int pc_gfx_read_window(uint8_t *rgba, int *w, int *h);
extern uint64_t test_now_ns;
extern double test_volume;
extern int test_pak[4], test_pak_sets;   /* tests/stubs.c: each pc_si_set_pak */

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
    act(PC_MENU_DOWN);
    at("Settings", "Game", NULL, "the Game page");
    act(PC_MENU_DOWN);
    at("Settings", "Mods", NULL, "the Mods page");
    act(PC_MENU_DOWN);
    at("Settings", "Saves", NULL, "the Saves page after it, the last settings' page (docs/port/SAVES.md)");
    /* Help and About, after the settings' pages: read-only, a heading stepped over */
    act(PC_MENU_DOWN);
    at("Settings", "Help", NULL, "Help after the settings' pages");
    act(PC_MENU_ACCEPT);
    at("Help", "A", "X | A, RT", "Help opens on A: its heading row is stepped over");
    act(PC_MENU_RIGHT);
    act(PC_MENU_CLEAR);
    at("Help", "A", "X | A, RT", "Help's rows don't change");
    act(PC_MENU_UP);
    at("Help", "Mouse", NULL, "up from the first wraps to the last, past the heading");
    act(PC_MENU_BACK);
    act(PC_MENU_DOWN);
    at("Settings", "About", NULL, "About");
    act(PC_MENU_ACCEPT);
    at("About", "Version", "unknown", "About, with nothing from main");
    act(PC_MENU_BACK);
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
    act_n(PC_MENU_DOWN, 9);
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
    act(PC_MENU_DOWN);
    at("Video", "Textures", NULL, "textures");
    act(PC_MENU_DOWN);
    at("Video", "Race HUD (widescreen)", "screen edges", "the race HUD: the last item");
    act(PC_MENU_RIGHT);
    at("Video", "Race HUD (widescreen)", "centered (4:3)", "the race HUD centered");
    CHECK(pc_gfx_hud() == PC_GFX_HUD_CENTERED && pc_menu_settings()->hud == 0, "the renderer has it at once: %d",
          pc_gfx_hud());
    act(PC_MENU_LEFT);
    CHECK(pc_gfx_hud() == PC_GFX_HUD_EDGES, "edges again: %d", pc_gfx_hud());
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
    /* the voices (audio.voices) apply after a restart: the setting changes, MusyX's count doesn't; the drones'
     * engines (audio.engines) at once (docs/port/AUDIO.md 10) */
    act(PC_MENU_DOWN);
    at("Audio", "Voices", "original (32)", "the voices: original");
    act(PC_MENU_RIGHT);
    act(PC_MENU_RIGHT);
    at("Audio", "Voices", "64", "64 voices");
    CHECK(pc_settings_voices(pc_menu_settings()) == 64 && pc_mx_voices() == 32,
          "the setting 64, the library's voices kept until a restart: %d", pc_mx_voices());
    act(PC_MENU_DOWN);
    at("Audio", "Drones' engines", "original", "the engines: original");
    act_n(PC_MENU_RIGHT, 3);
    at("Audio", "Drones' engines", "nearest 4", "nearest 4");
    CHECK(pc_engines() == 4, "at once: %d", pc_engines());
    act(PC_MENU_LEFT);
    act(PC_MENU_LEFT);
    act(PC_MENU_LEFT);
    act(PC_MENU_LEFT);
    at("Audio", "Drones' engines", "all", "before original: all");
    CHECK(pc_engines() == PC_ENGINES_ALL, "all: %d", pc_engines());
    act(PC_MENU_RIGHT);
    CHECK(pc_engines() == PC_ENGINES_ORIGINAL, "original again: %d", pc_engines());
    act(PC_MENU_UP);
    act_n(PC_MENU_LEFT, 2);
    at("Audio", "Voices", "original (32)", "the voices back to original");
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
    /* each player's accessory, swapped at once (port/ultra's pc_si_set_pak): player 2's from auto */
    act(PC_MENU_UP);   /* round to the last item, Defaults */
    act_n(PC_MENU_UP, 3);
    at("Controllers (all players)", "P2 accessory", "auto: none", "player 2's accessory: auto, no rumble now");
    int sets = test_pak_sets;
    act(PC_MENU_LEFT);
    at("Controllers (all players)", "P2 accessory", "rumble pak", "player 2's rumble pak");
    CHECK(test_pak[1] == PC_CFG_PAK_RUMBLE && test_pak_sets == sets + 1, "swapped at once: %d", test_pak[1]);
    act(PC_MENU_LEFT);
    at("Controllers (all players)", "P2 accessory", "controller pak", "player 2's controller pak");
    CHECK(test_pak[1] == PC_CFG_PAK_MEMORY && pc_menu_settings()->pak[1] == PC_CFG_PAK_MEMORY &&
              pc_menu_settings()->pak[0] == PC_CFG_PAK_MEMORY, "the setting, player 1's unchanged");
    act(PC_MENU_RIGHT);
    act(PC_MENU_RIGHT);
    at("Controllers (all players)", "P2 accessory", "auto: none", "auto again: what it gives now");
    CHECK(test_pak[1] == PC_CFG_PAK_AUTO, "auto: %d", test_pak[1]);
    act(PC_MENU_UP);
    at("Controllers (all players)", "P1 accessory", "controller pak", "player 1's: the controller pak");
    CHECK(test_pak[0] == -1, "player 1's not touched: %d", test_pak[0]);
    /* the cars a race holds (game.cars): the setting at once, the race's count only from a race's start, when the
     * game latches it (pc_race_cars_begin) */
    act(PC_MENU_BACK);
    act(PC_MENU_DOWN);
    act(PC_MENU_ACCEPT);
    at("Game", "Cars in a race", "original (6)", "the Game page: original");
    CHECK(pc_cars_setting() == 6 && pc_race_cars() == 6, "6 and 6: %d, %d", pc_cars_setting(), pc_race_cars());
    act(PC_MENU_RIGHT);
    at("Game", "Cars in a race", "7", "7 cars");
    CHECK(pc_cars_setting() == 7 && pc_menu_settings()->cars == 7 && pc_race_cars() == 6,
          "the setting 7 at once, the race's 6 kept: %d, %d", pc_cars_setting(), pc_race_cars());
    act_n(PC_MENU_RIGHT, 5);
    at("Game", "Cars in a race", "12", "12 cars, the most");
    pc_race_cars_begin();   /* a race starts */
    CHECK(pc_race_cars() == 12, "the race latched 12: %d", pc_race_cars());
    act(PC_MENU_RIGHT);
    at("Game", "Cars in a race", "original (6)", "past 12: original");
    CHECK(pc_cars_setting() == 6 && pc_race_cars() == 12, "during the race: the setting 6, the race's 12 kept: %d, %d",
          pc_cars_setting(), pc_race_cars());
    act(PC_MENU_LEFT);
    at("Game", "Cars in a race", "12", "before original: 12");
    /* the game's memory (game.memory, docs/port/MEMORY.md): the setting only, for the next start (marked *) */
    act(PC_MENU_DOWN);
    at("Game", "Memory", "original (N64)", "the memory: original");
    CHECK(pc_menu_settings()->memory == 0, "original: %d", pc_menu_settings()->memory);
    act_n(PC_MENU_RIGHT, 3);
    at("Game", "Memory", "64 MB", "64 MB");
    CHECK(pc_menu_settings()->memory == 64, "64: %d", pc_menu_settings()->memory);
    act_n(PC_MENU_RIGHT, 2);
    at("Game", "Memory", "256 MB", "256 MB, the most");
    act(PC_MENU_RIGHT);
    at("Game", "Memory", "original (N64)", "past 256: original");
    act(PC_MENU_LEFT);
    at("Game", "Memory", "256 MB", "before original: 256");
    /* the game's frame rate (game.frame_rate, FRAMERATE.md 7): the setting at once, the game's copy only at its next
     * change of state (pc_game_fps_begin) */
    act(PC_MENU_DOWN);
    at("Game", "Game frame rate", "original (30)", "the game's frame rate: original");
    CHECK(pc_game_fps_setting() == 0 && pc_fps60 == 0, "original: %d, %d", pc_game_fps_setting(), pc_fps60);
    act(PC_MENU_RIGHT);
    at("Game", "Game frame rate", "60", "60");
    CHECK(pc_game_fps_setting() == 60 && pc_menu_settings()->game_fps == 60 && pc_fps60 == 0,
          "the setting 60 at once, the game's original kept: %d, %d", pc_game_fps_setting(), pc_fps60);
    pc_game_fps_begin();   /* the game's state changes */
    CHECK(pc_fps60 == 1, "the next screen latched 60: %d", pc_fps60);
    act(PC_MENU_RIGHT);
    at("Game", "Game frame rate", "original (30)", "past 60: original");
    CHECK(pc_game_fps_setting() == 0 && pc_fps60 == 1, "on this screen: the setting original, the game's 60 kept");
    act(PC_MENU_LEFT);
    at("Game", "Game frame rate", "60", "before original: 60");
    act(PC_MENU_DOWN);
    at("Game", "Defaults", NULL, "the page's Defaults");
    act(PC_MENU_ACCEPT);
    act(PC_MENU_UP);
    at("Game", "Game frame rate", "original (30)", "Defaults: original");
    CHECK(pc_menu_settings()->game_fps == 0 && pc_game_fps_setting() == 0 && pc_fps60 == 1, "%d, %d, %d",
          pc_menu_settings()->game_fps, pc_game_fps_setting(), pc_fps60);
    pc_game_fps_begin();
    CHECK(pc_fps60 == 0, "the next screen: original (%d)", pc_fps60);
    act(PC_MENU_UP);
    at("Game", "Memory", "original (N64)", "Defaults: the memory original");
    CHECK(pc_menu_settings()->memory == 0, "%d", pc_menu_settings()->memory);
    act(PC_MENU_UP);
    at("Game", "Cars in a race", "original (6)", "Defaults: original");
    CHECK(pc_menu_settings()->cars == 0 && pc_cars_setting() == 6 && pc_race_cars() == 12, "%d, %d, %d",
          pc_menu_settings()->cars, pc_cars_setting(), pc_race_cars());
    pc_race_cars_begin();
    CHECK(pc_race_cars() == 6, "the next race: 6 (%d)", pc_race_cars());
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
    act_n(PC_MENU_DOWN, 7);
    at("Settings", "Save to rush2049pc.ini", "4 changes", "four to save");
    act(PC_MENU_ACCEPT);
    at("Settings", "Save to rush2049pc.ini", "saved", "saved");
    CHECK(!strcmp(pc_menu_message(), "Saved 4 settings to menu.ini"), "the message: '%s'", pc_menu_message());
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
    CHECK(!strcmp(pc_menu_message(), "Nothing to save"), "the message: '%s'", pc_menu_message());
    at("Settings", "Save to rush2049pc.ini", "saved", "still saved");
    act_n(PC_MENU_UP, 8);
    act(PC_MENU_ACCEPT);
    act(PC_MENU_LEFT);         /* the volume 95: something to save again */
    act(PC_MENU_BACK);
    act_n(PC_MENU_DOWN, 8);
    at("Settings", "Save to rush2049pc.ini", "1 change", "a change after the save");
    act_n(PC_MENU_UP, 8);
    act(PC_MENU_ACCEPT);
    act(PC_MENU_RIGHT);        /* and back to what the file has */
    act(PC_MENU_BACK);
    act_n(PC_MENU_DOWN, 8);
    at("Settings", "Save to rush2049pc.ini", "saved", "the file has it all again");
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
    act_n(PC_MENU_DOWN, 8);
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

/* ---------------------------------------------------------------- mouse */

/* The layout as FRONTEND.md gives it: a panel of 304 x 224 units in the middle, a unit the largest whole number of
 * pixels that fits 320 x 240 in the window; rows of 12 from y 18, values from x 168, the arrows at 160 and 289, the
 * scroll arrows from x 294, a bar of 60. */
typedef struct {
    int k, ox, oy;
} Panel;

static Panel panel_of(int w, int h)
{
    Panel p;
    p.k = w / 320 < h / 240 ? w / 320 : h / 240;
    if (p.k < 1)
        p.k = 1;
    p.ox = (w - 304 * p.k) / 2;
    p.oy = (h - 224 * p.k) / 2;
    return p;
}

/* The middle of row i's unit (ux, 18 + 12 i + uy) in pixels. */
static void row_px(const Panel *p, int i, int ux, int uy, int *x, int *y)
{
    *x = p->ox + ux * p->k + p->k / 2;
    *y = p->oy + (18 + 12 * i + uy) * p->k + p->k / 2;
}

static void hit_is(int w, int h, int x, int y, int area, int item, int part, const char *what)
{
    PcMenuHit hit;
    pc_menu_hit(w, h, x, y, &hit);
    CHECK(hit.area == area && (item < -1 || hit.item == item) && (part < 0 || hit.part == part),
          "%s: at (%d, %d) of %dx%d: area %d item %d part %d; expected %d %d %d", what, x, y, w, h, hit.area,
          hit.item, hit.part, area, item, part);
}

/* A mouse event at (x, y) in the window system's units of a ww x wh window whose drawable is dw x dh; then 0.6 s
 * pass, so that two clicks aren't a double click. */
static void mouse_at(int what, int x, int y, int ww, int wh, int dw, int dh, int amount)
{
    int px, py;
    pc_menu_mouse_pixels(x, y, ww, wh, dw, dh, &px, &py);
    pc_menu_mouse(what, px, py, dw, dh, amount);
    pump();
    test_now_ns += 600000000u;
}

/* The selected row's highlight is drawn at row i: the window's pixel left of the labels there. */
static int highlighted(int w, int h, int i)
{
    static uint8_t px[1280 * 960 * 4];
    Panel p = panel_of(w, h);
    int x, y, gw, gh;
    row_px(&p, i, 3, 1, &x, &y);
    if (w * h > 1280 * 960 || pc_gfx_read_window(px, &gw, &gh) != 0 || gw != w || gh != h)
        return 0;
    const uint8_t *c = px + ((size_t)y * w + x) * 4;
    return c[0] == 0x2E && c[1] == 0x5A && c[2] == 0xA0;
}

static void test_mouse(void)
{
    /* window sizes in the window system's units, and their drawables in pixels */
    static const struct { int ww, wh, dw, dh; const char *what; } sizes[] = {
        {640, 480, 640, 480, "640 x 480"},
        {1920, 1080, 1920, 1080, "1080p"},
        {2880, 1620, 2880, 1620, "a 200% Windows desktop: DPI aware, pixels both"},
        {1440, 810, 2880, 1620, "2x points (macOS, Wayland)"},
        {1280, 720, 1920, 1080, "1.5x"},
        {853, 480, 853, 480, "widescreen 16:9 at 480"},
        {3440, 1440, 3440, 1440, "21:9 fullscreen"},
        {320, 240, 320, 240, "the smallest window"},
        {1366, 768, 1366, 768, "a laptop panel, an odd fit"},
        {1000, 1000, 1250, 1250, "1.25x, square"},
    };
    PcSettings s;
    PcPad pads[4];
    pc_settings_defaults(&s);
    if (start(&s) != 0) {
        g_fail++;
        return;
    }
    show_frame();
    act(PC_MENU_TOGGLE);
    act_n(PC_MENU_DOWN, 2);
    act(PC_MENU_ACCEPT);
    at("Keyboard", "A", NULL, "the Keyboard page: 24 items, a scrolled list");
    for (size_t n = 0; n < sizeof sizes / sizeof *sizes; n++) {
        int ww = sizes[n].ww, wh = sizes[n].wh, dw = sizes[n].dw, dh = sizes[n].dh, x, y, px, py;
        Panel p = panel_of(dw, dh);
        char what[160];
        /* every window unit lands in its own pixels */
        for (int u = 0; u < ww; u += 37) {
            pc_menu_mouse_pixels(u, u * wh / ww, ww, wh, dw, dh, &px, &py);
            CHECK(px >= u * dw / ww && px < (u + 1) * dw / ww + (dw % ww != 0) && px < dw, "%s: unit %d -> pixel %d",
                  sizes[n].what, u, px);
        }
        /* the rows, from the window's units */
        for (int i = 0; i < 15; i += 7) {
            row_px(&p, i, 100, 6, &x, &y);
            snprintf(what, sizeof what, "%s: row %d", sizes[n].what, i);
            pc_menu_mouse_pixels(x * ww / dw, y * wh / dh, ww, wh, dw, dh, &px, &py);
            hit_is(dw, dh, px, py, PC_HIT_ITEM, i, PC_PART_LABEL, what);
        }
        /* the panel's edges, in pixels */
        snprintf(what, sizeof what, "%s: edges", sizes[n].what);
        hit_is(dw, dh, p.ox, p.oy, PC_HIT_PANEL, -1, -1, what);
        hit_is(dw, dh, p.ox - 1, p.oy + 50 * p.k, PC_HIT_OUTSIDE, -1, -1, what);
        hit_is(dw, dh, p.ox + 50 * p.k, p.oy - 1, PC_HIT_OUTSIDE, -1, -1, what);
        hit_is(dw, dh, p.ox + 304 * p.k - 1, p.oy + 224 * p.k - 1, PC_HIT_PANEL, -1, -1, what);
        hit_is(dw, dh, p.ox + 304 * p.k, p.oy + 50 * p.k, PC_HIT_OUTSIDE, -1, -1, what);
        hit_is(dw, dh, p.ox + 50 * p.k, p.oy + 224 * p.k, PC_HIT_OUTSIDE, -1, -1, what);
        hit_is(dw, dh, 0, 0, PC_HIT_OUTSIDE, -1, -1, what);
        /* a row's first and last pixel lines, and the title above the first */
        hit_is(dw, dh, p.ox + 100 * p.k, p.oy + 18 * p.k, PC_HIT_ITEM, 0, -1, what);
        hit_is(dw, dh, p.ox + 100 * p.k, p.oy + 18 * p.k - 1, PC_HIT_PANEL, -1, -1, what);
        hit_is(dw, dh, p.ox + 100 * p.k, p.oy + 30 * p.k - 1, PC_HIT_ITEM, 0, -1, what);
        hit_is(dw, dh, p.ox + 100 * p.k, p.oy + 30 * p.k, PC_HIT_ITEM, 1, -1, what);
        hit_is(dw, dh, p.ox + 100 * p.k, p.oy + 198 * p.k, PC_HIT_PANEL, -1, -1, what);   /* the footer */
        /* the scroll arrow on the last row (more below), none on the first (nothing above) */
        row_px(&p, 14, 297, 6, &x, &y);
        hit_is(dw, dh, x, y, PC_HIT_SCROLL_DOWN, -1, -1, what);
        row_px(&p, 0, 297, 6, &x, &y);
        hit_is(dw, dh, x, y, PC_HIT_ITEM, 0, -1, what);
    }
    act(PC_MENU_BACK);
    act_n(PC_MENU_UP, 2);
    act(PC_MENU_ACCEPT);
    at("Video", "Render scale", NULL, "Video");
    for (size_t n = 0; n < sizeof sizes / sizeof *sizes; n++) {   /* the parts of a value */
        int dw = sizes[n].dw, dh = sizes[n].dh, x, y;
        Panel p = panel_of(dw, dh);
        row_px(&p, 1, 20, 6, &x, &y);
        hit_is(dw, dh, x, y, PC_HIT_ITEM, 1, PC_PART_LABEL, sizes[n].what);
        row_px(&p, 1, 161, 6, &x, &y);
        hit_is(dw, dh, x, y, PC_HIT_ITEM, 1, PC_PART_LESS, sizes[n].what);
        row_px(&p, 1, 200, 6, &x, &y);
        hit_is(dw, dh, x, y, PC_HIT_ITEM, 1, PC_PART_LESS, sizes[n].what);
        row_px(&p, 1, 240, 6, &x, &y);
        hit_is(dw, dh, x, y, PC_HIT_ITEM, 1, PC_PART_MORE, sizes[n].what);
        row_px(&p, 1, 291, 6, &x, &y);
        hit_is(dw, dh, x, y, PC_HIT_ITEM, 1, PC_PART_MORE, sizes[n].what);
    }

    /* the overlay driven by the mouse, at 640 x 480 (window units and pixels the same) */
    Panel p = panel_of(640, 480);
    int x, y;
    row_px(&p, 1, 100, 6, &x, &y);
    mouse_at(PC_MOUSE_MOVE, x, y, 640, 480, 640, 480, 0);
    at("Video", "Widescreen", "auto", "pointing selects");
    CHECK(highlighted(640, 480, 1), "the highlight is drawn where the mouse is");
    row_px(&p, 1, 240, 6, &x, &y);
    mouse_at(PC_MOUSE_LEFT, x, y, 640, 480, 640, 480, 0);
    at("Video", "Widescreen", "off", "a click right of the value: the next");
    row_px(&p, 1, 161, 6, &x, &y);
    mouse_at(PC_MOUSE_LEFT, x, y, 640, 480, 640, 480, 0);
    mouse_at(PC_MOUSE_LEFT, x, y, 640, 480, 640, 480, 0);
    at("Video", "Widescreen", "21:9", "two clicks on the left arrow: back past the start");
    row_px(&p, 6, 20, 6, &x, &y);
    mouse_at(PC_MOUSE_LEFT, x, y, 640, 480, 640, 480, 0);
    at("Video", "Vsync", "off", "a click on a label: as Enter, the next value");
    CHECK(!pc_menu_settings()->vsync, "the setting changed");
    mouse_at(PC_MOUSE_LEFT, x, y, 640, 480, 640, 480, 0);
    at("Video", "Vsync", "on", "and back");
    mouse_at(PC_MOUSE_MOVE, p.ox + 10, p.oy + 5, 640, 480, 640, 480, 0);
    at("Video", "Vsync", NULL, "the title is no item: the selection stays");
    mouse_at(PC_MOUSE_RIGHT, x, y, 640, 480, 640, 480, 0);
    at("Settings", "Video", NULL, "the right button goes back");
    CHECK(pc_menu_is_open(), "still open");

    /* a double click on Video: the second press, on the page the first opened, changes nothing there */
    row_px(&p, 0, 20, 6, &x, &y);
    pc_menu_mouse(PC_MOUSE_LEFT, x, y, 640, 480, 0);
    test_now_ns += 200000000u;
    pc_menu_mouse(PC_MOUSE_LEFT, x, y, 640, 480, 0);
    pump();
    at("Video", "Vsync", "on", "a double click opens the page (on the item it was left on), and no more");
    test_now_ns += 400000000u;
    mouse_at(PC_MOUSE_LEFT, x, y, 640, 480, 640, 480, 0);
    at("Video", "Render scale", "N64 (1x)", "a click later counts");
    mouse_at(PC_MOUSE_LEFT, p.ox + 161 * p.k, y, 640, 480, 640, 480, 0);
    at("Video", "Render scale", "native", "and back");
    mouse_at(PC_MOUSE_RIGHT, x, y, 640, 480, 640, 480, 0);

    /* the volume's bar: where it is clicked */
    row_px(&p, 1, 20, 6, &x, &y);
    mouse_at(PC_MOUSE_LEFT, x, y, 640, 480, 640, 480, 0);
    at("Audio", "Volume", "100 %", "a click opens a page");
    row_px(&p, 0, 20, 6, &x, &y);
    mouse_at(PC_MOUSE_LEFT, p.ox + (168 + 30) * p.k, y, 640, 480, 640, 480, 0);
    at("Audio", "Volume", "50 %", "the bar's middle: 50");
    CHECK(test_volume == 0.5, "port/audio's volume %g", test_volume);
    mouse_at(PC_MOUSE_LEFT, p.ox + (168 + 59) * p.k + 1, y, 640, 480, 640, 480, 0);
    at("Audio", "Volume", "100 %", "the bar's end: 100");
    mouse_at(PC_MOUSE_LEFT, p.ox + 168 * p.k, y, 640, 480, 640, 480, 0);
    at("Audio", "Volume", "0 %", "its start: 0");
    mouse_at(PC_MOUSE_LEFT, p.ox + (168 + 66) * p.k, y, 640, 480, 640, 480, 0);
    at("Audio", "Volume", "5 %", "right of the bar: a step up");
    mouse_at(PC_MOUSE_RIGHT, x, y, 640, 480, 640, 480, 0);

    /* the wheel scrolls a long page, and the item under the mouse is then the selected one */
    row_px(&p, 2, 20, 6, &x, &y);
    mouse_at(PC_MOUSE_LEFT, x, y, 640, 480, 640, 480, 0);
    at("Keyboard", "A", NULL, "Keyboard");
    row_px(&p, 4, 20, 6, &x, &y);
    mouse_at(PC_MOUSE_MOVE, x, y, 640, 480, 640, 480, 0);
    at("Keyboard", "D-pad up", NULL, "row 4");
    mouse_at(PC_MOUSE_WHEEL, 0, 0, 640, 480, 640, 480, -1);
    at("Keyboard", "D-pad right", NULL, "a notch down: 3 rows; row 4 is item 7 now");
    hit_is(640, 480, x, y, PC_HIT_ITEM, 7, -1, "the list moved");
    mouse_at(PC_MOUSE_WHEEL, 0, 0, 640, 480, 640, 480, -10);
    hit_is(640, 480, x, y, PC_HIT_ITEM, 13, -1, "at most to the last page: item 9 at the top");
    at("Keyboard", "C-right", NULL, "and under the mouse");
    row_px(&p, 0, 297, 6, &x, &y);
    hit_is(640, 480, x, y, PC_HIT_SCROLL_UP, -1, -1, "now there is something above");
    mouse_at(PC_MOUSE_LEFT, x, y, 640, 480, 640, 480, 0);
    row_px(&p, 4, 20, 6, &x, &y);
    hit_is(640, 480, x, y, PC_HIT_ITEM, 10, -1, "the up arrow: 3 rows up");
    CHECK(highlighted(640, 480, 7), "the selection stays on its item, which moved down to row 7");

    /* a binding: a click waits for a key; the mouse doesn't move the selection meanwhile; a click cancels */
    mouse_at(PC_MOUSE_LEFT, x, y, 640, 480, 640, 480, 0);
    at("Keyboard", "C-up", "I", "the item clicked");
    CHECK(pc_menu_capturing() == PC_MENU_CAPTURE_KEY, "waiting for a key");
    row_px(&p, 1, 20, 6, &x, &y);
    mouse_at(PC_MOUSE_MOVE, x, y, 640, 480, 640, 480, 0);
    at("Keyboard", "C-up", NULL, "pointing elsewhere while it waits");
    mouse_at(PC_MOUSE_LEFT, x, y, 640, 480, 640, 480, 0);
    CHECK(!pc_menu_capturing(), "a click cancels");
    at("Keyboard", "C-up", "I", "unchanged");
    mouse_at(PC_MOUSE_WHEEL, 0, 0, 640, 480, 640, 480, 10);
    at("Keyboard", "B", NULL, "back at the top: the mouse is on row 1");

    /* a 2x display: points in, pixels drawn; at 1280 x 960 */
    pc_gfx_set_window_size(1280, 960);
    p = panel_of(1280, 960);
    row_px(&p, 5, 100, 6, &x, &y);
    mouse_at(PC_MOUSE_MOVE, x / 2, y / 2, 640, 480, 1280, 960, 0);
    at("Keyboard", "D-pad down", NULL, "2x: row 5");
    CHECK(highlighted(1280, 960, 5), "drawn at row 5 of the 1280 x 960 window");

    /* outside the panel: back, and on the first page it closes */
    mouse_at(PC_MOUSE_LEFT, 2, 2, 640, 480, 1280, 960, 0);
    at("Settings", "Keyboard", NULL, "a click outside: back");
    mouse_at(PC_MOUSE_LEFT, 2, 2, 640, 480, 1280, 960, 0);
    CHECK(!pc_menu_is_open() && !pc_input_paused(), "and again: closed");

    /* closed, the mouse does nothing: the game never gets it */
    mouse_at(PC_MOUSE_LEFT, x / 2, y / 2, 640, 480, 1280, 960, 0);
    mouse_at(PC_MOUSE_RIGHT, x / 2, y / 2, 640, 480, 1280, 960, 0);
    mouse_at(PC_MOUSE_WHEEL, x / 2, y / 2, 640, 480, 1280, 960, 1);
    pc_input_poll(pads);
    CHECK(!pc_menu_is_open() && pads[0].buttons == 0 && pads[0].stick_x == 0 && pads[0].stick_y == 0,
          "closed: clicks don't open it, nor reach the pads");
    stop();
}

/* ---------------------------------------------------------------- pages: Help and About */

static int g_fills;

static void mock_about(PcAbout *a)
{
    g_fills++;
    snprintf(a->version, sizeof a->version, "v9.9.9-1-gabcdef1");
    snprintf(a->build_type, sizeof a->build_type, "RelWithDebInfo");
    snprintf(a->commit, sizeof a->commit, "abcdef1234567890abcdef1234567890abcdef12");
    snprintf(a->build, sizeof a->build, "local build");
    snprintf(a->audio, sizeof a->audio, "null at 48000 Hz");
    snprintf(a->audio_buffer, sizeof a->audio_buffer, "pulls of 512 (11 ms), queue %d (50 ms)", 1600 + g_fills);
    snprintf(a->rom, sizeof a->rom, "/roms/baserom.us.z64 (next to the program)");
    snprintf(a->saves, sizeof a->saves, "/home/player/.local/share/rush2049pc/rush2049pc.mpk");
    snprintf(a->log, sizeof a->log, "/home/player/.local/share/rush2049pc/rush2049pc.log");
    snprintf(a->data, sizeof a->data, "/home/player/Games/San Francisco Rush 2049/rush2049pc-v1.0-linux-x86_64");
    snprintf(a->memory, sizeof a->memory, "64 MiB; heap 3.5 of 63.5 MiB at most in use");
    snprintf(a->mods, sizeof a->mods, "1 on: Red Cars 1.2 (3 files, text)");
}

static void has_line(const char *text, const char *line, const char *what)
{
    CHECK(strstr(text, line) != NULL, "%s: no line '%s' in\n%s", what, line, text);
}

static void test_pages(void)
{
    PcSettings s;
    static char text[65536];
    char want[512];
    const char *vendor, *renderer, *version, *glsl;
    pc_settings_defaults(&s);
    snprintf(s.save_path, sizeof s.save_path, "/home/player/.config/rush2049pc/rush2049pc.ini");
    snprintf(s.screenshot_dir, sizeof s.screenshot_dir, "/home/player/.local/share/rush2049pc/screenshots");
    snprintf(s.key[PC_CTL_A], PC_CFG_BIND, "X, Space");
    s.analog = PC_CFG_STICK_RIGHT;
    snprintf(s.pad_menu, PC_CFG_BIND, "back+start");
    snprintf(s.pad[PC_CTL_CUP], PC_CFG_BIND, "-righty, y");
    pc_menu_set_about(mock_about);
    if (start(&s) != 0) {
        g_fail++;
        return;
    }

    /* the content provider: main's facts, the GL driver, the players, the files */
    size_t n = pc_menu_about_text(text, sizeof text);
    pc_gfx_driver(&vendor, &renderer, &version, &glsl);
    CHECK(n == strlen(text) && !strncmp(text, "Rush 2049 PC\n", 13), "the title: %.*s", 20, text);
    CHECK(vendor[0] && renderer[0] && version[0], "the driver's strings: '%s' '%s' '%s'", vendor, renderer, version);
    has_line(text, "\nVersion: v9.9.9-1-gabcdef1 (RelWithDebInfo)\n", "version and build type");
    has_line(text, "\nCommit: abcdef1234567890abcdef1234567890abcdef12\n", "commit");
    has_line(text, "\nBuild: local build\n", "build");
    snprintf(want, sizeof want, "\nGL vendor: %s\n", vendor);
    has_line(text, want, "GL vendor");
    snprintf(want, sizeof want, "\nGL renderer: %s\n", renderer);
    has_line(text, want, "GL renderer");
    snprintf(want, sizeof want, "\nGL version: %s (GLSL %s)\n", version, glsl);
    has_line(text, want, "GL version");
    has_line(text, "\nAudio: null at 48000 Hz\n", "audio");
    has_line(text, "\nAudio buffer: pulls of 512 (11 ms), queue 1601 (50 ms)\n", "audio buffer");
    has_line(text, "\nMemory: 64 MiB; heap 3.5 of 63.5 MiB at most in use\n", "memory");
    has_line(text, "\nMods: 1 on: Red Cars 1.2 (3 files, text)\n", "the mods in use");
    has_line(text, "\nGame frame rate: original (30 a second)\n", "game.frame_rate");   /* FRAMERATE.md 7.7 */
    has_line(text, "\nPlayer 1: -\nPlayer 2: -\nPlayer 3: -\nPlayer 4: -\n", "no live input headless: no players");
    has_line(text, "\nROM: /roms/baserom.us.z64 (next to the program)\n", "ROM");
    has_line(text, "\nSettings file: /home/player/.config/rush2049pc/rush2049pc.ini\n", "the settings file");
    has_line(text, "\nSaves: /home/player/.local/share/rush2049pc/rush2049pc.mpk\n", "saves");
    has_line(text, "\nScreenshots: /home/player/.local/share/rush2049pc/screenshots\n", "screenshots");
    has_line(text, "\nLog: /home/player/.local/share/rush2049pc/rush2049pc.log\n", "log");
    has_line(text, "\nData folder: /home/player/Games/San Francisco Rush 2049/rush2049pc-v1.0-linux-x86_64\n",
             "data folder");

    /* the page: made again each time it opens, so it follows main's numbers */
    act(PC_MENU_TOGGLE);
    act_n(PC_MENU_DOWN, 8);
    act(PC_MENU_ACCEPT);
    at("About", "Version", "v9.9.9-1-gabcdef1 (RelWithDebInfo)", "About opens on the version");
    act_n(PC_MENU_DOWN, 7);
    at("About", "Audio buffer", "pulls of 512 (11 ms), queue 1602 (50 ms)", "made when opened");
    act(PC_MENU_BACK);
    act(PC_MENU_ACCEPT);
    at("About", "Audio buffer", "pulls of 512 (11 ms), queue 1603 (50 ms)", "and again");
    CHECK(g_fills == 3, "main's fill: once a time (%d)", g_fills);
    act_n(PC_MENU_UP, 7);
    at("About", "Version", NULL, "up to the first");
    act(PC_MENU_UP);
    at("About", "Data folder", "/home/player/Games/San Francisco Rush 2049/rush2049pc-v1.0-linux-x86_64",
       "wraps to the last: no actions headless");
    act(PC_MENU_ACCEPT);
    at("About", "Data folder", NULL, "Enter on a row does nothing");
    act(PC_MENU_BACK);

    /* Help: from the bindings in effect */
    act(PC_MENU_UP);
    act(PC_MENU_ACCEPT);
    at("Help", "A", "X, Space | A, RT", "the keys and the buttons, in Xbox names");
    act(PC_MENU_DOWN);
    at("Help", "B", "C | X, LT", "B");
    act_n(PC_MENU_DOWN, 9);
    at("Help", "C-up", "I | RS up, Y", "a stick direction by its stick");
    act_n(PC_MENU_DOWN, 4);
    at("Help", "Stick up", "Up | RS up", "the N64 stick is the right one");
    act_n(PC_MENU_DOWN, 4);
    at("Help", "Settings", "F1 | Back+Start", "the hotkeys after a heading; a combination");
    act(PC_MENU_DOWN);
    at("Help", "Fullscreen", "F11, Alt+Enter | ", "fullscreen");
    act_n(PC_MENU_DOWN, 4);
    at("Help", "Move", "arrows, PgUp/PgDn | D-pad, LS, LB/RB", "this menu's keys, after another heading");
    act(PC_MENU_BACK);
    /* a binding changed: Help has it */
    act_n(PC_MENU_UP, 5);
    act(PC_MENU_ACCEPT);
    act(PC_MENU_DOWN);
    act(PC_MENU_CLEAR);
    act(PC_MENU_BACK);
    act_n(PC_MENU_DOWN, 5);
    act(PC_MENU_ACCEPT);
    at("Help", "Move", NULL, "Help keeps its place");
    act_n(PC_MENU_PAGE_UP, 2);
    at("Help", "A", NULL, "a page up twice: the first row, past the heading");
    act(PC_MENU_DOWN);
    at("Help", "B", "- | X, LT", "B's key cleared");
    act(PC_MENU_CLOSE);
    pc_menu_set_about(NULL);
    stop();
}

/* ---------------------------------------------------------------- pictures (not a test) */

int pcgfx_png_write(const char *path, const uint8_t *rgba, int w, int h);   /* port/gfx/gfx_png.c */

/* ---------------------------------------------------------------- the Saves page (docs/port/SAVES.md) */

/* main's side, mocked: player 1 keeps files (a profile and two ghosts), player 2 the .mpk, players 3 and 4 nothing. */
static PcSaveRow g_sv[3];
static int g_sv_n;
static char g_sv_acts[1024];

static int mock_backend(int player, char *where, size_t n)
{
    if (player == 0) {
        snprintf(where, n, "/home/player/.local/share/rush2049pc/saves/p1");
        return PC_SAVES_FILES;
    }
    snprintf(where, n, player == 1 ? "/home/player/.local/share/rush2049pc/rush2049pc-p2.mpk" : "");
    return player == 1 ? PC_SAVES_PAK : PC_SAVES_NONE;
}

static int mock_list(int player, PcSaveRow *out, int max)
{
    int n = 0;
    for (int i = 0; player == 0 && i < g_sv_n && n < max; i++)
        out[n++] = g_sv[i];
    return n;
}

static int mock_act(int player, int action, const char *file, char *msg, size_t size)
{
    static const char *const names[] = {"pin", "unpin", "export", "delete", "import", "export_mpk"};
    snprintf(g_sv_acts + strlen(g_sv_acts), sizeof g_sv_acts - strlen(g_sv_acts), "%d:%s:%s;", player + 1,
             names[action], file);
    for (int i = 0; i < g_sv_n; i++)
        if (!strcmp(g_sv[i].file, file)) {
            if (action == PC_SAVES_PIN || action == PC_SAVES_UNPIN)
                g_sv[i].pinned = action == PC_SAVES_PIN;
            if (action == PC_SAVES_DELETE) {
                memmove(&g_sv[i], &g_sv[i + 1], (size_t)(g_sv_n - i - 1) * sizeof g_sv[0]);
                g_sv_n--;
            }
            break;
        }
    snprintf(msg, size, "did %s", names[action]);
    return 0;
}

static int mock_notice(int player, char *out, size_t size)
{
    snprintf(out, size, "%s", player == 0 ? "Refused x.r49ghost: its CRC is wrong (the file is damaged)" : "");
    return player == 0;
}

static const PcSavesUi g_mock_saves = {mock_backend, mock_list, mock_act, mock_notice};

static void test_saves_page(void)
{
    PcSettings s;
    pc_settings_defaults(&s);
    s.saves[0] = PC_CFG_SAVES_FILES;
    memset(g_sv, 0, sizeof g_sv);
    snprintf(g_sv[0].file, sizeof g_sv[0].file, "ALICE.r49save");
    snprintf(g_sv[0].label, sizeof g_sv[0].label, "ALICE");
    g_sv[0].kind = 1, g_sv[0].on_pak = 1, g_sv[0].pages = 9;
    snprintf(g_sv[1].file, sizeof g_sv[1].file, "track 1 forward - ALICE - 1m02.35s.r49ghost");
    snprintf(g_sv[1].label, sizeof g_sv[1].label, "T1 fwd ALICE 1:02.35");
    snprintf(g_sv[1].name, sizeof g_sv[1].name, "ALICE");
    g_sv[1].kind = 2, g_sv[1].track = 0, g_sv[1].finish = 62.35f, g_sv[1].car = 3, g_sv[1].pages = 12;
    snprintf(g_sv[2].file, sizeof g_sv[2].file, "track 2 backward - BOB - 1m10.00s.r49ghost");
    snprintf(g_sv[2].label, sizeof g_sv[2].label, "T2 bwd BOB 1:10.00");
    g_sv[2].kind = 2, g_sv[2].track = 1, g_sv[2].backward = 1, g_sv[2].on_pak = 1, g_sv[2].imported = 1;
    g_sv_n = 3;
    g_sv_acts[0] = 0;
    pc_menu_set_saves(&g_mock_saves);
    if (start(&s) != 0) {
        g_fail++;
        return;
    }
    act(PC_MENU_TOGGLE);
    act_n(PC_MENU_DOWN, 6);
    at("Settings", "Saves", NULL, "Saves after Game and Mods");
    act(PC_MENU_ACCEPT);
    at("Saves", "Player", "P1", "the Saves page, player 1");
    act(PC_MENU_DOWN);
    at("Saves", "Kept as", "files (a folder)", "kept as files (game.saves)");
    act(PC_MENU_DOWN);
    at("Saves", "Folder", "/home/player/.local/share/rush2049pc/saves/p1", "the folder");
    act(PC_MENU_DOWN);
    at("Saves", "Import", "Refused x.r49ghost: its CRC is wrong (the file is damaged)", "a file refused at start");
    act(PC_MENU_DOWN);
    at("Saves", "Import ghosts", NULL, "the actions");
    act(PC_MENU_DOWN);
    at("Saves", "Export to the .mpk", NULL, "export to the .mpk (no Open headless)");
    act(PC_MENU_DOWN);
    at("Saves", "ALICE", "on the pak", "the profile, on the pak");
    act(PC_MENU_DOWN);
    at("Saves", "T1 fwd ALICE 1:02.35", "kept", "a ghost kept off the pak");
    act(PC_MENU_DOWN);
    at("Saves", "T2 bwd BOB 1:10.00", "on the pak, import", "an imported ghost on the pak");
    /* a ghost's own page: pin, export, delete (asked twice) */
    act(PC_MENU_UP);
    act(PC_MENU_ACCEPT);
    at("T1 fwd ALICE 1:02.35", "Kind", "ghost", "the ghost's page");
    act(PC_MENU_DOWN);
    at("T1 fwd ALICE 1:02.35", "Player", "ALICE", "its player");
    act(PC_MENU_DOWN);
    at("T1 fwd ALICE 1:02.35", "Track", "1, forward", "its track and way");
    act(PC_MENU_DOWN);
    at("T1 fwd ALICE 1:02.35", "Time", "1:02.35", "its time");
    act_n(PC_MENU_DOWN, 5);
    at("T1 fwd ALICE 1:02.35", "Pin to the pak", NULL, "Pin");
    act(PC_MENU_ACCEPT);
    CHECK(strstr(g_sv_acts, "1:pin:track 1 forward - ALICE - 1m02.35s.r49ghost;") != NULL, "pinned: %s", g_sv_acts);
    at("T1 fwd ALICE 1:02.35", "Unpin", NULL, "now Unpin");
    CHECK(!strcmp(pc_menu_message(), "did pin"), "main's message in the footer: %s", pc_menu_message());
    act(PC_MENU_DOWN);
    at("T1 fwd ALICE 1:02.35", "Export (.r49ghost)", NULL, "Export");
    act(PC_MENU_ACCEPT);
    CHECK(strstr(g_sv_acts, "1:export:track 1") != NULL, "exported");
    act(PC_MENU_DOWN);
    at("T1 fwd ALICE 1:02.35", "Delete", "", "Delete");
    act(PC_MENU_ACCEPT);
    CHECK(!strstr(g_sv_acts, "delete") && strstr(pc_menu_message(), "again"), "asked once: %s", pc_menu_message());
    at("T1 fwd ALICE 1:02.35", "Delete", "again: delete", "it says so");
    act(PC_MENU_ACCEPT);
    CHECK(strstr(g_sv_acts, "1:delete:track 1 forward") != NULL && g_sv_n == 2, "deleted the second time");
    CHECK(!strcmp(pc_menu_message(), "did delete"), "the message: %s", pc_menu_message());
    const char *pg, *item;
    pc_menu_state(&pg, &item, NULL, 0);
    CHECK(!strcmp(pg, "Saves"), "back on the list once it is gone: %s", pg);
    /* the list's actions */
    act(PC_MENU_BACK);
    at("Settings", "Saves", NULL, "B: the main page");
    act(PC_MENU_ACCEPT);
    act(PC_MENU_PAGE_UP);
    act_n(PC_MENU_DOWN, 4);
    at("Saves", "Import ghosts", NULL, "Import");
    act(PC_MENU_ACCEPT);
    act(PC_MENU_DOWN);
    act(PC_MENU_ACCEPT);
    CHECK(strstr(g_sv_acts, "1:import:;1:export_mpk:;") != NULL, "import and export to the .mpk: %s", g_sv_acts);
    /* player 2 keeps the .mpk: what files would give, and the setting from the next start */
    act_n(PC_MENU_UP, 5);
    at("Saves", "Player", "P1", "the player row");
    act(PC_MENU_RIGHT);
    at("Saves", "Player", "P2", "player 2");
    act(PC_MENU_DOWN);
    at("Saves", "Kept as", "pak (.mpk)", "kept as the .mpk");
    act(PC_MENU_DOWN);
    at("Saves", "Pak", NULL, "player 2 has no controller pak now (auto)");
    act(PC_MENU_DOWN);
    at("Saves", "Kept in", "/home/player/.local/share/rush2049pc/rush2049pc-p2.mpk", "its .mpk");
    act(PC_MENU_DOWN);
    at("Saves", "Files", "Kept as files, each profile and ghost", "what files gives");
    act_n(PC_MENU_UP, 3);
    act(PC_MENU_RIGHT);
    CHECK(pc_menu_settings()->saves[1] == PC_CFG_SAVES_FILES, "game.saves2 = files, as a setting");
    at("Saves", "Kept as", "files (a folder)", "changed");
    act(PC_MENU_LEFT);
    act_n(PC_MENU_UP, 1);
    act(PC_MENU_LEFT);
    at("Saves", "Player", "P1", "Left: back to player 1");
    act(PC_MENU_CLOSE);
    pc_menu_set_saves(NULL);
    stop();
}

/* Each page as it looks over a frame, as PNGs in the scratch directory: for looking at, not checked. */
static void pictures(void)
{
    static const struct { const char *name; int down; } shots[] = {
        {"main", 0}, {"video", 0}, {"audio", 1}, {"keyboard", 2}, {"controllers", 3}, {"game", 4}, {"mods", 5},
        {"saves", 6}, {"help", 7}, {"about", 8}};
    static const PcMenuMod mods[3] = {
        {"red-cars", "Red Cars", "1.2", "Someone", 5, 13, 12, 1, 0, 1, ""},
        {"long-track", "Long Track", "0.3", "Someone Else", 0, 4, 4, 0, 12, 0, ""},
        {"old", "old", "", "", 0, 2, 0, 0, 0, 0, "it has no mod.ini (tools/mods/rushmods.py new makes one)"},
    };
    static uint8_t px[1280 * 960 * 4];
    PcSettings s;
    char path[PC_CFG_PATH];
    int w, h;
    pc_settings_defaults(&s);
    snprintf(s.save_path, sizeof s.save_path, "/home/player/.config/rush2049pc/rush2049pc.ini");
    pc_settings_set(&s, "mods", "long-track", "off", NULL, 0);
    pc_menu_set_about(mock_about);
    pc_menu_set_saves(&g_mock_saves);
    s.saves[0] = PC_CFG_SAVES_FILES;
    if (start(&s) != 0)
        return;
    pc_menu_set_mods(mods, 3, "/home/player/.local/share/rush2049pc/mods");
    pc_gfx_set_window_size(1280, 960);
    show_frame();
    for (size_t i = 0; i < sizeof shots / sizeof *shots; i++) {
        act(PC_MENU_TOGGLE);
        act_n(PC_MENU_DOWN, shots[i].down);
        if (i)
            act(PC_MENU_ACCEPT);
        int keys = !strcmp(shots[i].name, "keyboard"), about = !strcmp(shots[i].name, "about");
        if (keys) {   /* a capture in progress, and the list scrolled */
            act_n(PC_MENU_DOWN, 17);
            act(PC_MENU_ACCEPT);
        }
        if (about)   /* About's last row: a path over both footer lines */
            act(PC_MENU_UP);
        if (!strcmp(shots[i].name, "mods"))   /* a mod picked up to move */
            act(PC_MENU_DOWN), act(PC_MENU_ACCEPT);
        pc_gfx_read_window(px, &w, &h);
        snprintf(path, sizeof path, "%s/menu-%s.png", g_dir, shots[i].name);
        pcgfx_png_write(path, px, w, h);
        printf("%s\n", path);
        if (keys)
            pc_menu_capture(NULL);
        if (!strcmp(shots[i].name, "mods"))
            act(PC_MENU_ACCEPT);
        if (i)
            act(PC_MENU_BACK);
        act_n(PC_MENU_UP, shots[i].down);
        act(PC_MENU_CLOSE);
    }
    stop();
}

/* ---------------------------------------------------------------- the Mods page (docs/port/MODS.md) */

static void test_mods(void)
{
    PcSettings s;
    char path[PC_CFG_PATH - 64], *text;
    static const PcMenuMod mods[3] = {
        {"red", "Red Cars", "1.2", "Someone", 5, 3, 3, 0, 2, 1, ""},
        {"blue", "Blue", "", "", 0, 1, 0, 1, 0, 0, ""},
        {"noini", "noini", "", "", 0, 2, 0, 0, 0, 0, "it has no mod.ini"},
    };
    snprintf(path, sizeof path, "%s/mods.ini", g_dir);
    write_text(path, "[game]\nmods_order = blue\n");
    pc_settings_resolve(&s, PC_CFG_FILE_READ, path, NULL, NULL, 0);
    pc_menu_set_mods(mods, 3, "/data/mods");
    if (start(&s) != 0) {
        g_fail++;
        return;
    }
    act(PC_MENU_TOGGLE);
    act_n(PC_MENU_DOWN, 5);
    at("Settings", "Mods", ">", "the Mods page, after Game");
    act(PC_MENU_ACCEPT);
    at("Mods", "Mods", "on", "game.mods first");
    act(PC_MENU_DOWN);
    at("Mods", "1. Blue", "on", "blue first: game.mods_order says so");
    act(PC_MENU_DOWN);
    at("Mods", "2. Red Cars 1.2", "on", "then red, by its priority");
    act(PC_MENU_DOWN);
    at("Mods", "3. noini", "on (unusable)", "a mod that can't be used says so");
    act(PC_MENU_DOWN);
    at("Mods", "Folder", "/data/mods", "the folder");
    act(PC_MENU_DOWN);
    at("Mods", "Defaults", NULL, "Defaults last");
    act_n(PC_MENU_UP, 3);
    act(PC_MENU_LEFT);
    at("Mods", "2. Red Cars 1.2", "off", "Left: off");
    act(PC_MENU_RIGHT);
    act(PC_MENU_RIGHT);
    at("Mods", "2. Red Cars 1.2", "off", "on, then off again");
    act(PC_MENU_ACCEPT);   /* picked up */
    act(PC_MENU_UP);
    at("Mods", "1. Red Cars 1.2", "off", "moved up: first, the selection with it");
    act(PC_MENU_UP);
    at("Mods", "1. Red Cars 1.2", "off", "the first stays first");
    act(PC_MENU_ACCEPT);   /* put down */
    act(PC_MENU_DOWN);
    at("Mods", "2. Blue", "on", "then blue: the selection moves again");
    act(PC_MENU_BACK);
    act_n(PC_MENU_DOWN, 4);
    at("Settings", "Save to rush2049pc.ini", "2 changes", "two changes: red's flag and the order");
    act(PC_MENU_ACCEPT);
    text = read_text(path);
    CHECK(text && strstr(text, "mods_order = red, blue, noini\n") && strstr(text, "\n[mods]\nred = off\n"),
          "saved: the order and red's flag:\n%s", text ? text : "(none)");
    act_n(PC_MENU_UP, 4);
    act(PC_MENU_ACCEPT);
    act(PC_MENU_PAGE_DOWN);
    at("Mods", "Defaults", NULL, "a page down: Defaults");
    act(PC_MENU_ACCEPT);
    act(PC_MENU_PAGE_UP);
    act(PC_MENU_DOWN);
    at("Mods", "1. Red Cars 1.2", "on", "Defaults: every mod on, in mod.ini's order (red's priority first)");
    act(PC_MENU_DOWN);
    at("Mods", "2. Blue", "on", "then blue");
    stop();
    pc_menu_set_mods(NULL, 0, "");
}

int main(int argc, char **argv)
{
    static const struct { const char *name; void (*fn)(void); } cases[] = {
        {"nav", test_nav}, {"live", test_live}, {"save", test_save}, {"script", test_script},
        {"mouse", test_mouse}, {"pages", test_pages}, {"mods", test_mods}, {"saves", test_saves_page},
        {"pictures", pictures}};
    if (argc < 3) {
        fprintf(stderr, "usage: frontend_menu_test nav|live|save|script|mouse|pages|saves|pictures SCRATCH_DIR\n");
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
