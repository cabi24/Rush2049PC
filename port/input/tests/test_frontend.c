/* port/input/tests/test_frontend.c: the settings and the stick math, without SDL (docs/port/FRONTEND.md).
 *
 *   frontend_test ini|settings|layers|locate|stick SCRATCH_DIR */
#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
#include <dirent.h>
#endif

#include "config.h"
#include "input_internal.h"
#include "pc_compat.h"

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

/* ---------------------------------------------------------------- ini */

typedef struct {
    char got[32][3][64];
    int lines[32], n, bad[8], nbad;
} Seen;

static int seen_entry(void *user, const char *section, const char *key, const char *value, int line)
{
    Seen *s = user;
    if (s->n < 32) {
        snprintf(s->got[s->n][0], 64, "%s", section);
        snprintf(s->got[s->n][1], 64, "%s", key);
        snprintf(s->got[s->n][2], 64, "%s", value);
        s->lines[s->n++] = line;
    }
    return 0;
}

static void seen_bad(void *user, int line, const char *why)
{
    Seen *s = user;
    if (s->nbad < 8)
        s->bad[s->nbad++] = line;
}

static void expect(const Seen *s, int i, const char *section, const char *key, const char *value, int line)
{
    CHECK(i < s->n, "entry %d missing", i);
    if (i >= s->n)
        return;
    CHECK(!strcmp(s->got[i][0], section) && !strcmp(s->got[i][1], key) && !strcmp(s->got[i][2], value) &&
              s->lines[i] == line,
          "entry %d: [%s] %s = '%s' (line %d), expected [%s] %s = '%s' (line %d)", i, s->got[i][0], s->got[i][1],
          s->got[i][2], s->lines[i], section, key, value, line);
}

static void test_ini(void)
{
    const char *text = "\xEF\xBB\xBF"                       /* 1: BOM, then a key before any section */
                       "top = 1\r\n"
                       "; comment\n"                        /* 2 */
                       "  # another\n"                      /* 3 */
                       "\n"                                 /* 4 */
                       "[ Video ]  ; trailing comment\n"    /* 5 */
                       "Width=1280\n"                       /* 6 */
                       "  height   =   960   \n"            /* 7 */
                       "path = a;b # a comment\n"           /* 8: ; and # need whitespace before them */
                       "spaced = \"  keep ; this  \"  ; x\n" /* 9: quotes keep everything */
                       "empty =\n"                          /* 10 */
                       "no equals sign\n"                   /* 11: bad */
                       "[broken\n"                          /* 12: bad */
                       "= value\n"                          /* 13: bad (no key) */
                       "q = \"unterminated\n"               /* 14: bad */
                       "[keyboard]\n"                       /* 15 */
                       "A = X, Left Shift\r\n"              /* 16 */
                       "last = 9";                          /* 17: no final newline */
    Seen s;
    memset(&s, 0, sizeof s);
    int nbad = pc_ini_parse(text, seen_entry, seen_bad, &s);
    CHECK(nbad == 4 && s.nbad == 4, "%d malformed lines, expected 4", nbad);
    CHECK(s.bad[0] == 11 && s.bad[1] == 12 && s.bad[2] == 13 && s.bad[3] == 14, "bad lines %d %d %d %d", s.bad[0],
          s.bad[1], s.bad[2], s.bad[3]);
    CHECK(s.n == 8, "%d entries, expected 8", s.n);
    expect(&s, 0, "", "top", "1", 1);
    expect(&s, 1, "video", "width", "1280", 6);
    expect(&s, 2, "video", "height", "960", 7);
    expect(&s, 3, "video", "path", "a;b", 8);
    expect(&s, 4, "video", "spaced", "  keep ; this  ", 9);
    expect(&s, 5, "video", "empty", "", 10);
    expect(&s, 6, "keyboard", "a", "X, Left Shift", 16);
    expect(&s, 7, "keyboard", "last", "9", 17);
}

/* ---------------------------------------------------------------- settings */

