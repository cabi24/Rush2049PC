/* port/main.c: PC entry point (docs/PORT_PLAN.md, docs/port/ULTRA.md, docs/port/FRONTEND.md).
 *
 * Resolves the settings (defaults, rush2049pc.ini, the environment, a press file's `pak` line, then the
 * command line: port/config), loads and checks the ROM, maps the RDRAM window, fills the game's initialized data
 * (port/data), opens the renderer (port/gfx) and live input (port/input), then starts the game's own main
 * (0x800020F0, compiled as n64_main) on an N64 thread and runs the host main loop (port/ultra/loop.c) until the
 * window closes or a limit is reached. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app.h"
#include "config.h"
#include "pc_compat.h"
#include "ultra.h"

extern void n64_main(void *argument);   /* src/rom/lib_2cf0.c */
int pc_selftest(void);                  /* port/ultra/selftest.c */

static void usage(FILE *f)
{
    fprintf(f,
            "usage: rush2049pc [options] [ROM]\n"
            "Settings come from rush2049pc.ini (next to the program, else the user settings folder; written with\n"
            "the defaults on first run), then the environment, then these options.\n"
            "  --rom PATH, ROM     the San Francisco Rush 2049 (USA) ROM, .z64/.v64/.n64. Without one, game.rom\n"
            "                      (default baserom.us.z64) is looked for next to the program, in the user data\n"
            "                      folder, then in the working directory\n"
            "  --no-rom-check      run a ROM whose SHA-1 differs\n"
            "  --no-rom            bring-up: a blank cartridge instead of the ROM (every ROM read is zero)\n"
            "  --headless          render off screen (EGL surfaceless, WGL on Windows; on Linux the default\n"
            "                      without DISPLAY or WAYLAND_DISPLAY); --window: an SDL2 window\n"
            "  --fullscreen        start in fullscreen (Alt+Enter or F11 toggles); --exclusive: the display's mode\n"
            "                      changes to --resolution (desktop, 720p, 1080p, 1440p, 1620p, 2160p or WxH)\n"
            "  --size WxH          window size in pixels (default: two thirds of the desktop; headless 320x240)\n"
            "  --scale N           internal resolution: native (the window's pixels, the default), auto (the\n"
            "                      largest whole multiple of the N64's), or a multiple, 1-8 (1: the N64's)\n"
            "  --widescreen A      widen the picture to an aspect: off, auto (the window's), 16:9, 21:9, W:H...\n"
            "  --frame-rate R      frames shown a second: original (the game's own), display, or 60, 120...;\n"
            "                      the frames between the game's are drawn from its last two\n"
            "  --seconds N         stop after N seconds (of game time with --fast)\n"
            "  --fast              run as fast as the host can: the N64 clock moves on only while every N64\n"
            "                      thread waits, so a run is the same on any host (tests, scripted runs)\n"
            "  --frames N          stop after N graphics tasks (at the retrace that shows the last)\n"
            "  --screenshot-at N   save a PNG once N graphics tasks have run (--screenshot PATH, default\n"
            "                      rush2049pc-N.png)\n"
            "  --stop-at-gfx       stop at the first graphics task (osSpTaskStartGo), reporting it\n"
            "  --until-state M[:N] stop once the game's state word (gGameState) has a bit of M and N more\n"
            "                      graphics tasks have run; with --seconds, not getting there is exit code 1\n"
            "  --press T:BUTTON[@PORT][:DUR],...   scripted input (v5's format; T, DUR in seconds)\n"
            "  --press-file PATH   a .press input script\n"
            "  --log-input         print every change of a player's buttons and stick\n"
            "  --press-via K[:T]   the scripts through SDL as live input (tests): controller or joystick (virtual\n"
            "                      ones, plugged in at T seconds), or keyboard; with a window or --headless\n"
            "  --pak PATH          controller pak image for controller 1 (default rush2049pc.mpk in the data\n"
            "                      folder: next to the program when rush2049pc.ini is there, else the user data\n"
            "                      folder; it also holds screenshots/ and rush2049pc.log)\n"
            "  --no-pak            no accessory; --rumble: a rumble pak instead\n"
            "  --expansion-pak     report 8 MiB of RDRAM, the Expansion Pak (the default; game.expansion_pak=off: 4 MiB)\n"
            "  --audio BACKEND     auto (default), sdl, alsa, null or wav (port/audio)\n"
            "  --dump-audio PATH   also write everything the game plays to a WAV file\n"
            "  --config PATH       read this settings file instead (it is not created); --no-config: none\n"
            "  --set S.KEY=VALUE   any setting, as the file names it (video.vsync=off, keyboard.A=Space)\n"
            "  --print-config      print the settings in effect, as a settings file, and exit\n"
            "  --status N          a status line every N seconds\n"
            "  -v, --verbose       more log lines (repeat for more); --trace-os: every thread switch\n"
            "  --log PATH          write the run's output to PATH (default rush2049pc.log where the program keeps\n"
            "                      its files; the run before is kept as rush2049pc.old.log); --no-log: none\n"
            "  --selftest          test the thread scheduler, queues, events and timers (no ROM needed)\n"
            "  --version           print the version and the build, and exit\n"
            "F1 (or both stick clicks on a controller) opens the settings overlay, which holds the game; a\n"
            "--press script's MENU does too. Ctrl-\\ prints the N64 threads' states.\n");
}

