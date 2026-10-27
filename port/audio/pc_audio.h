/* port/audio/pc_audio.h: port/audio's interface beyond pc_port.h, for the rest of the PC layer and the tests.
 * pc_port.h has the shared entry points (pc_audio_init, pc_audio_run_task, pc_audio_set_frequency,
 * pc_audio_push, pc_audio_queued_frames). */
#ifndef PC_AUDIO_H
#define PC_AUDIO_H

#include <stddef.h>
#include <stdint.h>

#include "pc_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Which form of the task records pc_audio_run_task reads (musyx_abi.h). The build's default comes from the
 * CMake cache variable PC_AUDIO_TASK_FORM (NATIVE or N64). */
enum { PC_AUDIO_FORM_NATIVE = 0, PC_AUDIO_FORM_N64 = 1 };
void pc_audio_set_task_form(int form);
int pc_audio_task_form(void);

/* Run a task in an explicit form; pc_audio_run_task uses the current one. */
void pc_audio_run_task_form(const void *sfds, size_t count, int form);

/* Output. Backends: "auto" (SDL2, then ALSA, then null), "sdl", "alsa", "null" (discards in real time),
 * "wav" (null plus the dump file, which it requires), "manual" (holds samples until pc_audio_pull; tests).
 * Without pc_audio_init, the first call initializes from the environment: RUSH2049_AUDIO (backend) and
 * RUSH2049_DUMP_AUDIO (path). RUSH2049_AUDIO_RATE forces the device rate; RUSH2049_ALSA_DEVICE names the
 * ALSA device ("default"). */

/* Device-rate stereo frames out of the queue; the rest of out is zero-filled. Returns frames that were
 * queued. Backends call this from their own thread. */
size_t pc_audio_pull(int16_t *out, size_t frames);
uint32_t pc_audio_device_rate(void);   /* 0 until a rate is known */
const char *pc_audio_backend_name(void);

typedef struct {
    uint64_t pushed_frames;     /* game-rate frames given to pc_audio_push */
    uint64_t dropped_frames;    /* device-rate frames dropped because the queue was full */
    uint64_t underrun_frames;   /* device-rate frames pulled from an empty queue (played as silence) */
} PcAudioStats;
void pc_audio_get_stats(PcAudioStats *stats);

#ifdef __cplusplus
}
#endif

#endif
