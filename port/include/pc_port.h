/* port/include/pc_port.h: the interfaces between the parts of the PC layer (docs/PORT_PLAN.md).
 *
 *   port/ultra   libultra replacement, ROM, memory, threads, main loop (owns main())
 *   port/data    initial values of the game's data, read from the ROM by declared type
 *   port/gfx     F3DEX2 display-list interpreter and renderer
 *   port/audio   audio command-list interpreter and output
 *   port/assets  the game's files from the ROM, converted to the host's byte order
 *   port/input   keyboard, game controllers, scripted presses (port/config: rush2049pc.ini)
 *
 * PC-layer code only: game C never includes this (it carries its own declarations). Each part implements its
 * functions; port/ultra links weak no-op defaults for any part that isn't built yet. */
#ifndef PC_PORT_H
#define PC_PORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------- ROM and memory (port/ultra) */

/* The cartridge image in big-endian (.z64) byte order, loaded and checked at startup. */
const uint8_t *pc_rom_bytes(void);
size_t pc_rom_size(void);

/* The N64's RDRAM as the game addresses it outside its own symbols (the heap, frame buffers, the overlay
 * region at 0x8038A400). Mapped at host address 0x80000000 where the OS allows it, so N64 addresses in that
 * range are host addresses; elsewhere these translate. */
#define PC_RDRAM_BASE 0x80000000u
#define PC_RDRAM_SIZE 0x00800000u
void *pc_n64_ptr(uint32_t n64_addr);      /* K0/K1/physical address -> host pointer (NULL if outside RDRAM) */
uint32_t pc_n64_addr(const void *host);   /* host pointer into RDRAM -> K0 address (0 if outside) */

/* Ranges that hold raw big-endian bytes from the ROM (asset buffers filled by DMA). The renderer and the
 * audio interpreter read vertices, matrices, 16-bit texels and command lists in N64 byte order there. */
void pc_mem_register_be(const void *start, size_t size);
void pc_mem_unregister_be(const void *start);
int pc_mem_is_be(const void *p);

/* ---------------------------------------------------------------- game data (port/data) */

/* Fill the boot and game images' initialized data from the ROM. 0 on success. */
int pc_data_init(const uint8_t *rom, size_t rom_size);

/* An overlay "load": both runtime images share 0x8038A400 on the N64; natively both are linked in, and a load
 * resets that image's data to its initial values and zeroes its BSS. */
enum { PC_OVERLAY_A = 0, PC_OVERLAY_B = 1 };
void pc_data_overlay_load(int overlay);

/* ---------------------------------------------------------------- the game's files (port/assets) */

/* _ReadWave, after a wave's file is in its heap block and before SetupWaveModel relocates it: convert it to the
 * host's byte order by format (docs/port/ASSETS.md §5). size is the block's size. */
void pc_wave_loaded(int wave, void *buf, size_t size);
/* The wave's block is freed or moves: drop its big-endian registrations. */
void pc_wave_released(int wave, void *buf);

/* ---------------------------------------------------------------- graphics (port/gfx), host main thread */

/* Display-list command words on TARGET_PC: Gfx (and Acmd) keep w0 as 32 bits and widen w1 to hold a host
 * pointer. 16 bytes per command. Lists in a pc_mem_register_be range are N64 form instead: 8 bytes, big-endian,
 * 32-bit (segmented or K0) addresses. */
typedef struct {
    uint32_t w0;
    uintptr_t w1;
} PcGfxWords;

int pc_gfx_init(int headless, int window_width, int window_height);   /* 0 on success */
void pc_gfx_run_dl(const void *dl);          /* one graphics task: OSTask.t.data_ptr */
void pc_gfx_present(const void *framebuffer);   /* osViSwapBuffer: show the frame rendered into it */
int pc_gfx_poll_events(void);                /* window and input events; nonzero when the user quits */
/* The window's events for port/input: pc_gfx_poll_events passes each one (an SDL_Event *) to hook before its
 * own handling, and a nonzero return quits. NULL removes it. 0 when there is an SDL window, -1 otherwise
 * (headless, or no SDL2), and the hook is then never called. */
int pc_gfx_set_event_hook(int (*hook)(const void *sdl_event));
int pc_gfx_save_screenshot(const char *path);   /* last presented frame as PNG; 0 on success */
void pc_gfx_shutdown(void);

/* The window and the display (docs/port/GFX.md "Window and display"). Sizes are pixels (the drawable's, at any
 * display scaling). pc_gfx_init's window size 0 x 0 is two thirds of the desktop (headless: 320 x 240, the N64's
 * frame). Without a window these keep their values; pc_gfx_set_window_pixels then sets the headless size. */
enum { PC_GFX_FULLSCREEN_DESKTOP = 0, PC_GFX_FULLSCREEN_EXCLUSIVE = 1 };   /* video.fullscreen_mode's order */
void pc_gfx_set_fullscreen_mode(int mode);       /* what fullscreen means; applies at once when in fullscreen */
/* video.resolution, 0 x 0 for the desktop's: exclusive fullscreen's display mode (the closest one), and the size
 * desktop fullscreen draws the frame at (fitted to the screen). */
