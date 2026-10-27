/* port/ultra/defaults.c: weak no-op versions of the other parts' interfaces (port/include/pc_port.h), so the
 * executable links and boots before port/data, port/gfx and port/audio are merged. Their strong definitions
 * replace these at link time; link them as object libraries (PC_PORT_LIBS in CMakeLists.txt), since a weak
 * definition already present stops the linker from pulling a member out of a static archive. */
#include <stdio.h>

#include "pc_port.h"

#define WEAK __attribute__((weak))

WEAK int pc_data_init(const uint8_t *rom, size_t rom_size)
{
    (void)rom, (void)rom_size;
    fprintf(stderr, "rush2049pc: port/data is not linked: the game's initialized data is zero\n");
    return 0;
}

WEAK void pc_data_overlay_load(int overlay) { (void)overlay; }

WEAK int pc_gfx_init(int headless, int window_width, int window_height)
{
    (void)headless, (void)window_width, (void)window_height;
    return 0;
}

WEAK void pc_gfx_run_dl(const void *dl) { (void)dl; }
WEAK void pc_gfx_present(const void *framebuffer) { (void)framebuffer; }
WEAK int pc_gfx_poll_events(void) { return 0; }
WEAK int pc_gfx_save_screenshot(const char *path) { (void)path; return -1; }
WEAK void pc_gfx_shutdown(void) {}

WEAK int pc_audio_init(const char *backend, const char *dump_wav_path)
{
    (void)backend, (void)dump_wav_path;
    return 0;
}

WEAK void pc_audio_shutdown(void) {}
WEAK void pc_audio_run_task(const void *cmdlist, size_t data_size) { (void)cmdlist, (void)data_size; }
WEAK void pc_audio_set_frequency(uint32_t hz) { (void)hz; }
WEAK void pc_audio_push(const int16_t *samples, size_t stereo_frames) { (void)samples, (void)stereo_frames; }
WEAK size_t pc_audio_queued_frames(void) { return 0; }
