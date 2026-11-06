/* port/main.c: PC entry point (docs/PORT_PLAN.md, docs/port/ULTRA.md, docs/port/FRONTEND.md).
 *
 * Resolves the settings (defaults, rush2049pc.ini, the environment, a press file's `pak` line, then the
 * command line: port/config), loads and checks the ROM, maps the RDRAM window, fills the game's initialized data
 * (port/data), opens the renderer (port/gfx) and live input (port/input), then starts the game's own main
 * (0x800020F0, compiled as n64_main) on an N64 thread and runs the host main loop (port/ultra/loop.c) until the
 * window closes or a limit is reached. */
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app.h"
#include "config.h"
#include "input_internal.h"
#include "mods.h"
#include "pc_audio.h"
#include "pc_compat.h"
#include "pc_data.h"
#include "pakfs.h"
#include "pc_game_fps.h"
#include "pc_start_menu.h"
#include "pc_ghost_prompt.h"
#include "pc_cheats.h"
#include "pc_game_types.h"
#include "ultra.h"

extern void n64_main(void *argument);   /* src/rom/lib_2cf0.c */
static PcMods g_mods;                   /* the mods folder (docs/port/MODS.md) */
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
            "  --seconds N         stop after N seconds (of game time with --fast or --paced)\n"
            "  --fast              run as fast as the host can: the N64 clock moves on only while every N64\n"
            "                      thread waits, so a run is the same on any host (tests, scripted runs)\n"
            "  --paced             --fast's run in real time: the same course to the bit, each move of the N64\n"
            "                      clock at its wall time (online play's clock; RUSH_PACED=1 too)\n"
            "  --frames N          stop after N graphics tasks (at the retrace that shows the last)\n"
            "  --screenshot-at N   save a PNG once N graphics tasks have run (--screenshot PATH, default\n"
            "                      rush2049pc-N.png)\n"
            "  --stop-at-gfx       stop at the first graphics task (osSpTaskStartGo), reporting it\n"
            "  --until-state M[:N] stop once the game's state word (gGameState) has a bit of M and N more\n"
            "                      graphics tasks have run; with --seconds, not getting there is exit code 1\n"
            "  --press T:BUTTON[@PORT][:DUR],...   scripted input (T, DUR in seconds)\n"
            "  --press-file PATH   a .press input script\n"
            "  --log-input         print every change of a player's buttons and stick\n"
            "  --log-cars          at the end, print each car of the race: its place, lap, distance and how far\n"
            "                      it is from where the race began (tests of races of more cars)\n"
            "  --log-physics N     every N retraces, the model thread's clock and each car's position and velocity\n"
            "                      (exact with RUSH_STATE_EXACT): game.frame_rate's comparison, fps_compare.py\n"
            "  --log-voices N      every N retraces and at the end, the sound's voices, starts refused and pools\n"
            "                      (audio.voices, audio.engines: docs/port/AUDIO.md 10)\n"
            "  --log-frames        at each of the game's frames, its retraces, the model tick and the camera's eye\n"
            "                      and car 0 as the last frame drew them: game.frame_rate's evenness, fps_even.py\n"
            "  --press-via K[:T]   the scripts through SDL as live input (tests): controller or joystick (virtual\n"
            "                      ones, plugged in at T seconds), or keyboard; with a window or --headless\n"
            "  --pak PATH          controller pak image for controller 1 (default rush2049pc.mpk in the data\n"
            "                      folder: next to the program when rush2049pc.ini is there, else the user data\n"
            "                      folder; it also holds screenshots/ and rush2049pc.log)\n"
            "  --saves pak|files   how every player's controller pak is kept: pak (the .mpk image, the default)\n"
            "                      or files (a folder of saves/p1... in the data folder: a file per profile and\n"
            "                      ghost; game.saves, saves2...; docs/port/SAVES.md)\n"
            "  --no-pak            no accessory on any controller; --rumble: a rumble pak on controller 1\n"
            "                      (controllers 2-4: game.pak2... = auto, memory, rumble or none; --set)\n"
            "  --expansion-pak     report 8 MiB of RDRAM, the Expansion Pak (the default; game.expansion_pak=off:\n"
            "                      4 MiB); --set game.memory=N gives the game N MiB, 16-256\n"
            "  --audio BACKEND     auto (default), sdl, alsa, null or wav (port/audio)\n"
            "  --dump-audio PATH   also write everything the game plays to a WAV file\n"
            "  --mods-dir PATH     the mods folder (game.mods_dir; default mods in the data folder); --no-mods:\n"
            "                      the cartridge's files only (game.mods=off)\n"
            "  --config PATH       read this settings file instead (it is not created); --no-config: none\n"
            "  --set S.KEY=VALUE   any setting, as the file names it (video.vsync=off, keyboard.A=Space)\n"
            "  --preset NAME       several settings at once, as F1's Presets page sets them: enhanced (the game at\n"
            "                      60, far, high detail, every engine heard), maximum, or n64 (their defaults);\n"
            "                      a --set after it wins\n"
            "  --print-config      print the settings in effect, as a settings file, and exit\n"
            "  --status N          a status line every N seconds\n"
            "  -v, --verbose       more log lines (repeat for more); --trace-os: every thread switch\n"
            "  --log PATH          write the run's output to PATH (default rush2049pc.log where the program keeps\n"
            "                      its files; the run before is kept as rush2049pc.old.log); --no-log: none\n"
            "  --selftest          test the thread scheduler, queues, events and timers (no ROM needed)\n"
            "  --version           print the version and the build, and exit\n"
            "F1 (or both stick clicks on a controller) opens the settings overlay, which holds the game; a\n"
            "--press script's MENU does too. Ctrl-\\ (Ctrl-Break on Windows) prints the N64 threads' states.\n");
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

/* RUSH2049_TEST_WAVE_GROW=FIRST[-LAST]:BYTES (tests, docs/port/MEMORY.md 5): those waves' files are BYTES larger than
 * the cartridge's, as a replacement file of that size would be. The game allocates and reads each by its size in the
 * file table (gWaveSizes); the bytes past the file are the cartridge's next ones (zero past its end), and the
 * converters read a file by its own tables, so only the heap block is larger. */
