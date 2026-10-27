/* port/main.c: PC entry point (docs/PORT_PLAN.md, docs/port/ULTRA.md).
 *
 * Loads and checks the ROM, maps the RDRAM window, fills the game's initialized data (port/data), opens the
 * renderer (port/gfx), then starts the game's own main (0x800020F0, compiled as n64_main) on an N64 thread and
 * runs the host main loop (port/ultra/loop.c) until the window closes or a limit is reached. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ultra.h"

extern void n64_main(void *argument);   /* src/rom/lib_2cf0.c */
int pc_selftest(void);                  /* port/ultra/selftest.c */

static void usage(FILE *f)
{
    fprintf(f,
            "usage: rush2049pc [--rom PATH] [options]\n"
            "  --rom PATH          the San Francisco Rush 2049 (USA) ROM, .z64/.v64/.n64 (default baserom.us.z64)\n"
            "  --no-rom-check      run a ROM whose SHA-1 differs\n"
            "  --no-rom            bring-up: a blank cartridge instead of the ROM (every ROM read is zero)\n"
            "  --headless          no window (port/gfx renders off screen or not at all)\n"
            "  --seconds N         stop after N seconds\n"
            "  --frames N          stop after N graphics tasks\n"
            "  --stop-at-gfx       stop at the first graphics task (osSpTaskStartGo), reporting it\n"
            "  --press T:BUTTON[@PORT][:DUR],...   scripted input (v5's format; T, DUR in seconds)\n"
            "  --press-file PATH   a .press input script\n"
            "  --pak PATH          controller pak image for controller 1 (default rush2049pc.mpk)\n"
            "  --no-pak            no accessory; --rumble: a rumble pak instead\n"
            "  --expansion-pak     report 8 MiB of RDRAM (default 4 MiB: no Expansion Pak)\n"
            "  --audio BACKEND     auto (default), sdl, alsa, null or wav (port/audio)\n"
            "  --dump-audio PATH   also write everything the game plays to a WAV file\n"
            "  --status N          a status line every N seconds\n"
            "  -v, --verbose       more log lines (repeat for more); --trace-os: every thread switch\n"
            "  --selftest          test the thread scheduler, queues, events and timers (no ROM needed)\n"
            "Ctrl-\\ prints the N64 threads' states.\n");
}

int main(int argc, char **argv)
{
    const char *rom = "baserom.us.z64", *pak = "rush2049pc.mpk";
    const char *audio = getenv("RUSH2049_AUDIO"), *dump_audio = getenv("RUSH2049_DUMP_AUDIO");
    int check = 1, no_rom = 0, headless = 0, pak_kind = PC_PAK_MEMORY, pak_set = 0, selftest = 0;
    PcRunOptions run = {0};

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = i + 1 < argc ? argv[i + 1] : NULL;
#define ARG() (v ? (i++, v) : (fprintf(stderr, "rush2049pc: %s needs a value\n", a), exit(2), ""))
        if (!strcmp(a, "--rom"))
            rom = ARG();
        else if (!strcmp(a, "--no-rom-check"))
            check = 0;
        else if (!strcmp(a, "--no-rom"))
            no_rom = 1;
        else if (!strcmp(a, "--headless"))
            headless = 1;
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
        } else if (!strcmp(a, "--pak"))
            pak = ARG(), pak_kind = PC_PAK_MEMORY, pak_set = 1;
        else if (!strcmp(a, "--no-pak"))
            pak_kind = PC_PAK_NONE, pak_set = 1;
        else if (!strcmp(a, "--rumble"))
            pak_kind = PC_PAK_RUMBLE, pak_set = 1;
        else if (!strcmp(a, "--audio"))
            audio = ARG();
        else if (!strcmp(a, "--dump-audio"))
            dump_audio = ARG();
        else if (!strcmp(a, "--expansion-pak"))
            pc_os_set_expansion_pak(1);
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
#undef ARG
    }
    if (!pak_set && pc_input_script_pak == 0)
        pak_kind = PC_PAK_NONE;
    if (!run.frames && pc_input_script_frames)
        run.frames = pc_input_script_frames;

    setvbuf(stderr, NULL, _IOLBF, 0);
    pc_mem_fix_absolute();
    pc_fault_install();
    if (pc_mem_init() != 0) {
        fprintf(stderr, "rush2049pc: cannot map the RDRAM window\n");
        return 1;
    }
    if (selftest)
        return pc_selftest();

    if (no_rom ? pc_rom_blank() != 0 : pc_rom_load(rom, check) != 0)
        return 1;
    pc_os_initialize();
    if (pc_data_init(pc_rom_bytes(), pc_rom_size()) != 0) {
        fprintf(stderr, "rush2049pc: the game's data could not be read from the ROM\n");
        if (!no_rom)
            return 1;   /* --no-rom goes on with the data zeroed */
    }
    if (pc_gfx_init(headless, 640, 480) != 0) {
        fprintf(stderr, "rush2049pc: no renderer\n");
        return 1;
    }
    pc_si_init(pak_kind == PC_PAK_MEMORY ? pak : NULL, pak_kind);
    if (pc_audio_init(audio ? audio : "auto", dump_audio) != 0)
        fprintf(stderr, "rush2049pc: audio output falls back to null\n");
    if (!pc_mem_fixed())
        fprintf(stderr, "rush2049pc: warning: N64 addresses are translated; the game's fixed addresses fault\n");

    pc_os_boot(n64_main, NULL, "boot");
    int code = pc_ultra_run(&run);
    pc_audio_shutdown();
    pc_gfx_shutdown();
    fflush(NULL);
    _exit(code);   /* the N64 threads are still running game code: no atexit handlers */
}