/* --no-rom: the file table (docs/port/ASSETS.md §2.2) is ROM data, so on a blank cartridge every wave is a
 * zero-size file at ROM offset 0. A zero-size heap block makes the file alias the next block's header, which the
 * text bank's fixups then overwrite. Instead every wave becomes a small raw file of zeros past the cartridge
 * header; the converters turn the zeros into empty files where the game would loop on them (port/assets). */
#define BLANK_WAVES 183
#define BLANK_FILE_SIZE 0x200u
#define BLANK_FILE_ROM 0x1000u
extern uint32_t D_8002E580[BLANK_WAVES];   /* gWaveSizes */
extern uint32_t D_8011B5BC[BLANK_WAVES];   /* ROM offsets */
extern int32_t D_80123564[BLANK_WAVES];    /* storage kind: 0 raw */

static void blank_files(void)
{
    for (int i = 0; i < BLANK_WAVES; i++)
        D_8002E580[i] = BLANK_FILE_SIZE, D_8011B5BC[i] = BLANK_FILE_ROM, D_80123564[i] = 0;
}

/* The MusyX service table (docs/port/MUSYX_DATA.md 8.1) is game data too, so a blank cartridge has none and
 * sndInit's first allocation calls 0. Its eight slots are the eight functions at the end of the game's code
 * (0x8010FBB4-0x8010FD7C), which nothing calls directly; their order here follows how the library calls each
 * slot. With a ROM the data loader fills the table, and a difference from this list is logged. */
extern void (*D_8011EAAC[8])(void);
extern void func_8010FBE0(void *task);                              /* 0: submit the audio task */
extern void sync_acquire_menu(void);                                /* 1: wait for it */
extern int32_t func_8010FC60(intptr_t callback);                    /* 2: set the retrace callback */
extern void func_8010FC70(int32_t unused);                          /* 3: clear it */
extern int32_t struct_init_and_call(intptr_t list, int32_t count, intptr_t done);  /* 4: DMA list */
extern intptr_t func_8010FD60(intptr_t address);                    /* 5: cartridge address */
extern void *resource_alloc_init(int32_t size, int32_t mode);       /* 6: allocate */
extern void synced_model_render(uint32_t address);                  /* 7: free */

static void musyx_services(int no_rom)
{
    void (*const known[8])(void) = {
        (void (*)(void))func_8010FBE0,  (void (*)(void))sync_acquire_menu,    (void (*)(void))func_8010FC60,
        (void (*)(void))func_8010FC70,  (void (*)(void))struct_init_and_call, (void (*)(void))func_8010FD60,
        (void (*)(void))resource_alloc_init, (void (*)(void))synced_model_render,
    };
    for (int i = 0; i < 8; i++) {
        if (no_rom)
            D_8011EAAC[i] = known[i];
        else if (D_8011EAAC[i] != known[i])
            fprintf(stderr, "rush2049pc: MusyX service %d is %p in the ROM's table, not %p as the port expects\n", i,
                    (void *)D_8011EAAC[i], (void *)known[i]);
    }
}

/* The window and the display before port/gfx opens them (docs/port/GFX.md "Window and display"). */
static void window_setup(const PcSettings *cfg)
{
    int x, y, w, h;
    char tail;
    pc_gfx_set_fullscreen_mode(cfg->fullscreen_mode);
    if (pc_settings_resolution(cfg->resolution, &w, &h) == 0)
        pc_gfx_set_resolution(w, h);
    if (sscanf(cfg->position, "%d,%d%c", &x, &y, &tail) == 2)
        pc_gfx_set_window_position(x, y);
}