static void test_grow_waves(void)
{
    const char *e = getenv("RUSH2049_TEST_WAVE_GROW");
    unsigned first, last, bytes;
    if (!e || !*e)
        return;
    if (sscanf(e, "%u-%u:%u", &first, &last, &bytes) != 3) {
        if (sscanf(e, "%u:%u", &first, &bytes) != 2) {
            fprintf(stderr, "rush2049pc: RUSH2049_TEST_WAVE_GROW takes FIRST[-LAST]:BYTES, not '%s'\n", e);
            return;
        }
        last = first;
    }
    if (last >= BLANK_WAVES)
        last = BLANK_WAVES - 1;
    for (unsigned w = first; w <= last; w++)
        D_8002E580[w] += bytes;
    fprintf(stderr, "rush2049pc: test: waves %u-%u are %u bytes larger (RUSH2049_TEST_WAVE_GROW)\n", first, last,
            bytes);
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

/* game.frame_rate as the game has it latched (pc_game_fps.h): 60, or 0 for original */
static int game_rate(void)
{
    return pc_fps60 ? 60 : 0;
}

/* What the state log adds to each change: who plays what (gNumPlayers, gNumWindows, the cars in the race, gRace,
 * gTrackNum, the pause screen's state) and where the first car is (its model record's RWR and the length of RWV, in feet and feet a second;
 * and its steering input, -1..1),
 * so a scripted run shows which path it took and whether the car moved. */
extern int16_t active_player_count, gNumWindows;
extern int32_t gameplay_mode;
extern int8_t D_80152744, D_8014978C;
extern int32_t D_801170FC;     /* gPauseState: 0 racing, 1 the pause menu, 2-8 its screens */

/* --log-cars (docs/port/MORECARS.md 10.3): each car of the race at the run's end, its slot, whether its model is in
 * the race (active), its place (0 the first), lap and distance along the track and its position (its car record's),
 * the race's latched count (pc_race_cars(), which the setting changes only from the next race),
 * and how far its model is from where it stood as the race began (the first state log snapshot in the race, 0x400000,
 * or in the attract demo, 8, with its cars in the race), so a test can tell that a race of N cars held N, each at a
 * place of its own and each moving (tools/port/cars_check.py). Off by default, and only this line: the traces are the
 * same with it. */
extern int16_t D_801543CA;      /* the race's car slots (gNumCars) */
static int g_log_cars;
static float g_cars_start[PC_MODEL_SLOTS][3];
static int g_cars_started;

static void cars_note_start(void)
{
    uint32_t s = game_state();
    int demo = (s & 8u) && *(volatile int8_t *)&D_80152744 > 0 && *(volatile int16_t *)&pc_models[0].active;
    if (!g_log_cars || g_cars_started || s == 0xFFFFFFFFu || !((s & 0x400000u) || demo))
        return;
    for (int i = 0; i < PC_MODEL_SLOTS; i++)
        for (int k = 0; k < 3; k++)
            g_cars_start[i][k] = ((const volatile float *)pc_models[i].rwr)[k];
    g_cars_started = 1;
}

static void cars_info(char *out, size_t size)
{
    int n = *(volatile int16_t *)&D_801543CA, len;
    if (n < 0 || n > PC_MODEL_SLOTS)
        n = PC_MODEL_SLOTS;
    len = snprintf(out, size, "cars at the end: %d in the race, count %d%s", n, pc_race_cars(),
                   g_cars_started ? "" : " (it never began)");
    for (int i = 0; i < n && len > 0 && (size_t)len < size; i++) {
        const volatile float *r = pc_models[i].rwr;
        double d[3], moved;
        for (int k = 0; k < 3; k++)
            d[k] = (double)r[k] - g_cars_start[i][k];
        moved = sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        const volatile float *c = pc_cars[i].pos;
        len += snprintf(out + len, size - (size_t)len,
                        "; %d %s place %d lap %d distance %.0f moved %.0f at %.0f %.0f %.0f", i,
                        *(volatile int16_t *)&pc_models[i].active ? "active" : "inactive",
                        *(volatile signed char *)&pc_cars[i].place, *(volatile signed char *)&pc_models[i].lap,
                        (double)*(volatile float *)&pc_cars[i].distance, moved, (double)c[0], (double)c[1],
                        (double)c[2]);
    }
}

/* --log-physics N (docs/port/FRAMERATE.md 7): every N retraces, the model thread's tick (D_80143FF4) and time
 * (D_801543CC) and each car of the race (gNumCars, D_801543CA): its model record's position (RWR) and velocity (RWV),
 * its lap and its place. Read as the state log is, while every N64 thread waits, so a run at game.frame_rate 60 and
 * one at original compare retrace by retrace (tools/port/fps_compare.py). RUSH_STATE_EXACT gives exact hex floats. */
extern int32_t D_80143FF4;
extern float D_801543CC;
static void physics_info(char *out, size_t size)
{
    static int exact = -1;
    if (exact < 0)
        exact = getenv("RUSH_STATE_EXACT") != NULL;
    int n = *(volatile int16_t *)&D_801543CA, len;
    if (n < 0 || n > PC_MODEL_SLOTS)
        n = 0;
    len = snprintf(out, size, exact ? "tick %d time %a cars %d" : "tick %d time %.3f cars %d",
                   (int)*(volatile int32_t *)&D_80143FF4, (double)*(volatile float *)&D_801543CC, n);
    for (int i = 0; i < n && len > 0 && (size_t)len < size; i++) {
        const volatile float *r = pc_models[i].rwr, *v = pc_models[i].rwv;
        len += snprintf(out + len, size - (size_t)len, exact ? "; %d %a %a %a v %a %a %a lap %d place %d"
                        : "; %d %.2f %.2f %.2f v %.2f %.2f %.2f lap %d place %d", i, (double)r[0], (double)r[1],
                        (double)r[2], (double)v[0], (double)v[1], (double)v[2],
                        *(volatile signed char *)&pc_models[i].lap, *(volatile signed char *)&pc_cars[i].place);
    }
}

/* --log-voices N (docs/port/AUDIO.md 10): every N retraces, read as the state log is, a line of the sound's voices
 * since the line before: the mixer's subframes and the voices each mixed (port/audio), the starts MusyX refused and
 * the voices it took from a playing sound, the effects the sound manager started that MusyX didn't, the tags its
 * priority passes held back for their budget (a pass), the positional starts that found no emitter, the commands
 * that found no message and the effect starts that found no entry (PC_SND_*, the game's sound code); the emitters and fx tags in use now, the message pool's
 * high-water mark, and which drones' engines played (audio.engines). At the end, the same over the whole run, with the
 * mixer's spread. Off by default, and only these lines: the traces are the same with it. */
typedef struct { unsigned char f[4]; uint32_t count; void *head, *tail; } SndList;
extern SndList D_801461E8, D_80143FF8;   /* the active emitters; the free fx tags */
extern int32_t D_80110264, D_8011026C;   /* the fx tags made, the emitters made (hud_setup) */
static uint32_t g_voices_every;
static PcMixerStats g_voices_mix;
static unsigned long long g_voices_count[PC_SND_COUNTERS];

static int voices_line(char *out, size_t size, const PcMixerStats *m, const PcMixerStats *m0,
                       const unsigned long long *c, const unsigned long long *c0)
{
    unsigned long long d[PC_SND_COUNTERS];
    unsigned long long sub = m->subframes - m0->subframes;
    for (int i = 0; i < PC_SND_COUNTERS; i++)
        d[i] = c[i] - c0[i];
    return snprintf(out, size, "mixed %.2f voices a subframe of %u (%llu subframes); musyx refused %llu, stole %llu "
                    "(and %llu of the songs'); "
                    "effects refused %llu; over budget %.2f a pass (%llu passes); no emitter %llu, no message %llu, no entry "
                    "%llu; loops without a macro %llu (voice-ticks); second starts dropped %llu; "
                    "emitters %u of %d, fx tags %d of %d", sub ? (double)(m->voices - m0->voices) / (double)sub : 0.0,
                    m->slots, sub, d[PC_SND_MX_REFUSED], d[PC_SND_MX_STOLEN], d[PC_SND_MX_STOLEN_SONG],
                    d[PC_SND_FX_REFUSED],
                    d[PC_SND_PASSES] ? (double)d[PC_SND_OVER_BUDGET] / (double)d[PC_SND_PASSES] : 0.0,
                    d[PC_SND_PASSES], d[PC_SND_NO_EMITTER], d[PC_SND_NO_MESSAGE], d[PC_SND_NO_ENTRY],
                    d[PC_SND_MX_LOOP_ORPHAN], d[PC_SND_START_PLAYING],
                    (unsigned)*(volatile uint32_t *)&D_801461E8.count, (int)*(volatile int32_t *)&D_8011026C,
                    (int)*(volatile int32_t *)&D_80110264 - (int)*(volatile uint32_t *)&D_80143FF8.count,
                    (int)*(volatile int32_t *)&D_80110264);
}

static void voices_info(char *out, size_t size)
{
    PcMixerStats m;
    unsigned long long c[PC_SND_COUNTERS];
    int msg_most;
    pc_audio_mixer_stats(&m);
    pc_snd_counters(c, &msg_most);
    int len = voices_line(out, size, &m, &g_voices_mix, c, g_voices_count);
    unsigned on = pc_engines_on(), heard;
    int layers, playing, most;
    pc_engines_heard(&layers, &playing, &heard, &most);
    if (len > 0 && (size_t)len < size)
        len += snprintf(out + len, size - (size_t)len, "; messages most %d; drones' engine layers playing %d of %d, "
                        "drones", msg_most, playing, layers);
    for (int i = 0; i < PC_MAX_LINKS && len > 0 && (size_t)len < size; i++)
        if (heard >> i & 1)
            len += snprintf(out + len, size - (size_t)len, " %d", i);
    if (len > 0 && (size_t)len < size)
        len += snprintf(out + len, size - (size_t)len, "; engines on");
    for (int i = 0; i < PC_MAX_LINKS && len > 0 && (size_t)len < size; i++)
        if (on >> i & 1)
            len += snprintf(out + len, size - (size_t)len, " %d", i);
    if (len > 0 && (size_t)len < size && !on)
        snprintf(out + len, size - (size_t)len, " %s", pc_engines() == PC_ENGINES_ORIGINAL ? "(original: all the game's)"
                 : "none");
    g_voices_mix = m;
    memcpy(g_voices_count, c, sizeof c);
}

static void voices_end(char *out, size_t size)
{
    PcMixerStats m, zero;
    unsigned long long c[PC_SND_COUNTERS], c0[PC_SND_COUNTERS] = {0};
    int msg_most, len;
    memset(&zero, 0, sizeof zero);
    pc_audio_mixer_stats(&m);
    pc_snd_counters(c, &msg_most);
    len = snprintf(out, size, "voices at the end: ");
    len += voices_line(out + len, size - (size_t)len, &m, &zero, c, c0);
    {
        int layers, playing, most;
        unsigned heard;
        pc_engines_heard(&layers, &playing, &heard, &most);
        len += snprintf(out + len, size - (size_t)len, "; messages most %d; most voices a subframe %u; most drones' "
                        "engine layers playing %d; fx tags most %d, emitters most %d; subframes by voices", msg_most,
                        m.voices_most, most, pc_snd_most_of(PC_SND_MOST_TAGS), pc_snd_most_of(PC_SND_MOST_EMITTERS));
    }
    for (unsigned v = 0; v < sizeof m.voices_hist / sizeof *m.voices_hist && len > 0 && (size_t)len < size; v++)
        if (m.voices_hist[v])
            len += snprintf(out + len, size - (size_t)len, " %u:%llu", v, (unsigned long long)m.voices_hist[v]);
}

static void (*g_end_cars)(char *, size_t);
static void ends_info(char *out, size_t size)   /* --log-cars and --log-voices together: two lines */
{
    int len = 0;
    if (g_end_cars) {
        g_end_cars(out, size);
        len = (int)strlen(out);
        len += snprintf(out + len, size - (size_t)len, "\nrush2049pc: ");
    }
    if (g_voices_every && len >= 0 && (size_t)len < size)
        voices_end(out + len, size - (size_t)len);
}

/* --log-frames (docs/port/FRAMERATE.md 7.6): each of the game's frames, at its start (viUpdateTime, on the game
 * thread): the retrace, the frame's retraces, the model tick, and what the last frame drew from: the camera's eye
 * (gCamera[0].pos, +36), car 0's drawn position (its car record, which the game makes from the model record once a
 * frame) and the camera's view direction (the third row of its matrix, +24). tools/port/fps_even.py checks one model tick a frame at 60 and two at original, and the eye's steps even.
 * RUSH_STATE_EXACT: exact floats. */
extern uint8_t gCamera[];
static void frame_log(int retraces)
{
    static int exact = -1;
    if (exact < 0)
        exact = getenv("RUSH_STATE_EXACT") != NULL;
    const volatile float *e = (const volatile float *)(gCamera + 36), *f = (const volatile float *)(gCamera + 24),
                         *c = pc_cars[0].pos;
    fprintf(stderr, exact ? "rush2049pc: frame vi=%u: retraces %d tick %d eye %a %a %a car %a %a %a view %a %a %a\n"
            : "rush2049pc: frame vi=%u: retraces %d tick %d eye %.3f %.3f %.3f car %.3f %.3f %.3f view %.4f %.4f %.4f\n",
            __atomic_load_n(&pc_frame, __ATOMIC_RELAXED), retraces, (int)*(volatile int32_t *)&D_80143FF4,
            (double)e[0], (double)e[1], (double)e[2], (double)c[0], (double)c[1], (double)c[2], (double)f[0],
            (double)f[1], (double)f[2]);
}

static void game_info(char *out, size_t size)
{
    /* RUSH_STATE_EXACT: the position and speed as exact hex floats, so two runs compare bit for bit
     * (tools/port/synth_compare.py, docs/port/OPTIMIZED.md). */
    static int exact = -1;
    if (exact < 0)
        exact = getenv("RUSH_STATE_EXACT") != NULL;
    cars_note_start();
    const volatile float *v = pc_models[0].rwv, *r = pc_models[0].rwr;   /* the model record (pc_game_types.h) */
    const volatile float *steer = &pc_models[0].steer;                     /* its controls: steer -1..1 */
    double speed = sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]);
    snprintf(out, size, exact ? "players %d, windows %d, cars %d, mode %d, track %d, pause %d; car 0 at %a %a %a, "
             "speed %a, steer %a" : "players %d, windows %d, cars %d, mode %d, track %d, pause %d; car 0 at %.0f %.0f "
             "%.0f, speed %.0f, steer %.2f", *(volatile int16_t *)&active_player_count, *(volatile int16_t *)&gNumWindows,
             *(volatile int8_t *)&D_80152744, (int)*(volatile int32_t *)&gameplay_mode, *(volatile int8_t *)&D_8014978C,
             (int)*(volatile int32_t *)&D_801170FC, (double)r[0], (double)r[1], (double)r[2], speed, (double)*steer);
}

