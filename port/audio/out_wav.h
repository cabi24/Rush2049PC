/* port/audio/out_wav.h: a 16-bit stereo PCM WAV writer (--dump-audio and tests). */
#ifndef PC_OUT_WAV_H
#define PC_OUT_WAV_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct PcWav {
    FILE *f;
    uint32_t rate;
    uint64_t frames;
    uint64_t frames_at_patch;
} PcWav;

int pc_wav_open(PcWav *w, const char *path, uint32_t rate);   /* 0 on success */
void pc_wav_set_rate(PcWav *w, uint32_t rate);   /* before the first frame */
void pc_wav_write(PcWav *w, const int16_t *frames, size_t n);
void pc_wav_close(PcWav *w);

/* Read one back (tests): 16-bit stereo PCM only. Returns frames (malloc'd into *out) or -1. */
long pc_wav_read(const char *path, int16_t **out, uint32_t *rate);

#endif
