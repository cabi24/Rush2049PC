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
/* The same in float, full scale 1.0: the converter's overshoot past full scale is kept (pc_audio_pull clamps it and
 * counts the samples in PcAudioStats.clipped). SDL's backend pulls this way. */
size_t pc_audio_pull_f32(float *out, size_t frames);
/* pc_audio_set_volume (pc_port.h) scales what pc_audio_pull returns. */
uint32_t pc_audio_device_rate(void);   /* 0 until a rate is known */
const char *pc_audio_backend_name(void);

/* The interpreter wrote one of the game's AI buffers. The queue matches it to the AI's push of that buffer it was
 * rendered for: one already pushed waits in the queue for it (PcAudioStats.waited). */
void pc_audio_note_rendered(const int16_t *buf);

typedef struct {
    uint64_t pushed_frames;     /* game-rate frames given to pc_audio_push */
    uint64_t dropped_frames;    /* device-rate frames dropped because the queue was full */
    uint64_t underrun_frames;   /* device-rate frames that found the queue empty while the game played (silence) */
    uint64_t underruns;         /* times the queue ran dry while the game played */
    uint64_t idle_frames;       /* device-rate frames of silence while the game pushed nothing for 0.25 s */
    uint64_t waited;            /* AI buffers pushed before the task that renders them for that push had run (and,
                                 * before renders are matched, pushed again without a render since) */
    uint64_t rescued;           /* of those, rendered before the device needed them: played as on the N64 */
    uint64_t played_late;       /* of those, played with the buffer's previous render, which hadn't played */
    uint64_t silenced;          /* of those, played as silence */
    uint64_t missed;            /* of the silenced, where the buffer held sound (the N64 plays it again) */
    uint64_t skipped;           /* renders a later one replaced before they played, or dropped by a new match */
    uint64_t rematched;         /* times the renderer's position was matched anew (a restart, a stall) */
    uint64_t renderer_skips;    /* buffers the game's renderer passed over: a render further on in the ring
                                 * within 5 ms of the one before (a restart comes after a retrace or more) */
    uint64_t clipped;           /* samples pc_audio_pull clamped to 16 bits: the converter's overshoot of loud input */
    uint64_t pulls;
    uint64_t level_sum;         /* the queue at each pull while the game plays, device-rate frames: sum, count, */
    uint64_t level_count;       /* least and most (after its first second) */
    uint32_t level_min, level_max;
    uint32_t device_rate;
    uint32_t period_frames;     /* the largest pull a device made, device-rate frames */
    uint32_t target_frames;     /* the queue the AI keeps (pc_audio_target_frames), game-rate frames */
    uint32_t max_queued;        /* the most the queue held, device-rate frames */
    uint32_t game_rate;         /* the game's rate (osAiSetFrequency), Hz; target_frames is at this rate */
} PcAudioStats;
void pc_audio_get_stats(PcAudioStats *stats);

#ifdef __cplusplus
}
#endif

#endif