/* video.hud = edges moves the HUD to the screen's edges only in a race with one view (port/gfx/gfx_hud.c): the
 * countdown (0x200000) and the race (0x400000), its pause screen included, with gNumWindows 1 (not the boot's -1). */
static int hud_race(void)
{
    uint32_t s = game_state();
    return s != 0xFFFFFFFFu && (s & 0x00600000u) != 0 && *(volatile int16_t *)&gNumWindows == 1;
}

/* --fast and paced mode: the audio output's null backend drains the queue by the N64 clock, as a device would in real
 * time (paced mode's tap included: its accounting is the null output's, docs/port/AUDIO.md 11). */
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

/* The settings overlay's About page (port/input/menu.c): what only main knows, filled each time the page is made. */
static struct {
    char data[PC_APP_PATH];
    int no_rom;
} g_about;

static void about_fill(PcAbout *a)
{
    PcAudioStats st;
    const char *where = NULL, *rom = pc_app_rom_path(&where);
    snprintf(a->version, sizeof a->version, "%s", pc_app_version());
    snprintf(a->build_type, sizeof a->build_type, "%s", pc_app_build_type());
    snprintf(a->commit, sizeof a->commit, "%s", pc_app_version_commit());
    snprintf(a->build, sizeof a->build, "%s", pc_app_version_build());
    pc_audio_get_stats(&st);
    snprintf(a->audio, sizeof a->audio, "%s at %u Hz", pc_audio_backend_name(), (unsigned)pc_audio_device_rate());
    if (st.device_rate && st.game_rate)   /* as the "audio:" line at exit says it */
        snprintf(a->audio_buffer, sizeof a->audio_buffer, "pulls of %u (%.0f ms), queue %u (%.0f ms)",
                 (unsigned)st.period_frames, st.period_frames * 1000.0 / st.device_rate, (unsigned)st.target_frames,
                 st.target_frames * 1000.0 / st.game_rate);
    if (g_about.no_rom)
        snprintf(a->rom, sizeof a->rom, "none (--no-rom)");
    else
        snprintf(a->rom, sizeof a->rom, "%s%s%s%s", rom, where && *where ? " (" : "", where ? where : "",
                 where && *where ? ")" : "");
    /* the controller paks chosen now: controller 1's path alone, or each with its controller */
    int n = 0, only1 = pc_si_pak_path(0) != NULL;
    for (int ch = 1; ch < 4; ch++)
        only1 &= !pc_si_pak_path(ch);
    for (int ch = 0; ch < 4; ch++) {
        const char *path = pc_si_pak_path(ch);
        size_t at = strlen(a->saves);
        if (path && only1)
            snprintf(a->saves, sizeof a->saves, "%s", path);
        else if (path)
            snprintf(a->saves + at, sizeof a->saves - at, "%sP%d %s", n++ ? ", " : "", ch + 1, path);
    }
    if (!a->saves[0])
        snprintf(a->saves, sizeof a->saves, "none (no controller pak)");
    snprintf(a->log, sizeof a->log, "%s", pc_app_log_path() ? pc_app_log_path() : "none");
    snprintf(a->data, sizeof a->data, "%s", g_about.data);
    pc_mods_summary(&g_mods, a->mods, sizeof a->mods);   /* docs/port/MODS.md */
    /* game.memory (docs/port/MEMORY.md 4): the window, and the heap's high-water mark so far */
    PcHeapStats hs;
    if (pc_heap_stats(&hs) == 0)
        snprintf(a->memory, sizeof a->memory, "%u MiB; heap %.1f of %.1f MiB at most in use", hs.mem_size >> 20,
                 hs.peak / 1048576.0, hs.size / 1048576.0);
    else
        snprintf(a->memory, sizeof a->memory, "%u MiB", hs.mem_size >> 20);
}