void pc_gfx_set_resolution(int w, int h);
int pc_gfx_set_fullscreen(int on);               /* 0 on success */
int pc_gfx_toggle_fullscreen(void);              /* Alt+Enter, the fullscreen hotkey */
int pc_gfx_is_fullscreen(void);                  /* -1 without a window */
int pc_gfx_set_vsync(int on);                    /* 0 on success */
void pc_gfx_set_window_pixels(int w, int h);     /* the window's size (applies after fullscreen when in it) */
void pc_gfx_set_window_position(int x, int y);   /* before pc_gfx_init: where the window opens, if on a display */
/* The window's last place and size out of fullscreen (video.remember_window): x, y in the desktop's coordinates,
 * w x h pixels. -1 without a window. */
int pc_gfx_window_geometry(int *x, int *y, int *w, int *h);

/* A high frame rate (video.frame_rate; docs/port/GFX.md "High frame rate"). 0, the default, is the game's own: each
 * frame is shown when the VI swaps it in (pc_gfx_present). Otherwise the main loop shows a frame this many times a
 * second (pc_gfx_present_tick), drawn between the game's last two; -1 is the display's refresh rate. The game's
 * timing doesn't change: pc_gfx_present still only marks when each frame is swapped in, and screenshots still
 * read the game's frames. */
void pc_gfx_set_frame_rate(int rate);
int pc_gfx_frame_rate(void);                 /* frames shown a second (-1 resolved), 0 for the game's own */
void pc_gfx_retrace(uint32_t vi);            /* each retrace, before the VI's present: its number */
/* Show the frame for this moment, in retraces (pc_gfx_retrace's). mode 0: as the moment says (--fast); 1: on the
 * host's clock, a frame between two isn't drawn while the GPU hasn't finished what it was given; 2: no frame is drawn
 * between two, only the game's are shown (the host is behind). 1 when something new was shown, 0 if not. */
int pc_gfx_present_tick(double vi_time, int mode);
/* A mode bit: the frame is drawn into the window and its swap left to pc_gfx_swap_tick (the main loop times it, and
 * with vsync swaps when the vblank it was drawn for comes). pc_gfx_vsync: 1 when the window's swap waits for vsync;
 * pc_gfx_display_hz: the display's refresh (60 without one). */
#define PC_GFX_TICK_NO_SWAP 4
void pc_gfx_swap_tick(void);
/* 1 when a tick at vi_time would draw a frame between the one shown and the game's next, which isn't drawn yet. */
int pc_gfx_tick_waits(double vi_time);
int pc_gfx_vsync(void);
int pc_gfx_display_hz(void);
/* The display's exact refresh where the platform gives it (docs/port/FRAMERATE.md 2: Windows' display configuration
 * and compositor, X11's XRandR): bit 1, *period_ns is its period; bit 2, a vblank was *vblank_age_ns ago (negative: to
 * come). 0: nothing beyond pc_gfx_display_hz's whole hertz (headless, Wayland). */
int pc_gfx_display_timing(double *period_ns, int64_t *vblank_age_ns);
/* 1 when the game has swapped in a frame the window hasn't shown yet (a high frame rate): a tick the main loop
 * skips to keep the retraces on time still shows it, as the setting off would at its swap. */
int pc_gfx_frame_waiting(void);
/* Textures (docs/port/TEXTURES.md): 0 original, 1 upscaled (3D textures, by the internal scale), 2 pack (replacements
 * from the texture pack folder). From the next frame. pc_gfx_parse_textures: "original", "upscaled", "pack"; *ok = 0
 * for anything else. */
void pc_gfx_set_textures(int mode);
int pc_gfx_parse_textures(const char *text, int *ok);
/* The race HUD in widescreen (docs/port/GFX.md "The HUD at the screen's edges"). PC_GFX_HUD_EDGES (the default) moves
 * the HUD's pieces at the 4:3 frame's sides out by the widening, so they keep their distance to the screen's edges,
 * while race() says a race with one view is on; PC_GFX_HUD_CENTERED keeps all 2D in the 4:3 frame. From the next
 * graphics task. pc_gfx_parse_hud: "edges", "centered"; *ok = 0 for anything else. */
enum { PC_GFX_HUD_CENTERED = 0, PC_GFX_HUD_EDGES = 1 };
void pc_gfx_set_hud(int mode);
int pc_gfx_hud(void);
int pc_gfx_parse_hud(const char *text, int *ok);
void pc_gfx_set_hud_race(int (*race)(void));
/* Where the HUD moves a point at frame x (N64 pixels of the 4:3 frame), in N64 pixels: what the game's 3D models
 * placed at HUD points (ZOID_WindowTo3D) add to their x. 0 unless edges is in effect. */