/* video.remember_window: the window's size and place out of fullscreen go into the settings file read at start
 * when they changed (not with --no-config, nor headless). */
static void remember_window(const PcSettings *cfg, int headless)
{
    static char w_text[16], h_text[16], pos[32];
    char why[PC_CFG_PATH + 64];
    PcSettingValue ch[3];
    int n = 0, x, y, w, h;
    if (headless || !cfg->remember_window || !cfg->source[0] || pc_gfx_window_geometry(&x, &y, &w, &h) != 0)
        return;
    snprintf(w_text, sizeof w_text, "%d", w);
    snprintf(h_text, sizeof h_text, "%d", h);
    snprintf(pos, sizeof pos, "%d,%d", x, y);
    if (w != cfg->width || h != cfg->height) {
        ch[n++] = (PcSettingValue){"video", "width", w_text};
        ch[n++] = (PcSettingValue){"video", "height", h_text};
    }
    if (strcmp(pos, cfg->position))
        ch[n++] = (PcSettingValue){"video", "position", pos};
    if (n && pc_settings_save(cfg->source, ch, n, why, sizeof why) != 0)
        fprintf(stderr, "rush2049pc: the window's size and place not kept: %s\n", why);
    else if (n)
        fprintf(stderr, "rush2049pc: window %d x %d at %s kept in %s\n", w, h, pos, cfg->source);
}

/* The game's state word (gGameState, groups/state/state.c), which the host loop logs and --until-state waits
 * for: 1 initGameState, 2 the start prompt, 0x40000000 the pak warning, 4 the front end's menus, ... */
extern uint32_t state_word_a;

static uint32_t game_state(void)
{
    return *(volatile uint32_t *)&state_word_a;
}

/* What the state log adds to each change: who plays what (gNumPlayers, gNumWindows, the cars in the race, gRace,
 * gTrackNum, the pause screen's state) and where the first car is (its model record's RWR and the length of RWV, in feet and feet a second;
 * and its steering input, -1..1),
 * so a scripted run shows which path it took and whether the car moved. */
extern int16_t active_player_count, gNumWindows;
extern int32_t gameplay_mode;
extern int8_t D_80152744, D_8014978C;
extern uint8_t D_8014A250[];   /* the model records (0x808 bytes, N64 layout): RWV at +0x220, RWR at +0x22C */
extern int32_t D_801170FC;     /* gPauseState: 0 racing, 1 the pause menu, 2-8 its screens */

static void game_info(char *out, size_t size)
{
    /* RUSH_STATE_EXACT: the position and speed as exact hex floats, so two runs compare bit for bit
     * (tools/port/synth_compare.py, docs/port/OPTIMIZED.md). */
    static int exact = -1;
    if (exact < 0)
        exact = getenv("RUSH_STATE_EXACT") != NULL;
    const volatile float *v = (const volatile float *)(D_8014A250 + 0x220), *r = v + 3;
    const volatile float *steer = (const volatile float *)(D_8014A250 + 0x720);   /* its controls: steer -1..1 */
    double speed = sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]);
    snprintf(out, size, exact ? "players %d, windows %d, cars %d, mode %d, track %d, pause %d; car 0 at %a %a %a, "
             "speed %a, steer %a" : "players %d, windows %d, cars %d, mode %d, track %d, pause %d; car 0 at %.0f %.0f "
             "%.0f, speed %.0f, steer %.2f", *(volatile int16_t *)&active_player_count, *(volatile int16_t *)&gNumWindows,
             *(volatile int8_t *)&D_80152744, (int)*(volatile int32_t *)&gameplay_mode, *(volatile int8_t *)&D_8014978C,
             (int)*(volatile int32_t *)&D_801170FC, (double)r[0], (double)r[1], (double)r[2], speed, (double)*steer);
}

/* --fast: the audio output's null backend drains the queue by the N64 clock, as a device would in real time. */
static double fast_clock(void)
{
    return pc_host_ns() / 1e9;
}

/* A portable copy's empty rush2049pc.ini, as the release archives ship it, gets the defaults with their comments,
 * as a first run writes them in the user settings folder (docs/port/RELEASE.md). */