/* Whether the command line set this setting (its path is then from the working directory). */
static int from_command_line(const PcSettingValue *over, int n, const char *section, const char *key)
{
    int set = 0;
    for (int i = 1; i < n; i++)
        set |= !strcasecmp(over[i].section, section) && !strcasecmp(over[i].key, key);
    return set;
}

/* ---------------------------------------------------------------- the mods folder (docs/port/MODS.md) */

/* The ROM offset the sample directory's offsets count from: the sample base AudioInitSys hands sndPushGroup,
 * (s32)D_00039B40 (groups/audio_init_sys/car_shadow_render.c, docs/port/MUSYX_DATA.md). */
#define MODS_SAMPLE_BASE 0x39B40u

static PcMenuMod g_menu_mods[PC_MENU_MODS];

static int mods_flag(void *user, const char *folder)
{
    return pc_settings_mod_on(user, folder);
}

/* Before the RDRAM window is mapped (it needs the ROM): the mods, their files checked and settled, and the memory
 * they need, which may raise cfg->memory (pc_mods_prepare logs why). Then the overlay's Mods page. */
static int mods_prepare(const PcSettings *cfg, const PcAppDirs *dirs, int dir_from_command_line, int no_rom)
{
    char dir[PC_APP_PATH], cache[PC_APP_PATH], line[512];
    PcModsCart cart;
    PcModsImage img[8];
    int n = 0;
    pc_app_data_file(dirs, cfg->mods_dir, dir_from_command_line, dir, sizeof dir);
    pc_app_data_file(dirs, "mods_cache", 0, cache, sizeof cache);
    PcModsConfig mc = {cfg->mods && !no_rom, dir, cfg->mods_order, mods_flag, (void *)cfg, cfg->memory,
                       cfg->expansion_pak, cfg->cars, cache};
    pc_mods_scan(&g_mods, &mc);
    for (uint32_t i = 0; i < pc_data_tables.n_images && n < 8; i++, n++)
        img[n] = (PcModsImage){pc_data_tables.images[i].name, pc_data_tables.images[i].storage == PC_IMG_DEFLATE,
                               pc_data_tables.images[i].rom_off, pc_data_tables.images[i].rom_len,
                               pc_data_tables.images[i].size, pc_data_tables.images[i].n64_base};
    if (mc.enabled && pc_mods_cart_read(&cart, pc_rom_bytes(), pc_rom_size(), img, n) != 0) {
        fprintf(stderr, "rush2049pc: mods: the ROM's file table can't be read; no mod is used\n");
        g_mods.enabled = 0;
    }
    if (g_mods.enabled)
        pc_mods_prepare(&g_mods, &mc, &cart, MODS_SAMPLE_BASE);
    pc_mods_summary(&g_mods, line, sizeof line);
    if (no_rom && cfg->mods)
        fprintf(stderr, "rush2049pc: mods: not with --no-rom\n");
    else
        fprintf(stderr, "rush2049pc: mods: %s%s%s\n", line, g_mods.n || g_mods.enabled ? " in " : "",
                g_mods.n || g_mods.enabled ? dir : "");
    for (int i = 0; i < g_mods.n && i < PC_MENU_MODS; i++) {
        const PcMod *m = &g_mods.mod[i];
        PcMenuMod *o = &g_menu_mods[i];
        snprintf(o->folder, sizeof o->folder, "%s", m->folder);
        snprintf(o->name, sizeof o->name, "%s", m->name);
        snprintf(o->version, sizeof o->version, "%s", m->version);
        snprintf(o->author, sizeof o->author, "%s", m->author);
        snprintf(o->problem, sizeof o->problem, "%s", m->problem);
        o->priority = m->priority, o->files = m->nfiles, o->used = m->used, o->refused = m->refused;
        o->texts = m->nstrings, o->textures = m->textures[0] != 0;
    }
    pc_menu_set_mods(g_menu_mods, g_mods.n < PC_MENU_MODS ? g_mods.n : PC_MENU_MODS, dir);
    return g_mods.enabled ? g_mods.memory : cfg->memory;
}

