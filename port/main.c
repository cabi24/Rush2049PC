/* port/main.c: PC entry point (docs/PORT_PLAN.md, docs/port/ULTRA.md, docs/port/FRONTEND.md).
 *
 * Resolves the settings (defaults, rush2049pc.ini, the environment, a press file's `pak` line, then the
 * command line: port/config), loads and checks the ROM, maps the RDRAM window, fills the game's initialized data
 * (port/data), opens the renderer (port/gfx) and live input (port/input), then starts the game's own main
 * (0x800020F0, compiled as n64_main) on an N64 thread and runs the host main loop (port/ultra/loop.c) until the
 * window closes or a limit is reached. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "pc_compat.h"
#include "ultra.h"

extern void n64_main(void *argument);   /* src/rom/lib_2cf0.c */
int pc_selftest(void);                  /* port/ultra/selftest.c */

static void usage(FILE *f)
{
    fprintf(f,
            "usage: rush2049pc [--rom PATH] [options]\n"
            "Settings come from rush2049pc.ini (next to the program, else ~/.config/rush2049pc/; written with the\n"
            "defaults on first run), then the environment, then these options.\n"
            "  --rom PATH          the San Francisco Rush 2049 (USA) ROM, .z64/.v64/.n64 (default baserom.us.z64)\n"
            "  --no-rom-check      run a ROM whose SHA-1 differs\n"
            "  --no-rom            bring-up: a blank cartridge instead of the ROM (every ROM read is zero)\n"
            "  --headless          render off screen (EGL surfaceless; the default without DISPLAY or\n"
            "                      WAYLAND_DISPLAY); --window: an SDL2 window\n"
            "  --fullscreen        start in fullscreen (Alt+Enter or F11 toggles)\n"
            "  --size WxH          window size (default 640x480)\n"
            "  --scale N           internal resolution, a multiple of the N64's (1-8), or auto (the window's)\n"
            "  --widescreen A      widen the picture to an aspect: off, auto (the window's), 16:9, 21:9, W:H...\n"
            "  --seconds N         stop after N seconds\n"
            "  --frames N          stop after N graphics tasks (at the retrace that shows the last)\n"
            "  --screenshot-at N   save a PNG once N graphics tasks have run (--screenshot PATH, default\n"
            "                      rush2049pc-N.png)\n"
            "  --stop-at-gfx       stop at the first graphics task (osSpTaskStartGo), reporting it\n"
            "  --press T:BUTTON[@PORT][:DUR],...   scripted input (v5's format; T, DUR in seconds)\n"
            "  --press-file PATH   a .press input script\n"
            "  --log-input         print every change of a player's buttons and stick\n"
            "  --pak PATH          controller pak image for controller 1 (default rush2049pc.mpk)\n"
            "  --no-pak            no accessory; --rumble: a rumble pak instead\n"
            "  --expansion-pak     report 8 MiB of RDRAM (default 4 MiB: no Expansion Pak)\n"
            "  --audio BACKEND     auto (default), sdl, alsa, null or wav (port/audio)\n"
            "  --dump-audio PATH   also write everything the game plays to a WAV file\n"
            "  --config PATH       read this settings file instead (it is not created); --no-config: none\n"
            "  --set S.KEY=VALUE   any setting, as the file names it (video.vsync=off, keyboard.A=Space)\n"
            "  --print-config      print the settings in effect, as a settings file, and exit\n"
            "  --status N          a status line every N seconds\n"
            "  -v, --verbose       more log lines (repeat for more); --trace-os: every thread switch\n"
            "  --selftest          test the thread scheduler, queues, events and timers (no ROM needed)\n"
            "Ctrl-\\ prints the N64 threads' states.\n");
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

#define MAX_OVERRIDES 64

int main(int argc, char **argv)
{
    const char *dump_audio = getenv("RUSH2049_DUMP_AUDIO"), *config = NULL;
    int no_rom = 0, headless = !getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY"), selftest = 0, use_config = 1;
    int print_config = 0;
    PcRunOptions run = {0};
    PcSettingValue over[MAX_OVERRIDES + 1];   /* the command line's settings, in order; [0] for a press file */
    int nover = 1;
    char width[16], height[16];

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = i + 1 < argc ? argv[i + 1] : NULL;
#define ARG() (v ? (i++, v) : (fprintf(stderr, "rush2049pc: %s needs a value\n", a), exit(2), ""))
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
        else if (!strcmp(a, "--screenshot-at"))
            run.screenshot_at = (uint32_t)atoi(ARG());
        else if (!strcmp(a, "--screenshot"))
            run.screenshot = ARG();
        else if (!strcmp(a, "--seconds"))
            run.seconds = atof(ARG());
        else if (!strcmp(a, "--frames"))
            run.frames = (uint32_t)atoi(ARG());
        else if (!strcmp(a, "--stop-at-gfx"))
            pc_rcp_stop_at_gfx = 1;
        else if (!strcmp(a, "--press")) {
            if (pc_input_add_spec(ARG()) != 0)
                return 2;
        } else if (!strcmp(a, "--press-file")) {
            if (pc_input_add_file(ARG()) != 0)
                return 2;
        } else if (!strcmp(a, "--log-input"))
            pc_input_log = 1;
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
        else if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
            usage(stdout);
            return 0;
        } else {
            fprintf(stderr, "rush2049pc: unknown option %s\n", a);
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
    char scale[8], aspect[16];
    pc_settings_get(&cfg, "video", "scale", scale, sizeof scale);
    setenv("RUSH_GFX_SCALE", scale, 1);
    setenv("RUSH_GFX_API", cfg.api == PC_CFG_API_GL ? "gl" : "gles", 1);
    pc_settings_get(&cfg, "video", "widescreen", aspect, sizeof aspect);
    setenv("RUSH_GFX_WIDESCREEN", aspect, 1);
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
    if (selftest)
        return pc_selftest();

    if (no_rom ? pc_rom_blank() != 0 : pc_rom_load(cfg.rom, cfg.rom_check) != 0)
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
    if (pc_gfx_init(headless, cfg.width, cfg.height) != 0) {   /* the game draws 320 x 220 */
        fprintf(stderr, "rush2049pc: no renderer\n");
        return 1;
    }
    if (pc_input_init(&cfg) != 0 && !headless)
        fprintf(stderr, "rush2049pc: no window: scripted input only\n");
    pc_si_init(pak_kind == PC_PAK_MEMORY ? cfg.pak_file : NULL, pak_kind);
    if (pc_audio_init(cfg.audio, dump_audio) != 0)
        fprintf(stderr, "rush2049pc: audio output falls back to null\n");
    if (!pc_mem_fixed())
        fprintf(stderr, "rush2049pc: warning: N64 addresses are translated; the game's fixed addresses fault\n");

    pc_os_boot(n64_main, NULL, "boot");
    int code = pc_ultra_run(&run);
    pc_input_shutdown();
    pc_audio_shutdown();
    pc_gfx_shutdown();
    fflush(NULL);
    _exit(code);   /* the N64 threads are still running game code: no atexit handlers */
}