static void test_settings(void)
{
    PcSettings s, t;
    char why[200];
    pc_settings_defaults(&s);
    CHECK(!strcmp(s.rom, "baserom.us.z64") && s.rom_check == 1 && s.expansion_pak == 0, "game defaults");
    CHECK(s.pak == PC_CFG_PAK_MEMORY && !strcmp(s.pak_file, "rush2049pc.mpk"), "pak defaults");
    CHECK(s.width == 640 && s.height == 480 && !s.fullscreen && s.vsync && s.scale == 1, "video defaults");
    CHECK(s.api == PC_CFG_API_GLES && !s.widescreen && !s.pause_on_focus_loss && !s.screenshot_dir[0], "video");
    CHECK(!strcmp(s.audio, "auto"), "audio default");
    CHECK(s.keyboard_player == 1 && s.first_controller_player == 1, "players");
    CHECK(fabs(s.deadzone - 0.15) < 1e-9 && fabs(s.axis_threshold - 0.5) < 1e-9 && s.stick_range == 80 &&
              s.stick_diagonal == 70 && s.rumble == 1.0,
          "input defaults");
    CHECK(!strcmp(s.key[PC_CTL_A], "X") && !strcmp(s.key[PC_CTL_START], "Return") &&
              !strcmp(s.key[PC_CTL_STICK_LEFT], "Left"),
          "keyboard defaults");
    CHECK(!strcmp(s.pad[PC_CTL_A], "a, righttrigger") && !strcmp(s.pad[PC_CTL_B], "x, lefttrigger") &&
              s.analog == PC_CFG_STICK_LEFT && !s.pad[PC_CTL_STICK_UP][0],
          "controller defaults");
    CHECK(!strcmp(s.hotkey[PC_HOTKEY_FULLSCREEN], "F11") && !strcmp(s.hotkey[PC_HOTKEY_SCREENSHOT], "F12"),
          "hotkeys");

    /* every type, case-insensitive names, bad values leave the setting alone */
    CHECK(pc_settings_set(&s, "VIDEO", "Scale", "4", why, sizeof why) == 0 && s.scale == 4, "int");
    CHECK(pc_settings_set(&s, "video", "scale", "9", why, sizeof why) == -2 && s.scale == 4, "int range: %s", why);
    CHECK(pc_settings_set(&s, "video", "scale", "2x", why, sizeof why) == -2 && s.scale == 4, "int junk");
    {
        char v[16];
        CHECK(pc_settings_set(&s, "video", "scale", "Auto", why, sizeof why) == 0 && s.scale == 0 &&
                  pc_settings_get(&s, "video", "scale", v, sizeof v) == 0 && !strcmp(v, "auto"),
              "scale auto");
        CHECK(pc_settings_set(&s, "video", "scale", "0", why, sizeof why) == -2 && s.scale == 0, "scale 0 is no scale");
        CHECK(pc_settings_set(&s, "video", "scale", "4", why, sizeof why) == 0 && s.scale == 4 &&
                  pc_settings_get(&s, "video", "scale", v, sizeof v) == 0 && !strcmp(v, "4"),
              "scale back to 4");
        CHECK(pc_settings_set(&s, "input", "stick_diagonal", "auto", why, sizeof why) == -2, "auto only for scale");
    }
    CHECK(pc_settings_set(&s, "video", "vsync", "OFF", why, sizeof why) == 0 && !s.vsync, "bool off");
    CHECK(pc_settings_set(&s, "video", "vsync", "yes", why, sizeof why) == 0 && s.vsync, "bool yes");
    CHECK(pc_settings_set(&s, "video", "vsync", "maybe", why, sizeof why) == -2 && s.vsync, "bool bad");
    CHECK(pc_settings_set(&s, "input", "deadzone", "0.25", why, sizeof why) == 0 && s.deadzone == 0.25, "real");
    CHECK(pc_settings_set(&s, "input", "deadzone", "0.95", why, sizeof why) == -2 && s.deadzone == 0.25, "real range");
    CHECK(pc_settings_set(&s, "game", "pak", "Rumble", why, sizeof why) == 0 && s.pak == PC_CFG_PAK_RUMBLE, "enum");
    CHECK(pc_settings_set(&s, "game", "pak", "eeprom", why, sizeof why) == -2 && s.pak == PC_CFG_PAK_RUMBLE &&
              strstr(why, "none, memory, rumble"),
          "enum bad: %s", why);
    CHECK(pc_settings_set(&s, "video", "api", "gl", why, sizeof why) == 0 && s.api == PC_CFG_API_GL, "api");
    {   /* an aspect: off, auto, W:H or a ratio from 4:3 to 4, written back by name */
        static const struct { const char *in, *out; } wide[] = {
            {"16:9", "16:9"}, {"32:18", "16:9"}, {"1.6", "16:10"}, {"21:9", "21:9"}, {"2.39", "2.39"},
            {"3:2", "3:2"}, {"AUTO", "auto"}, {"4:3", "off"}, {"off", "off"}};
        char v[32];
        for (size_t i = 0; i < sizeof wide / sizeof *wide; i++) {
            int ok = pc_settings_set(&s, "video", "widescreen", wide[i].in, why, sizeof why) == 0;
            pc_settings_get(&s, "video", "widescreen", v, sizeof v);
            CHECK(ok && !strcmp(v, wide[i].out), "widescreen %s: %s", wide[i].in, ok ? v : why);
        }
        pc_settings_set(&s, "video", "widescreen", "16:9", why, sizeof why);
        CHECK(fabs(s.widescreen - 16.0 / 9.0) < 1e-9, "widescreen value");
        CHECK(pc_settings_set(&s, "video", "widescreen", "5:4", why, sizeof why) == -2 &&
                  fabs(s.widescreen - 16.0 / 9.0) < 1e-9,
              "widescreen narrower than 4:3: %s", why);
        CHECK(pc_settings_set(&s, "video", "widescreen", "on", why, sizeof why) == -2, "widescreen on is no aspect");
        CHECK(pc_settings_set(&s, "video", "widescreen", "16:9:1", why, sizeof why) == -2, "widescreen junk");
        CHECK(pc_settings_set(&s, "video", "widescreen", "5", why, sizeof why) == -2, "widescreen past 4");
    }
    CHECK(pc_settings_set(&s, "game", "rom", "/roms/my rom.z64", why, sizeof why) == 0 &&
              !strcmp(s.rom, "/roms/my rom.z64"),
          "string");
    CHECK(pc_settings_set(&s, "video", "frobnicate", "1", why, sizeof why) == -1, "unknown key");
    CHECK(pc_settings_set(&s, "nowhere", "scale", "1", why, sizeof why) == -1, "unknown section");
    CHECK(pc_settings_set(&s, "keyboard", "cup", "Up, Keypad 8", why, sizeof why) == 0 &&
              !strcmp(s.key[PC_CTL_CUP], "Up, Keypad 8"),
          "binding");
    CHECK(pc_settings_set(&s, "keyboard", "C_UP", "I", why, sizeof why) == -1, "unknown control");
    CHECK(pc_settings_set(&s, "controller", "analog", "right", why, sizeof why) == 0 && s.analog == PC_CFG_STICK_RIGHT,
          "analog");
    CHECK(pc_settings_set(&s, "controller", "L", "", why, sizeof why) == 0 && !s.pad[PC_CTL_L][0], "unbound");
    CHECK(pc_settings_set(&s, "hotkeys", "quit", "", why, sizeof why) == 0 && !s.hotkey[PC_HOTKEY_QUIT][0], "hotkey");
    CHECK(pc_settings_set(&s, "hotkeys", "explode", "F1", why, sizeof why) == -1, "unknown hotkey");
    CHECK(pc_settings_assign(&s, "audio.backend = wav", why, sizeof why) == 0 && !strcmp(s.audio, "wav"), "assign");
    CHECK(pc_settings_assign(&s, "audio=wav", why, sizeof why) == -3, "assign without a dot");
    CHECK(pc_settings_assign(&s, "keyboard.A=Space", why, sizeof why) == 0 && !strcmp(s.key[PC_CTL_A], "Space"),
          "assign binding");

    /* the written file reads back to the same settings, odd strings included */
    pc_settings_set(&s, "video", "screenshot_dir", " /tmp/a dir ; x ", why, sizeof why);
    pc_settings_set(&s, "game", "pak_file", "#hash.mpk", why, sizeof why);
    char *text = pc_settings_format(&s);
    pc_settings_defaults(&t);
    int problems = pc_settings_load_text(&t, text, "formatted");
    char *again = pc_settings_format(&t);
    CHECK(problems == 0, "%d problems reading the formatted file", problems);
    CHECK(!strcmp(text, again), "round trip differs");
    CHECK(!strcmp(t.screenshot_dir, " /tmp/a dir ; x ") && !strcmp(t.pak_file, "#hash.mpk") && t.scale == 4 &&
              t.deadzone == 0.25 && t.analog == PC_CFG_STICK_RIGHT && !t.hotkey[PC_HOTKEY_QUIT][0],
          "round trip values");
    free(text);
    free(again);

    /* a bad line is skipped and counted, and the rest still applies */
    pc_settings_defaults(&t);
    problems = pc_settings_load_text(&t, "[video]\nscale = 99\nwidth = 800\nnonsense\n[keyboard]\nQQ = X\n", "bad");
    CHECK(problems == 3 && t.scale == 1 && t.width == 800, "bad lines: %d problems, scale %d width %d", problems,
          t.scale, t.width);
}

