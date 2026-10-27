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
            "  --scale N           internal resolution, a multiple of the N64's (1-8)\n"
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
    char scale[8];
    snprintf(scale, sizeof scale, "%d", cfg.scale);
    setenv("RUSH_GFX_SCALE", scale, 1);
    setenv("RUSH_GFX_API", cfg.api == PC_CFG_API_GL ? "gl" : "gles", 1);
    if (cfg.widescreen)
        fprintf(stderr, "rush2049pc: widescreen is not implemented yet; the picture stays 4:3\n");
    if (!run.frames && pc_input_script_frames)
        run.frames = pc_input_script_frames;
    char shot_path[64];
    if (run.screenshot_at && !run.screenshot) {
        snprintf(shot_path, sizeof shot_path, "rush2049pc-%u.png", run.screenshot_at);
        run.screenshot = shot_path;
    }

    setvbuf(stderr, NULL, _IOLBF, 0);
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
