/* port/include/pc_port.h: the interfaces between the parts of the PC layer (docs/PORT_PLAN.md).
 *
 *   port/ultra   libultra replacement, ROM, memory, threads, main loop (owns main())
 *   port/data    initial values of the game's data, read from the ROM by declared type
 *   port/gfx     F3DEX2 display-list interpreter and renderer
 *   port/audio   audio command-list interpreter and output
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
int pc_gfx_save_screenshot(const char *path);   /* last presented frame as PNG; 0 on success */
void pc_gfx_shutdown(void);

/* ---------------------------------------------------------------- audio (port/audio) */

/* Output setup, from main before the game starts. backend: "auto" (SDL2, then ALSA, then null), "sdl",
 * "alsa", "null" or "wav"; dump_wav_path: NULL, or a WAV file that receives every pushed frame
 * (--dump-audio). 0 on success; otherwise output falls back to null. Without it, the first audio call
 * initializes from the environment (port/audio/pc_audio.h). */
int pc_audio_init(const char *backend, const char *dump_wav_path);
void pc_audio_shutdown(void);
/* One audio task: OSTask.t.data_ptr / data_size, as the game sets them. Runs to completion. The game's
 * microcode is MusyX v1, not Acmd: data_ptr is an array of subframe descriptors and data_size is how many
 * (not bytes); each writes 192 stereo frames into an AI buffer (port/audio/musyx_abi.h). */
void pc_audio_run_task(const void *cmdlist, size_t data_size);
/* osAiSetFrequency / osAiSetNextBuffer / osAiGetLength. Samples are the game's interleaved stereo s16 in
 * native byte order. */
void pc_audio_set_frequency(uint32_t hz);
void pc_audio_push(const int16_t *samples, size_t stereo_frames);
size_t pc_audio_queued_frames(void);

/* ---------------------------------------------------------------- input (port/ultra for now) */

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