/* ---------------------------------------------------------------- layers */

static void write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    CHECK(f != NULL, "cannot write %s", path);
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

static void test_layers(void)
{
    char path[1024];
    PcSettings s;
    snprintf(path, sizeof path, "%s/layers.ini", g_dir);
    write_text(path, "[game]\nrom = /from/file.z64\n[video]\nscale = 2\napi = gl\n[audio]\nbackend = wav\n"
                     "[input]\ndeadzone = 0.3\n");
    unsetenv("RUSH2049_AUDIO");
    unsetenv("RUSH_GFX_SCALE");
    unsetenv("RUSH_GFX_API");

    /* file over the defaults */
    CHECK(pc_settings_resolve(&s, PC_CFG_FILE_READ, path, NULL, NULL, 0) == 0, "resolve");
    CHECK(!strcmp(s.rom, "/from/file.z64") && s.scale == 2 && s.api == PC_CFG_API_GL && !strcmp(s.audio, "wav") &&
              s.deadzone == 0.3 && s.width == 640,
          "file layer");
    CHECK(!strcmp(s.source, path), "source %s", s.source);

    /* the environment over the file */
    setenv("RUSH2049_AUDIO", "null", 1);
    setenv("RUSH_GFX_SCALE", "4", 1);
    setenv("RUSH_GFX_API", "bogus", 1);   /* bad: reported and ignored */
    pc_settings_resolve(&s, PC_CFG_FILE_READ, path, NULL, NULL, 0);
    CHECK(!strcmp(s.audio, "null") && s.scale == 4 && s.api == PC_CFG_API_GL, "environment layer: %s %d %d", s.audio,
          s.scale, s.api);

    /* the command line over both, in order (the last of two wins) */
    PcSettingValue over[] = {{"video", "scale", "3"}, {"game", "pak", "none"}, {"game", "pak", "rumble"},
                             {"audio", "backend", "sdl"}};
    CHECK(pc_settings_resolve(&s, PC_CFG_FILE_READ, path, NULL, over, 4) == 0, "resolve overrides");
    CHECK(s.scale == 3 && s.pak == PC_CFG_PAK_RUMBLE && !strcmp(s.audio, "sdl") && !strcmp(s.rom, "/from/file.z64"),
          "override layer");

    /* a bad override is an error; the good ones still apply */
    PcSettingValue bad[] = {{"video", "scale", "0"}, {"video", "width", "1024"}};
    CHECK(pc_settings_resolve(&s, PC_CFG_FILE_READ, path, NULL, bad, 2) == -1 && s.width == 1024 && s.scale == 4,
          "bad override");

    /* no file: defaults and environment only */
    pc_settings_resolve(&s, PC_CFG_FILE_NONE, path, NULL, NULL, 0);
    CHECK(!strcmp(s.rom, "baserom.us.z64") && s.scale == 4 && !s.source[0], "no file");
    unsetenv("RUSH2049_AUDIO");
    unsetenv("RUSH_GFX_SCALE");
    unsetenv("RUSH_GFX_API");

    /* a missing explicit file: defaults, not created */
    snprintf(path, sizeof path, "%s/missing.ini", g_dir);
    unlink(path);
    pc_settings_resolve(&s, PC_CFG_FILE_READ, path, NULL, NULL, 0);
    CHECK(s.scale == 1 && access(path, F_OK) != 0, "missing explicit file");
}