/* Once the game's data is loaded: the file table's entries of the replaced waves point at the mods' files (pi.c's
 * mods region), and the renderer gets their texture packs. */
static void mods_install(void)
{
    char dirs[4 * PC_APP_PATH];
    int n = pc_mods_install(&g_mods, D_8011B5BC, D_8002E580, D_80123564);
    if (n)
        fprintf(stderr, "rush2049pc: mods: %d of the cartridge's files replaced (%zu KiB)\n", n,
                g_mods.region_size >> 10);
    pc_mods_texture_dirs(&g_mods, '\n', dirs, sizeof dirs);
    if (dirs[0])
        setenv("RUSH_GFX_TEXTURE_MODS", dirs, 1);
}

/* ---------------------------------------------------------------- game.saves = files (docs/port/SAVES.md) */

/* Whether the game can take a controller pak taken out and put back now, for another choice of saves on it (si.c's
 * swap): at the start prompt, "press start" or the main menu (gGameState 2, 4, 0x10), with no player on a saved
 * profile (each input record's profile handle a default one, &D_80146150[k], or none) and no ghost chosen for a
 * race (gGhosts). Elsewhere the change waits, at most until the next start. */
extern void *D_80146150[4];                                 /* the default profiles' handles */
extern struct { void **handle; uint8_t rest[72]; } D_8014A160[4];   /* gPlayers, seen from their profile handle */
extern void **D_80152698[4];                                /* gGhosts: the ghosts of the next race */

static int swap_safe(int ch)
{
    (void)ch;
    uint32_t s = game_state();
    if (s != 0x2 && s != 0x4 && s != 0x10)
        return 0;
    for (int i = 0; i < 4; i++) {
        void **h = *(void **volatile *)&D_8014A160[i].handle;
        if (h && (h < &D_80146150[0] || h >= &D_80146150[4]))
            return 0;
        if (*(void **volatile *)&D_80152698[i])
            return 0;
    }
    return 1;
}

/* The game's own note name prefix and ghost extension in the pak's character set (the game's data, read at run
 * time: never kept), for the ghosts imported into a folder (pakstore.c places them on the pak with them). */
extern char D_801211BC[], D_801211F4[], D_801211C8[];   /* the prefix SaveServer reads, the ghost's and profile's ext */
void func_800A150C(uint8_t *out, uint8_t *in, uint8_t limit);   /* ASCII to the pak's characters (the game's) */

static void saves_game_names(void)
{
    uint8_t prefix[8], ext[4], in[16];
    memset(in, 0, sizeof in);
    memcpy(in, D_801211BC, 8);
    func_800A150C(prefix, in, 8);
    memset(in, 0, sizeof in);
    memcpy(in, D_801211F4, 4);
    func_800A150C(ext, in, 4);
    /* SaveServer tells a ghost from a profile by the extension's first character: with no prefix, or a ghost
     * extension that starts as the profile's (a cartridge without the strings), an import keeps its own names */
    if (!prefix[0] || !ext[0] || !D_801211F4[0] || D_801211F4[0] == D_801211C8[0]) {
        pc_store_set_game_names(NULL, NULL);
        return;
    }
    pc_store_set_game_names(prefix, ext);
}

/* The F1 Saves page's facts and actions (port/input/menu.c) over pakstore.c. */
static char g_saves_mpk[4][PC_APP_PATH], g_saves_dir[4][PC_APP_PATH];

static int saves_backend(int player, char *where, size_t size)
{
    if (player < 0 || player > 3)
        return PC_SAVES_NONE;
    const char *dir = pc_store_dir(player);
    if (dir) {
        snprintf(where, size, "%s", dir);
        return PC_SAVES_FILES;
    }
    snprintf(where, size, "%s", g_saves_mpk[player]);
    return PC_SAVES_PAK;
}