static void fill_portable_ini(const PcAppDirs *d)
{
    char ini[PC_APP_PATH];
    struct stat st;
    pc_app_join(ini, sizeof ini, d->exe, "rush2049pc.ini");
    if (stat(ini, &st) != 0 || st.st_size != 0)
        return;
    static PcSettings def;
    pc_settings_defaults(&def);
    if (pc_settings_write(&def, ini) == 0)
        fprintf(stderr, "rush2049pc: wrote the default settings to %s\n", ini);
    else
        fprintf(stderr, "rush2049pc: cannot write %s; using the defaults\n", ini);
}

/* Whether the command line set this setting (its path is then from the working directory). */
static int from_command_line(const PcSettingValue *over, int n, const char *section, const char *key)
{
    int set = 0;
    for (int i = 1; i < n; i++)
        set |= !strcasecmp(over[i].section, section) && !strcasecmp(over[i].key, key);
    return set;
}

#define MAX_OVERRIDES 64

static int rush_main(int argc, char **argv)
{
    const char *dump_audio = getenv("RUSH2049_DUMP_AUDIO"), *config = NULL;
#ifdef _WIN32
    int headless = 0;   /* Windows has no DISPLAY: a window unless --headless */
#else
    int headless = !getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY");
#endif
    int no_rom = 0, selftest = 0, use_config = 1;
    int print_config = 0, fast = 0;
    const char *bare_rom = NULL, *log_path = NULL;
    int use_log = 1;
    PcRunOptions run = {0};
    PcSettingValue over[MAX_OVERRIDES + 1];   /* the command line's settings, in order; [0] for a press file */
    int nover = 1;
    char width[16], height[16];

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = i + 1 < argc ? argv[i + 1] : NULL;
#define ARG() (v ? (i++, v) : (fprintf(stderr, "rush2049pc: %s needs a value\n", a), pc_app_finish(2), exit(2), ""))
#define SET(s, k, val)                                                                                        \
    do {                                                                                                      \
        if (nover > MAX_OVERRIDES) {                                                                          \
            fprintf(stderr, "rush2049pc: too many settings on the command line\n");                           \
            return 2;                                                                                         \
        }                                                                                                     \
        over[nover++] = (PcSettingValue){s, k, val};                                                          \
    } while (0)
        if (!strcmp(a, "--rom"))
            SET("game", "rom", ARG());
        else if (a[0] != '-' && a[0]) {   /* a ROM's path: what Windows passes for a file dropped on the program */
            if (bare_rom) {
                fprintf(stderr, "rush2049pc: one ROM at a time (%s, then %s)\n", bare_rom, a);
                return 2;
            }
            bare_rom = a;
            SET("game", "rom", a);
        }
        else if (!strcmp(a, "--no-rom-check"))
            SET("game", "rom_check", "off");
        else if (!strcmp(a, "--no-rom"))
            no_rom = 1;
        else if (!strcmp(a, "--headless"))
            headless = 1;
        else if (!strcmp(a, "--window"))
            headless = 0;
        else if (!strcmp(a, "--fullscreen"))
            SET("video", "fullscreen", "on");
        else if (!strcmp(a, "--exclusive")) {
            SET("video", "fullscreen", "on");
            SET("video", "fullscreen_mode", "exclusive");
        } else if (!strcmp(a, "--resolution"))
            SET("video", "resolution", ARG());
        else if (!strcmp(a, "--size")) {
            const char *s = ARG();
            unsigned w, h;
            char tail;
            if (sscanf(s, "%ux%u%c", &w, &h, &tail) != 2) {
                fprintf(stderr, "rush2049pc: --size takes WIDTHxHEIGHT, not '%s'\n", s);
                return 2;
            }
            snprintf(width, sizeof width, "%u", w);
            snprintf(height, sizeof height, "%u", h);
            SET("video", "width", width);
            SET("video", "height", height);
        } else if (!strcmp(a, "--scale"))
            SET("video", "scale", ARG());
        else if (!strcmp(a, "--widescreen"))
            SET("video", "widescreen", ARG());
        else if (!strcmp(a, "--frame-rate"))
            SET("video", "frame_rate", ARG());
        else if (!strcmp(a, "--screenshot-at"))
            run.screenshot_at = (uint32_t)atoi(ARG());
        else if (!strcmp(a, "--screenshot"))
            run.screenshot = ARG();
        else if (!strcmp(a, "--seconds"))
            run.seconds = atof(ARG());
        else if (!strcmp(a, "--fast"))
            fast = 1;
        else if (!strcmp(a, "--frames"))
            run.frames = (uint32_t)atoi(ARG());
        else if (!strcmp(a, "--stop-at-gfx"))
            pc_rcp_stop_at_gfx = 1;
        else if (!strcmp(a, "--until-state")) {
            const char *s = ARG();
            char *end;
            run.until_state = (uint32_t)strtoul(s, &end, 0);
            run.until_state_frames = *end == ':' ? (uint32_t)strtoul(end + 1, &end, 0) : 0;
            if (*end || !run.until_state) {
                fprintf(stderr, "rush2049pc: --until-state takes MASK[:FRAMES], not '%s'\n", s);
                return 2;
            }
        }
        else if (!strcmp(a, "--press")) {
            if (pc_input_add_spec(ARG()) != 0)
                return 2;
        } else if (!strcmp(a, "--press-file")) {
            if (pc_input_add_file(ARG()) != 0)
                return 2;
        } else if (!strcmp(a, "--log-input"))
            pc_input_log = 1;
        else if (!strcmp(a, "--press-via")) {
            if (pc_input_set_via(ARG()) != 0)
                return 2;
        }
        else if (!strcmp(a, "--pak")) {
            SET("game", "pak", "memory");
            SET("game", "pak_file", ARG());
        } else if (!strcmp(a, "--no-pak"))
            SET("game", "pak", "none");
        else if (!strcmp(a, "--rumble"))
            SET("game", "pak", "rumble");
        else if (!strcmp(a, "--audio"))
            SET("audio", "backend", ARG());
        else if (!strcmp(a, "--dump-audio"))
            dump_audio = ARG();
        else if (!strcmp(a, "--expansion-pak"))
            SET("game", "expansion_pak", "on");
        else if (!strcmp(a, "--config"))
            config = ARG(), use_config = 1;
        else if (!strcmp(a, "--no-config"))
            use_config = 0;
        else if (!strcmp(a, "--set")) {
            char *s = strdup(ARG()), *eq = strchr(s, '='), *dot = strchr(s, '.');
            if (!eq || !dot || dot > eq) {
                fprintf(stderr, "rush2049pc: --set takes SECTION.KEY=VALUE, not '%s'\n", v);
                return 2;
            }
            *dot = *eq = 0;
            SET(s, dot + 1, eq + 1);
        } else if (!strcmp(a, "--print-config"))
            print_config = 1;
        else if (!strcmp(a, "--status"))
            run.status_every = atoi(ARG());
        else if (!strcmp(a, "-v") || !strcmp(a, "--verbose"))
            pc_verbose++;
        else if (!strcmp(a, "--trace-os"))
            pc_os_trace = 1;
        else if (!strcmp(a, "--selftest"))
            selftest = 1;
        else if (!strcmp(a, "--log"))
            log_path = ARG();
        else if (!strcmp(a, "--no-log"))
            use_log = 0;
        else if (!strcmp(a, "--version")) {
            char text[512];
            pc_app_version_text(text, sizeof text);
            fputs(text, stdout);
            return 0;
        } else if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
            usage(stdout);
            return 0;
        } else {
            fprintf(stderr, "rush2049pc: unknown option %s\n", a);
            if (pc_app_console_quiet())   /* the error's message box shows the last lines: not the usage's */
                fprintf(stderr, "rush2049pc: rush2049pc --help lists the options\n");
            else
                usage(stderr);
            return 2;
        }