/* ---------------------------------------------------------------- locate and first run */

#ifdef _WIN32
/* No rm on Windows: the tree, by hand. */
static void rmrf(const char *path)
{
    char sub[2048];
    struct stat st;
    DIR *d = opendir(path);
    struct dirent *e;
    while (d && (e = readdir(d)))
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) {
            snprintf(sub, sizeof sub, "%s/%s", path, e->d_name);
            if (stat(sub, &st) == 0 && S_ISDIR(st.st_mode))
                rmrf(sub);
            else
                unlink(sub);
        }
    if (d)
        closedir(d);
    rmdir(path);
}
#else
static void rmrf(const char *path)
{
    char cmd[2048];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", path);
    if (system(cmd) != 0)
        fprintf(stderr, "cannot remove %s\n", path);
}
#endif

static void test_locate(void)
{
    char xdg[1024], home[1024], exe[1024], path[1100], want[1100];
    int exists;
    PcSettings s;
    snprintf(xdg, sizeof xdg, "%s/xdg", g_dir);
    snprintf(home, sizeof home, "%s/home", g_dir);
    snprintf(exe, sizeof exe, "%s/exe", g_dir);
    rmrf(xdg);
    rmrf(home);
    rmrf(exe);
    mkdir(exe, 0755);
    mkdir(home, 0755);
#ifdef _WIN32
    /* Windows: %APPDATA%\rush2049pc (the xdg directory stands in for it); XDG_CONFIG_HOME and HOME don't count */
    setenv("APPDATA", xdg, 1);
    setenv("XDG_CONFIG_HOME", home, 1);
    CHECK(pc_config_locate(path, sizeof path, exe, &exists) == 0, "locate");
    snprintf(want, sizeof want, "%s/rush2049pc/rush2049pc.ini", xdg);
    CHECK(!strcmp(path, want) && !exists, "first location %s (%d)", path, exists);
    unsetenv("APPDATA");
    CHECK(pc_config_locate(path, sizeof path, exe, &exists) == -1, "no place at all");
    unsetenv("XDG_CONFIG_HOME");
    setenv("APPDATA", xdg, 1);
#else
    setenv("XDG_CONFIG_HOME", xdg, 1);
    setenv("HOME", home, 1);

    /* nothing yet: the XDG place, not there */
    CHECK(pc_config_locate(path, sizeof path, exe, &exists) == 0, "locate");
    snprintf(want, sizeof want, "%s/rush2049pc/rush2049pc.ini", xdg);
    CHECK(!strcmp(path, want) && !exists, "first location %s (%d)", path, exists);

    /* a relative XDG_CONFIG_HOME is ignored (the spec): ~/.config */
    setenv("XDG_CONFIG_HOME", "relative/dir", 1);
    pc_config_locate(path, sizeof path, exe, &exists);
    snprintf(want, sizeof want, "%s/.config/rush2049pc/rush2049pc.ini", home);
    CHECK(!strcmp(path, want), "relative XDG: %s", path);
    unsetenv("XDG_CONFIG_HOME");
    pc_config_locate(path, sizeof path, exe, &exists);
    CHECK(!strcmp(path, want), "no XDG: %s", path);
    unsetenv("HOME");
    CHECK(pc_config_locate(path, sizeof path, exe, &exists) == -1, "no place at all");
    setenv("HOME", home, 1);
    setenv("XDG_CONFIG_HOME", xdg, 1);
#endif

    /* first run: the file is written with the defaults (directories made) and read back */
    CHECK(pc_settings_resolve(&s, PC_CFG_FILE_CREATE, NULL, exe, NULL, 0) == 0, "first run");
    snprintf(want, sizeof want, "%s/rush2049pc/rush2049pc.ini", xdg);
    CHECK(access(want, R_OK) == 0 && !strcmp(s.source, want), "first-run file %s", s.source);
    PcSettings d;
    pc_settings_defaults(&d);
    char *a = pc_settings_format(&s), *b = pc_settings_format(&d);
    CHECK(!strcmp(a, b), "first-run settings are the defaults");
    free(a);
    free(b);

    /* the next run reads it, edits included; READ never creates */
    write_text(want, "[video]\nfullscreen = on\n");
    pc_settings_resolve(&s, PC_CFG_FILE_CREATE, NULL, exe, NULL, 0);
    CHECK(s.fullscreen == 1, "edited file read");
    unlink(want);
    pc_settings_resolve(&s, PC_CFG_FILE_READ, NULL, exe, NULL, 0);
    CHECK(access(want, F_OK) != 0 && !s.source[0], "READ does not create");

    /* a file next to the executable wins (a portable install) */
    snprintf(path, sizeof path, "%s/rush2049pc.ini", exe);
    write_text(path, "[video]\nwidth = 1600\n");
    pc_config_locate(want, sizeof want, exe, &exists);
    CHECK(!strcmp(want, path) && exists, "exe dir file: %s", want);
    pc_settings_resolve(&s, PC_CFG_FILE_CREATE, NULL, exe, NULL, 0);
    CHECK(s.width == 1600 && !strcmp(s.source, path) && !strcmp(s.source_dir, exe), "exe dir file read");

    /* pc_config_exe_dir: this program's directory */
    char self[1024];
    pc_config_exe_dir(self, sizeof self, NULL);
#ifdef _WIN32
    CHECK(self[0] && self[1] == ':' && access(self, F_OK) == 0, "exe dir %s", self);
#else
    CHECK(self[0] == '/' && access(self, X_OK) == 0, "exe dir %s", self);
#endif
}