int pc_gfx_hud_dx(int x);

/* Drawing over the frame, in window pixels (port/gfx/gfx_ui.c), for port/input's settings overlay. While a UI hook
 * is set, each present (and pc_gfx_redraw) calls it with the window's size after the frame is shown and before
 * the swap; it draws with pc_gfx_ui_rect and pc_gfx_ui_text. Screenshots keep the frame without it. NULL removes
 * it. Colors are 0xRRGGBBAA, blended over what is there. */
void pc_gfx_set_ui_hook(void (*draw)(int window_w, int window_h));
void pc_gfx_ui_rect(int x, int y, int w, int h, uint32_t rgba);
/* Printable ASCII in the port's own font (gfx_font.c): 5 x 9 pixels per glyph at scale 1 (7 above the baseline),
 * 6 apart; y is the top of the cell. Returns the width drawn. */
int pc_gfx_ui_text(int x, int y, int scale, uint32_t rgba, const char *text);
#define PC_GFX_FONT_W 5
#define PC_GFX_FONT_H 9
#define PC_GFX_FONT_ADVANCE 6
/* A glyph's rows, top first; bit 4 is the leftmost column. 0 for a character the font lacks. */
int pc_gfx_font_glyph(int c, uint8_t rows[PC_GFX_FONT_H]);
/* Show the last presented frame again, with the UI over it, and swap (the overlay while the game is held). */
void pc_gfx_redraw(void);
/* The GL driver as the log's "gfx:" line names it: GL_VENDOR, GL_RENDERER, GL_VERSION and the shading language's
 * version; "" before pc_gfx_init. For the overlay's About page. */
void pc_gfx_driver(const char **vendor, const char **renderer, const char **version, const char **glsl);
/* Called by pc_gfx_poll_events after the window's events, also without a window: port/input's turn at each
 * retrace and while the game is held. A nonzero return quits. NULL removes it. */
void pc_gfx_set_tick_hook(int (*tick)(void));

/* ---------------------------------------------------------------- audio (port/audio) */

/* Output setup, from main before the game starts. backend: "auto" (SDL2, then ALSA, then null), "sdl",
 * "alsa", "null" or "wav"; dump_wav_path: NULL, or a WAV file that receives every pushed frame
 * (--dump-audio). 0 on success; otherwise output falls back to null. Without it, the first audio call
 * initializes from the environment (port/audio/pc_audio.h). */
int pc_audio_init(const char *backend, const char *dump_wav_path);
void pc_audio_shutdown(void);
/* The master volume, 0-1 (audio.volume / 100), on the output device; 1 leaves the samples as they are. */
void pc_audio_set_volume(double volume);
/* The clock, in seconds, by which the null backend drains the queue: NULL for the host's (the default); --fast
 * gives it the N64 clock. */
void pc_audio_set_clock(double (*now_seconds)(void));
/* One audio task: OSTask.t.data_ptr / data_size, as the game sets them. Runs to completion. The game's
 * microcode is MusyX v1, not Acmd: data_ptr is an array of subframe descriptors and data_size is how many
 * (not bytes); each writes 192 stereo frames into an AI buffer (port/audio/musyx_abi.h). */
void pc_audio_run_task(const void *cmdlist, size_t data_size);
/* osAiSetFrequency / osAiSetNextBuffer / osAiGetLength. Samples are the game's interleaved stereo s16 in
 * native byte order. */
void pc_audio_set_frequency(uint32_t hz);
void pc_audio_push(const int16_t *samples, size_t stereo_frames);
size_t pc_audio_queued_frames(void);
/* The AI's pacing: the game frames to keep queued (from the device's pulls; it grows after an underrun), and a
 * function the device's thread calls after each pull, so the AI can ask for buffers without the main thread. */
size_t pc_audio_target_frames(void);
void pc_audio_set_pull_hook(void (*hook)(void));
/* The least the queue keeps, in milliseconds (audio.latency); 0: from the device. */
void pc_audio_set_latency(unsigned ms);
/* The main loop holds the game: the AI asks for no buffers, and the silence isn't counted as underruns. */
void pc_audio_set_paused(int paused);
/* The output's counters (underruns, the queue's level, buffers late for their render...) go to the log at exit;
 * with this, also every `seconds` while the game plays (-v: 10). pc_audio_periodic is the main loop's, about once a
 * second. */
void pc_audio_set_report_every(unsigned seconds);
void pc_audio_periodic(void);

/* ---------------------------------------------------------------- input (port/input) */

typedef struct {
    uint16_t buttons;   /* N64 button bits (CONT_A 0x8000 ... as libultra) */
    int8_t stick_x;
    int8_t stick_y;
    int connected;
    int rumble;         /* its controller has a rumble motor (an accessory set to auto is then a rumble pak) */
} PcPad;
void pc_input_poll(PcPad pads[4]);

#ifdef __cplusplus
}
#endif

#endif
