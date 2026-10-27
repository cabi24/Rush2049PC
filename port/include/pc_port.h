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

/* A high frame rate (video.frame_rate; docs/port/GFX.md "High frame rate"). 0, the default, is the game's own: each
 * frame is shown when the VI swaps it in (pc_gfx_present). Otherwise the main loop shows a frame this many times a
 * second (pc_gfx_present_tick), drawn between the game's last two; -1 is the display's refresh rate. The game's
 * timing doesn't change: pc_gfx_present still only marks when each frame is swapped in, and screenshots still
 * read the game's frames. */
void pc_gfx_set_frame_rate(int rate);
int pc_gfx_frame_rate(void);                 /* frames shown a second (-1 resolved), 0 for the game's own */
void pc_gfx_retrace(uint32_t vi);            /* each retrace, before the VI's present: its number */
void pc_gfx_present_tick(double vi_time);    /* show the frame for this moment, in retraces (pc_gfx_retrace's) */

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

/* ---------------------------------------------------------------- input (port/input) */

typedef struct {
    uint16_t buttons;   /* N64 button bits (CONT_A 0x8000 ... as libultra) */
    int8_t stick_x;
    int8_t stick_y;
    int connected;
} PcPad;
void pc_input_poll(PcPad pads[4]);

#ifdef __cplusplus
}
#endif

#endif