static int saves_list(int player, PcSaveRow *out, int max)
{
    static PcStoreEntry e[512];
    int n = pc_store_list(player, e, max < 512 ? max : 512);
    for (int i = 0; i < n; i++) {
        PcSaveRow *r = &out[i];
        memset(r, 0, sizeof *r);
        snprintf(r->file, sizeof r->file, "%s", e[i].file);
        snprintf(r->label, sizeof r->label, "%s", e[i].label);
        snprintf(r->name, sizeof r->name, "%s", e[i].name);
        r->kind = e[i].kind, r->track = e[i].track, r->backward = e[i].backward, r->car = e[i].car;
        r->pages = e[i].pages, r->finish = e[i].finish;
        r->on_pak = e[i].on_pak, r->pinned = e[i].pinned, r->imported = e[i].imported, r->deleting = e[i].deleting;
    }
    return n;
}

static int saves_act(int player, int action, const char *file, char *msg, size_t size)
{
    switch (action) {
    case PC_SAVES_PIN:
    case PC_SAVES_UNPIN:
        return pc_store_pin(player, file, action == PC_SAVES_PIN, msg, size);
    case PC_SAVES_EXPORT:
        return pc_store_export_ghost(player, file, msg, size);
    case PC_SAVES_DELETE:
        return pc_store_delete(player, file, msg, size);
    case PC_SAVES_IMPORT:
        return pc_store_import(player, msg, size);
    case PC_SAVES_EXPORT_MPK:
        return pc_store_export_mpk(player, g_saves_mpk[player], msg, size);
    }
    snprintf(msg, size, "?");
    return -1;
}

static const PcSavesUi g_saves_ui = {saves_backend, saves_list, saves_act, pc_store_notice};

#define MAX_OVERRIDES 64

#ifdef SIGHUP
/* The terminal closed (SIGHUP's default ended the process at once): the loop stops as for SIGTERM, and its end
 * writes the saves (port/ultra/si.c's pc_si_finish). Not under nohup, which ignores it for the program. */
static void on_hangup(int sig)
{
    (void)sig;
    if (!pc_quit)
        pc_quit = 1 + 129;
}
#endif

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
    /* paced mode (docs/port/ULTRA.md 10): a mode a session turns on, not a setting; RUSH_PACED=1 for the tests */
    int paced = getenv("RUSH_PACED") && strcmp(getenv("RUSH_PACED"), "") && strcmp(getenv("RUSH_PACED"), "0");
    const char *bare_rom = NULL, *log_path = NULL;
    int use_log = 1;
    PcRunOptions run = {0};
    /* the command line's settings, in order; [0] for a press file. Static: the --set strings it points into live as
     * long as the program, and LeakSanitizer looks for them from here (docs/port/SANITIZE.md). */
    static PcSettingValue over[MAX_OVERRIDES + 1];
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
        else if (!strcmp(a, "--paced"))
            paced = 1;
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
        else if (!strcmp(a, "--log-cars"))
            run.end_info = cars_info, g_log_cars = 1;
        else if (!strcmp(a, "--log-physics"))
            run.physics_every = (uint32_t)atoi(ARG()), run.physics_info = physics_info;
        else if (!strcmp(a, "--log-voices")) {
            g_voices_every = (uint32_t)atoi(ARG());
            if (!g_voices_every)
                g_voices_every = 60;
            run.voices_every = g_voices_every, run.voices_info = voices_info;
        }
        else if (!strcmp(a, "--log-frames"))
            pc_game_frame_hook = frame_log;
        else if (!strcmp(a, "--press-via")) {
            if (pc_input_set_via(ARG()) != 0)
                return 2;
        }
        else if (!strcmp(a, "--pak")) {
            SET("game", "pak", "memory");
            SET("game", "pak_file", ARG());
        } else if (!strcmp(a, "--no-pak")) {
            SET("game", "pak", "none");
            SET("game", "pak2", "none");
            SET("game", "pak3", "none");
            SET("game", "pak4", "none");
        }
        else if (!strcmp(a, "--rumble"))
            SET("game", "pak", "rumble");
        else if (!strcmp(a, "--saves")) {   /* game.saves (docs/port/SAVES.md): every player's */
            const char *k = ARG();
            SET("game", "saves", k);
            SET("game", "saves2", k);
            SET("game", "saves3", k);
            SET("game", "saves4", k);
        }
        else if (!strcmp(a, "--audio"))
            SET("audio", "backend", ARG());
        else if (!strcmp(a, "--dump-audio"))
            dump_audio = ARG();
        else if (!strcmp(a, "--expansion-pak"))
            SET("game", "expansion_pak", "on");
        else if (!strcmp(a, "--mods-dir"))   /* docs/port/MODS.md */
            SET("game", "mods_dir", ARG());
        else if (!strcmp(a, "--no-mods"))
            SET("game", "mods", "off");
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
        } else if (!strcmp(a, "--preset")) {   /* its settings here, in order (port/config/presets.c) */
            const char *name = ARG();
            const PcPreset *p = pc_preset_find(name);
            if (!p) {
                fprintf(stderr, "rush2049pc: --preset takes n64, enhanced or maximum, not '%s'\n", name);
                return 2;
            }
            for (int k = 0; k < p->n; k++)
                SET(p->values[k].section, p->values[k].key, p->values[k].value);
        }
        else if (!strcmp(a, "--print-config"))
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
    /* The ROM: from the command line as given, else game.rom by the lookup order (port/app/paths.c). Read before the
     * RDRAM window is mapped: the mods (docs/port/MODS.md) need the cartridge's files to say how much memory they
     * need. */
    int rom_from_command_line = from_command_line(over, nover, "game", "rom"), memory = cfg.memory;
    if (!selftest) {
        if (no_rom ? pc_rom_blank() != 0
                   : pc_app_load_rom(&dirs, cfg.rom, rom_from_command_line, cfg.rom_check,
                                     cfg.source[0] ? cfg.source : cfg.save_path, cfg.source, !headless) != 0)
            return 1;
        memory = mods_prepare(&cfg, &dirs, from_command_line(over, nover, "game", "mods_dir"), no_rom);
    }
    /* game.memory (docs/port/MEMORY.md): the RDRAM window pc_mem_init maps below, and osMemSize */
    if (memory) {   /* the setting, or what the mods raised it to */
        uint32_t bytes = (uint32_t)memory << 20;
        if (pc_mem_set_size(bytes) != 0) {
            fprintf(stderr, "rush2049pc: game.memory = %d can't be mapped\n", memory);
            return 2;
        }
        pc_os_set_mem_size(bytes);
        fprintf(stderr, "rush2049pc: %d MiB of memory (game.memory%s)\n", memory,
                memory != cfg.memory ? ", for the mods" : "");
    }
    pc_race_cars_set(cfg.cars);   /* [game] cars: the game latches it at each race's start (race_cars.c) */
    /* [audio] voices, before the game threads set MusyX up; [audio] engines (voices.c, docs/port/AUDIO.md 10) */
    pc_voices_set(pc_settings_voices(&cfg));
    pc_engines_set(pc_settings_engines(&cfg));
    if (cfg.voices || cfg.engines) {
        char engines[16];
        pc_settings_get(&cfg, "audio", "engines", engines, sizeof engines);
        fprintf(stderr, "rush2049pc: %d voices (audio.voices), engines %s (audio.engines)\n", pc_mx_voices(), engines);
    }
    if (cfg.cars)
        fprintf(stderr, "rush2049pc: up to %d cars a race (game.cars)\n", pc_cars_setting());
    pc_game_fps_set(cfg.game_fps);   /* [game] frame_rate: the game latches it at each change of state (game_fps.c) */
    pc_start_menu_set(cfg.pc_setup);   /* [game] pc_setup: PC SETUP in the main menu (start_menu.c) */
    pc_ghost_prompt_set(cfg.ghost_prompt);   /* [game] ghost_prompt: the ghost save prompt for all (ghost_prompt.c) */
    pc_cheats_menu_set(cfg.cheats_menu);   /* [game] cheats_menu, cheats_unlock: the cheat screen (cheats.c) */
    pc_cheats_unlock_set(cfg.cheats_unlock);
    if (cfg.cheats_menu || cfg.cheats_unlock)
        fprintf(stderr, "rush2049pc: cheats: the main menu's CHEATS option %s (game.cheats_menu), every cheat "
                "available %s (game.cheats_unlock)\n", cfg.cheats_menu ? "on" : "off",
                cfg.cheats_unlock ? "on" : "off");
    if (cfg.game_fps)
        fprintf(stderr, "rush2049pc: the game runs at %d frames a second (game.frame_rate)\n", pc_game_fps_setting());
    /* port/gfx reads its settings from the environment; the resolved values already include it */
    char scale[16], aspect[16], rate[16];
    pc_settings_get(&cfg, "video", "scale", scale, sizeof scale);
    setenv("RUSH_GFX_SCALE", scale, 1);
    setenv("RUSH_GFX_API", cfg.api == PC_CFG_API_GL ? "gl" : cfg.api == PC_CFG_API_GLES ? "gles" : "auto", 1);
    pc_settings_get(&cfg, "video", "widescreen", aspect, sizeof aspect);
    setenv("RUSH_GFX_WIDESCREEN", aspect, 1);
    setenv("RUSH_GFX_FILL", cfg.fill ? "1" : "0", 1);
    pc_settings_get(&cfg, "video", "frame_rate", rate, sizeof rate);
    setenv("RUSH_GFX_FRAME_RATE", rate, 1);
    setenv("RUSH_GFX_TEXTURES", cfg.textures == 1 ? "upscaled" : cfg.textures == 2 ? "pack" : "original", 1);
    setenv("RUSH_GFX_HUD", cfg.hud == PC_GFX_HUD_CENTERED ? "centered" : "edges", 1);
    /* draw distance, detail and fog (docs/port/DETAIL.md): the game's part, and the renderer's */
    {
        char draw[16], detail[16], fog[16], texlod[16];
        pc_detail_set(cfg.draw_distance, cfg.detail, cfg.fog);
        pc_settings_get(&cfg, "video", "draw_distance", draw, sizeof draw);
        pc_settings_get(&cfg, "video", "detail", detail, sizeof detail);
        pc_settings_get(&cfg, "video", "fog", fog, sizeof fog);
        setenv("RUSH_GFX_DRAW_DISTANCE", draw, 1);
        setenv("RUSH_GFX_FOG", fog, 1);
        pc_settings_get(&cfg, "video", "texture_lod", texlod, sizeof texlod);
        setenv("RUSH_GFX_TEXTURE_LOD", texlod, 1);
        if (cfg.texture_lod)
            fprintf(stderr, "rush2049pc: texture_lod %s (docs/port/DETAIL.md)\n", texlod);
        if (cfg.draw_distance || cfg.detail || cfg.fog)
            fprintf(stderr, "rush2049pc: draw_distance %s, detail %s, fog %s (docs/port/DETAIL.md)\n", draw, detail,
                    fog);
    }
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
#ifndef RUSH_NOFIXED   /* RUSH_NOFIXED: the names are relative to the window, with nothing to fix up (NOFIXED.md 2) */
    pc_mem_fix_absolute();