#undef SET
#undef ARG
    }

    /* A press file's `pak on|off` sits between the settings file and the command line. */
    PcSettingValue *first = &over[1];
    if (pc_input_script_pak >= 0) {
        over[0] = (PcSettingValue){"game", "pak", pc_input_script_pak ? "memory" : "none"};
        first = &over[0];
    }
    char exe_dir[PC_CFG_PATH];
    pc_config_exe_dir(exe_dir, sizeof exe_dir, argv[0]);
    /* The program's folders, and the log of a game run from here on (port/app, docs/port/RELEASE.md). */
    PcAppDirs dirs;
    pc_app_dirs(&dirs, exe_dir);
    if (use_log && !print_config && !selftest) {
        char default_log[PC_APP_PATH];
        pc_app_join(default_log, sizeof default_log, dirs.data, "rush2049pc.log");
        pc_app_log_window(!headless);
        pc_on_fault = pc_app_on_fault;
        pc_on_exit = pc_app_finish;
        if (pc_app_log_open(log_path ? log_path : default_log, argc, argv, &dirs) == 0)
            fprintf(stderr, "rush2049pc %s: the log is %s\n", pc_app_version(), pc_app_log_path());
    }
    if (dirs.portable && use_config && !config && !print_config && !selftest)
        fill_portable_ini(&dirs);
    static PcSettings cfg;
    int file_mode = !use_config || selftest           ? PC_CFG_FILE_NONE
                    : config != NULL || print_config ? PC_CFG_FILE_READ
                                                     : PC_CFG_FILE_CREATE;
    if (pc_settings_resolve(&cfg, file_mode, config, exe_dir, first, (int)(&over[nover] - first)) != 0)
        return 2;
    if (print_config) {
        char *text = pc_settings_format(&cfg);
        fputs(text, stdout);
        printf("\n; in effect: %s, then the environment and the command line\n",
               cfg.source[0] ? cfg.source : "the defaults (no settings file)");
        free(text);
        return 0;
    }

    pc_os_set_expansion_pak(cfg.expansion_pak);
    int pak_kind = cfg.pak == PC_CFG_PAK_MEMORY ? PC_PAK_MEMORY : cfg.pak == PC_CFG_PAK_RUMBLE ? PC_PAK_RUMBLE
                                                                                              : PC_PAK_NONE;
    /* port/gfx reads its settings from the environment; the resolved values already include it */
    char scale[16], aspect[16], rate[16];
    pc_settings_get(&cfg, "video", "scale", scale, sizeof scale);
    setenv("RUSH_GFX_SCALE", scale, 1);
    setenv("RUSH_GFX_API", cfg.api == PC_CFG_API_GL ? "gl" : cfg.api == PC_CFG_API_GLES ? "gles" : "auto", 1);
    pc_settings_get(&cfg, "video", "widescreen", aspect, sizeof aspect);
    setenv("RUSH_GFX_WIDESCREEN", aspect, 1);
    pc_settings_get(&cfg, "video", "frame_rate", rate, sizeof rate);
    setenv("RUSH_GFX_FRAME_RATE", rate, 1);
    setenv("RUSH_GFX_TEXTURES", cfg.textures == 1 ? "upscaled" : cfg.textures == 2 ? "pack" : "original", 1);
    if (cfg.widescreen > 0 && cfg.width == 640 && cfg.height == 480)   /* the default window, at the picture's aspect */
        cfg.width = (int)(cfg.height * cfg.widescreen + 0.5);
    if (!run.frames && pc_input_script_frames)
        run.frames = pc_input_script_frames;
    char shot_path[64];
    if (run.screenshot_at && !run.screenshot) {
        snprintf(shot_path, sizeof shot_path, "rush2049pc-%u.png", run.screenshot_at);
        run.screenshot = shot_path;
    }

