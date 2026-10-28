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

/* Device-rate stereo frames out of the queue; past what was queued, out gets the last frame faded to silence
 * over 32 frames, then zeros. Returns frames that were queued. Backends call this from their own thread; it then
 * calls the pull hook (pc_audio_set_pull_hook). */
size_t pc_audio_pull(int16_t *out, size_t frames);
/* pc_audio_set_volume (pc_port.h) scales what pc_audio_pull returns. */
uint32_t pc_audio_device_rate(void);   /* 0 until a rate is known */
const char *pc_audio_backend_name(void);

/* The interpreter wrote one of the game's AI buffers (for PcAudioStats.replayed). */
void pc_audio_note_rendered(const int16_t *buf);

typedef struct {
    uint64_t pushed_frames;     /* game-rate frames given to pc_audio_push */
    uint64_t dropped_frames;    /* device-rate frames dropped because the queue was full */
    uint64_t underrun_frames;   /* device-rate frames that found the queue empty while the game played (silence) */
    uint64_t underruns;         /* times the queue ran dry while the game played */
    uint64_t idle_frames;       /* device-rate frames of silence while the game pushed nothing for 0.25 s */
    uint64_t replayed;          /* AI buffers pushed again before a task rendered them anew (old sound repeated) */
    uint64_t pulls;
    uint32_t device_rate;
    uint32_t period_frames;     /* the largest pull a device made, device-rate frames */
    uint32_t target_frames;     /* the queue the AI keeps (pc_audio_target_frames), game-rate frames */
    uint32_t max_queued;        /* the most the queue held, device-rate frames */
} PcAudioStats;
void pc_audio_get_stats(PcAudioStats *stats);

#ifdef __cplusplus
}
#endif

#endif