#endif
    pc_fault_install();
    if (pc_mem_init() != 0) {
        fprintf(stderr, "rush2049pc: cannot map the RDRAM window\n");
        return 1;
    }
#ifndef RUSH_NOFIXED   /* RUSH_NOFIXED: the tables give the arenas in the window already */
    if (!pc_mem_fixed())
        pc_data_set_window(pc_n64_ptr);   /* the guard mode, or a host that can't map 0x80000000 (NOFIXED.md 5) */
#endif
    if (selftest) {
        pc_time_set_virtual(fast);
        if (paced)
            pc_time_set_paced(1);
        return pc_selftest();
    }

    pc_os_initialize();
    if (pc_data_init(pc_rom_bytes(), pc_rom_size()) != 0) {
        fprintf(stderr, "rush2049pc: the game's data could not be read from the ROM\n");
        if (!no_rom)
            return 1;   /* --no-rom goes on with the data zeroed */
    }
    if (no_rom)
        blank_files();
    mods_install();   /* docs/port/MODS.md */
    saves_game_names();   /* game.saves = files: what an imported ghost gets on the pak (docs/port/SAVES.md) */
    test_grow_waves();
    musyx_services(no_rom);
    /* RUSH2049_TEST_AUDIO_POOL_FAIL=N (tests): every Nth audio command finds the audio manager's message pool
     * empty, as on a starved host, so its callers' guards run in menus, race starts and object creation
     * (docs/port/QA.md "Integration soak", fix 6). N >= 2: the audio shutdown waits for a message, and with 1
     * none would ever come. */
    {
        extern int pc_audio_pool_fail_every;   /* src/blob/groups/slot18_alloc/func_80091B00.c */
        const char *e = getenv("RUSH2049_TEST_AUDIO_POOL_FAIL");
        if (e && *e) {
            int n = atoi(e);
            if (n >= 2) {
                pc_audio_pool_fail_every = n;
                fprintf(stderr, "rush2049pc: test: one audio command in %d finds the message pool empty\n", n);
            } else {
                fprintf(stderr, "rush2049pc: RUSH2049_TEST_AUDIO_POOL_FAIL takes a number from 2, not '%s'\n", e);
            }
        }
    }
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
    /* What the program writes goes into the data folder: next to it when portable, else the user data folder
     * (port/app/paths.c); paths from the command line are from the working directory. Each controller's pak file is
     * resolved now, so the overlay can choose memory later; controller 1's may be where an earlier build kept it. A
     * pak in use is backed up, or set aside when the game can't use it (port/app/saves.c), before the window opens:
     * its box comes first. */
    static char pak_path[4][PC_APP_PATH];
    char dir[PC_APP_PATH];
    for (int ch = 0; ch < 4; ch++) {
        static const char *const keys[4] = {"pak_file", "pak2_file", "pak3_file", "pak4_file"};
        int old = pc_app_pak_file(&dirs, cfg.pak_file[ch], from_command_line(over, nover, "game", keys[ch]),
                                  pak_path[ch], sizeof pak_path[ch]);
        snprintf(g_saves_mpk[ch], sizeof g_saves_mpk[ch], "%s", pak_path[ch]);
        if (cfg.saves[ch] == PC_CFG_SAVES_FILES) {
            /* game.saves = files (docs/port/SAVES.md): the folder, read now when the pak is in at start (else when
             * it is first chosen); the .mpk is only read, the first time, to import its saves */
            static const char *const dir_keys[4] = {"saves_dir", "saves2_dir", "saves3_dir", "saves4_dir"};
            char folder[PC_APP_PATH], msg[512];
            pc_app_data_file(&dirs, cfg.saves_dir[ch], from_command_line(over, nover, "game", dir_keys[ch]), folder,
                             sizeof folder);
            pc_store_configure(ch, folder, pak_path[ch], pc_app_version());
            snprintf(g_saves_dir[ch], sizeof g_saves_dir[ch], "%s", folder);
            if (cfg.pak[ch] != PC_CFG_PAK_MEMORY)
                continue;
            if (pc_store_open(ch, folder, pak_path[ch], pc_app_version(), msg, sizeof msg) == 0)
                fprintf(stderr, "rush2049pc: saves (controller %d's, files): %s: %s\n", ch + 1, folder, msg);
            else
                fprintf(stderr, "rush2049pc: saves (controller %d's, files): %s; no controller pak this time\n", ch + 1,
                        msg);
            continue;
        }
        if (cfg.pak[ch] != PC_CFG_PAK_MEMORY)
            continue;
        pc_app_dirname(pak_path[ch], dir, sizeof dir);
        pc_app_mkdirs(dir);
        if (ch == 0)
            fprintf(stderr, "rush2049pc: saves (the controller pak): %s%s\n", pak_path[ch],
                    old ? ", in the working directory, where earlier builds kept it" : "");
        else
            fprintf(stderr, "rush2049pc: saves (controller %d's pak): %s\n", ch + 1, pak_path[ch]);
        pc_app_saves_prepare(pak_path[ch], !headless);
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
    snprintf(g_about.data, sizeof g_about.data, "%s", dirs.data);
    g_about.no_rom = no_rom;
    pc_menu_set_about(about_fill);
    pc_menu_set_saves(&g_saves_ui);
    pc_app_data_file(&dirs, cfg.screenshot_dir[0] ? cfg.screenshot_dir : "screenshots",
                     cfg.screenshot_dir[0] && from_command_line(over, nover, "video", "screenshot_dir"), dir,
                     sizeof dir);
    snprintf(cfg.screenshot_dir, sizeof cfg.screenshot_dir, "%s", dir);
    if (!headless)
        pc_app_mkdirs(cfg.screenshot_dir);   /* F12's folder */
    if (pc_input_init(&cfg) != 0 && !headless)
        fprintf(stderr, "rush2049pc: no window: scripted input only\n");
    for (int ch = 0; ch < 4; ch++)   /* game.saves = files: the pak built from the folder (docs/port/SAVES.md) */
        if (g_saves_dir[ch][0])
            pc_si_set_pak_store(ch, g_saves_dir[ch]);
    pc_si_set_swap_check(swap_safe);
    pc_si_init(pak_path[0], cfg.pak[0]);   /* PC_CFG_PAK_* = PC_PAK_* */
    for (int ch = 1; ch < 4; ch++) {
        pc_si_set_pak_file(ch, pak_path[ch]);
        pc_si_set_pak(ch, cfg.pak[ch]);
    }
    if (paced)   /* the AI's queue the null output's on the N64 clock, a device fed from a tap (AUDIO.md 11) */
        pc_audio_set_paced(1);
    if (pc_audio_init(cfg.audio, dump_audio) != 0)
        fprintf(stderr, "rush2049pc: audio output falls back to null\n");
    pc_audio_set_volume(cfg.volume / 100.0);
    pc_audio_set_latency((unsigned)cfg.audio_latency);
    if (pc_verbose)
        pc_audio_set_report_every(10);   /* the output's counters every 10 s, besides the exit line */
#ifndef RUSH_NOFIXED   /* RUSH_NOFIXED: N64 addresses are translated, and the game has no fixed address left */
    if (!pc_mem_fixed())
        fprintf(stderr, "rush2049pc: warning: N64 addresses are translated; the game's fixed addresses fault\n");
#endif

    run.game_state = game_state;
    run.game_info = game_info;
    run.game_rate = game_rate;   /* game.frame_rate = 60 under vsync: the frames paced by the vblanks (FRAMERATE.md 7.6) */
    pc_gfx_set_hud_race(hud_race);
    if (fast || paced) {   /* paced mode is --fast's virtual clock, paced to the wall clock (port/ultra/pace.c) */
        pc_time_set_virtual(1);
        if (paced)
            pc_time_set_paced(1);
        pc_audio_set_clock(fast_clock);
    }
#ifdef SIGHUP
    struct sigaction hup;
    if (sigaction(SIGHUP, NULL, &hup) == 0 && hup.sa_handler != SIG_IGN)
        signal(SIGHUP, on_hangup);
#endif
    if (g_voices_every) {   /* --log-voices' end line, after --log-cars' if both */
        g_end_cars = run.end_info;
        run.end_info = ends_info;
    }
    pc_os_boot(n64_main, NULL, "boot");
    int code = pc_ultra_run(&run);
    pc_pools_report();   /* port/ultra/pools.c: the pools' high-water marks (docs/port/MORECARS.md 8) */
    pc_heap_report();    /* port/ultra/heap.c: the heap's (docs/port/MEMORY.md 4) */
    {
        uint32_t tasks, most;
        uint64_t tris;
        pc_gfx_scene_stats(&tasks, &tris, &most);
        pc_scene_report(tasks, tris, most);   /* port/config/detail.c: what the scene drew (docs/port/DETAIL.md) */
    }
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
