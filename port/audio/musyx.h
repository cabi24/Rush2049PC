/* port/audio/musyx.h: the MusyX v1 interpreter's stages, exposed for port/audio's tests. */
#ifndef MUSYX_H
#define MUSYX_H

#include <stddef.h>
#include <stdint.h>

#include "musyx_abi.h"

/* Run count SFDs (pc_audio.h's PC_AUDIO_FORM_*). */
void musyx_run_task(const void *sfds, size_t count, int form);

/* Decode frames 32-sample ADPCM frames from src (40-byte blocks of two frames: both frames' first two samples
 * as big-endian s16, then each frame's header byte and 15 bytes of nibbles), starting at the first frame
 * when skip < 32 and at the second when skip & 32. Bytes past src_len read as zero. dst holds frames * 32. */
void musyx_adpcm_decode(int16_t *dst, const uint8_t *src, size_t src_len, const int16_t book[128],
                        unsigned frames, unsigned skip);

/* The 4-tap resample filter: 64 phases, Q15 taps for input samples [0..3], interpolating at 1 + phase/64. */
extern int16_t musyx_lut[64][4];
void musyx_lut_init(void);                    /* once: from the ROM's microcode data if found, else computed */
void musyx_lut_compute(int16_t lut[64][4]);   /* this port's own table: Hann-windowed sinc, cutoff 0.8 Nyquist */
/* Find a table of this shape in big-endian bytes (the microcode's data). Offset, or -1. */
long musyx_lut_find(const uint8_t *data, size_t size, int16_t lut[64][4]);
const char *musyx_lut_source(void);           /* "rom+0x...", "computed" */

#endif