#ifndef _WIN32   /* stderr is unbuffered there, and the CRT has no line buffering */
    setvbuf(stderr, NULL, _IOLBF, 0);
#endif
    pc_mem_fix_absolute();
    pc_fault_install();
    if (pc_mem_init() != 0) {
        fprintf(stderr, "rush2049pc: cannot map the RDRAM window\n");
        return 1;
    }
    if (selftest) {
        pc_time_set_virtual(fast);
        return pc_selftest();
    }

    /* The ROM: from the command line as given, else game.rom by the lookup order (port/app/paths.c). */
    int rom_from_command_line = from_command_line(over, nover, "game", "rom");
    if (no_rom ? pc_rom_blank() != 0
               : pc_app_load_rom(&dirs, cfg.rom, rom_from_command_line, cfg.rom_check,
                                 cfg.source[0] ? cfg.source : cfg.save_path, cfg.source, !headless) != 0)
        return 1;
    pc_os_initialize();
    if (pc_data_init(pc_rom_bytes(), pc_rom_size()) != 0) {
        fprintf(stderr, "rush2049pc: the game's data could not be read from the ROM\n");
        if (!no_rom)
            return 1;   /* --no-rom goes on with the data zeroed */
    }
    if (no_rom)
        blank_files();
    musyx_services(no_rom);
    /* Textures (docs/port/TEXTURES.md): the pack, the dump and the upscaled textures' cache are in the data folder;
     * the environment's own (RUSH_GFX_TEXTURE_*) stay. */
    {
        char tex_dir[PC_APP_PATH];
        pc_app_data_file(&dirs, cfg.texture_pack, from_command_line(over, nover, "video", "texture_pack"), tex_dir,
                         sizeof tex_dir);
        setenv("RUSH_GFX_TEXTURE_PACK", tex_dir, 0);
        if (cfg.texture_dump[0]) {
            pc_app_data_file(&dirs, cfg.texture_dump, from_command_line(over, nover, "video", "texture_dump"),
                             tex_dir, sizeof tex_dir);
            setenv("RUSH_GFX_TEXTURE_DUMP", tex_dir, 0);
        }
        pc_app_data_file(&dirs, "texture_cache", 0, tex_dir, sizeof tex_dir);
        setenv("RUSH_GFX_TEXTURE_CACHE", tex_dir, 0);
    }
    window_setup(&cfg);
    if (pc_gfx_init(headless, cfg.width, cfg.height) != 0) {   /* the game draws 320 x 220; 0 x 0: auto */
        fprintf(stderr, "rush2049pc: no renderer: no OpenGL ES 3.0 or OpenGL 3.1 context\n");
        pc_app_alert(!headless, "Rush 2049 PC: no graphics",
                     "The program could not start OpenGL: it needs OpenGL ES 3.0 or OpenGL 3.1 or later.\n\n"
                     "Updating the graphics driver usually fixes this. The log says more.");
        return 1;
    }
    if (!headless)
        pc_app_window_icon();   /* Linux; Windows' window has the executable's icon */
    static char title_tag[80];
    snprintf(title_tag, sizeof title_tag, "Rush2049PC %s", pc_app_version());
    pc_input_title_tag = title_tag;
    /* What the program writes goes into the data folder: next to it when portable, else the user data folder
     * (port/app/paths.c); paths from the command line are from the working directory. */
    char pak_path[PC_APP_PATH], dir[PC_APP_PATH];
    if (pak_kind == PC_PAK_MEMORY) {
        int old = pc_app_pak_file(&dirs, cfg.pak_file, from_command_line(over, nover, "game", "pak_file"), pak_path,
                                  sizeof pak_path);
        pc_app_dirname(pak_path, dir, sizeof dir);
        pc_app_mkdirs(dir);
        fprintf(stderr, "rush2049pc: saves (the controller pak): %s%s\n", pak_path,
                old ? ", in the working directory, where earlier builds kept it" : "");
    }
    pc_app_data_file(&dirs, cfg.screenshot_dir[0] ? cfg.screenshot_dir : "screenshots",
                     cfg.screenshot_dir[0] && from_command_line(over, nover, "video", "screenshot_dir"), dir,
                     sizeof dir);
    snprintf(cfg.screenshot_dir, sizeof cfg.screenshot_dir, "%s", dir);
    if (!headless)
        pc_app_mkdirs(cfg.screenshot_dir);   /* F12's folder */
    if (pc_input_init(&cfg) != 0 && !headless)
        fprintf(stderr, "rush2049pc: no window: scripted input only\n");
    pc_si_init(pak_kind == PC_PAK_MEMORY ? pak_path : NULL, pak_kind);
    if (pc_audio_init(cfg.audio, dump_audio) != 0)
        fprintf(stderr, "rush2049pc: audio output falls back to null\n");
    pc_audio_set_volume(cfg.volume / 100.0);
    pc_audio_set_latency((unsigned)cfg.audio_latency);
    if (pc_verbose)
        pc_audio_set_report_every(10);   /* the output's counters every 10 s, besides the exit line */
    if (!pc_mem_fixed())
        fprintf(stderr, "rush2049pc: warning: N64 addresses are translated; the game's fixed addresses fault\n");

    run.game_state = game_state;
    run.game_info = game_info;
    if (fast) {
        pc_time_set_virtual(1);
        pc_audio_set_clock(fast_clock);
    }
    pc_os_boot(n64_main, NULL, "boot");
    int code = pc_ultra_run(&run);
    remember_window(&cfg, headless);
    pc_input_shutdown();
    pc_audio_shutdown();
    pc_gfx_shutdown();
    pc_app_finish(code);
    fflush(NULL);
    _exit(code);   /* the N64 threads are still running game code: no atexit handlers */
}

int main(int argc, char **argv)
{
    pc_app_startup();   /* Windows: the console (or none), the timer period (port/app/startup.c) */
    int code = rush_main(argc, argv);   /* returns only when it stops early: an error, --help, --print-config... */
    pc_app_finish(code);
    return code;
}