/* ---------------------------------------------------------------- stick */

static void stick(double x, double y, const PcStickShape *sh, int ex, int ey, const char *what)
{
    int8_t ox, oy;
    pc_stick_shape(x, y, sh, &ox, &oy);
    CHECK(ox == ex && oy == ey, "%s: (%.3f, %.3f) -> (%d, %d), expected (%d, %d)", what, x, y, ox, oy, ex, ey);
}

static void test_stick(void)
{
    PcStickShape oct = {0.15, 80, 70}, round = {0.15, 80, 0}, digital = {0, 80, 70};
    stick(0, 0, &oct, 0, 0, "center");
    stick(0.1, 0.1, &oct, 0, 0, "inside the dead zone (radial: |v| = 0.141)");
    stick(0.14, 0, &oct, 0, 0, "dead zone edge");
    stick(1, 0, &oct, 80, 0, "right");
    stick(-1, 0, &oct, -80, 0, "left");
    stick(0, 1, &oct, 0, 80, "up");
    stick(0, -1, &oct, 0, -80, "down");
    stick(0.7071068, 0.7071068, &oct, 70, 70, "full diagonal: the octagon's corner");
    stick(-0.7071068, -0.7071068, &oct, -70, -70, "full diagonal, down left");
    stick(1, 1, &oct, 70, 70, "a square pad's corner clamps to full tilt");
    stick(2, 0, &oct, 80, 0, "beyond full tilt");
    stick(0.575, 0, &oct, 40, 0, "half the travel past the dead zone");
    /* 22.5 degrees: on the octagon's edge (80 - 10u, 70u) between (80, 0) and (70, 70); the direction meets it
     * at distance 80 / (cos + sin / 7) = 81.75, (75.53, 31.28) */
    stick(cos(M_PI / 8), sin(M_PI / 8), &oct, 76, 31, "22.5 degrees on the edge");
    stick(0.7071068, 0.7071068, &round, 57, 57, "round gate diagonal");
    stick(1, 0, &round, 80, 0, "round gate right");
    stick(1, 1, &digital, 70, 70, "two keys");
    stick(-1, 0, &digital, -80, 0, "one key");
    PcStickShape big = {0, 127, 0};
    stick(1, 0, &big, 127, 0, "127");
    /* past the dead zone the value starts small, not with a jump */
    int8_t ox, oy;
    pc_stick_shape(0.16, 0, &oct, &ox, &oy);
    CHECK(ox >= 0 && ox <= 2, "just past the dead zone: %d", ox);
    /* monotonic along an axis */
    int last = -1, mono = 1;
    for (int i = 0; i <= 100; i++) {
        pc_stick_shape(i / 100.0, 0, &oct, &ox, &oy);
        mono &= ox >= last;
        last = ox;
    }
    CHECK(mono && last == 80, "monotonic");
    CHECK(pc_axis_deadzone(0.1, 0.15) == 0 && fabs(pc_axis_deadzone(-0.575, 0.15) + 0.5) < 1e-9 &&
              pc_axis_deadzone(1.5, 0.15) == 1,
          "axis dead zone");
}

int main(int argc, char **argv)
{
    static const struct { const char *name; void (*fn)(void); } cases[] = {
        {"ini", test_ini}, {"settings", test_settings}, {"layers", test_layers}, {"locate", test_locate},
        {"stick", test_stick}};
    if (argc < 3) {
        fprintf(stderr, "usage: frontend_test ini|settings|layers|locate|stick|all SCRATCH_DIR\n");
        return 2;
    }
    static char dir[1024];
    mkdir(argv[2], 0755);
#ifdef _WIN32
    if (!_fullpath(dir, argv[2], sizeof dir)) {
#else
    if (!realpath(argv[2], dir)) {   /* absolute: a relative XDG_CONFIG_HOME is ignored */
#endif
        perror(argv[2]);
        return 2;
    }
    g_dir = dir;
    int ran = 0;
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++)
        if (!strcmp(argv[1], "all") || !strcmp(argv[1], cases[i].name)) {
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
