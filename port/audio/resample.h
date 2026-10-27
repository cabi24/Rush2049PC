/* port/audio/resample.h: stereo s16 rate conversion from the game's AI rate to the device's. */
#ifndef PC_RESAMPLE_H
#define PC_RESAMPLE_H

#include <stddef.h>
#include <stdint.h>

#define RS_TAPS 32
#define RS_PHASES 256
#define RS_CHUNK 512   /* input frames buffered at a time */

typedef struct PcResampler {
    uint32_t in_rate, out_rate;
    uint64_t step;   /* input frames per output frame, 32.32 */
    uint64_t pos;    /* next output's position in hist, 32.32 */
    size_t len;      /* frames in hist */
    float hist[RS_TAPS + RS_CHUNK][2];
    float coef[RS_PHASES + 1][RS_TAPS];
} PcResampler;

/* (Re)configure; history is cleared. Equal rates pass samples through unchanged. */
void rs_init(PcResampler *rs, uint32_t in_rate, uint32_t out_rate);
/* Output frames that n input frames can produce, at most. */
size_t rs_max_out(const PcResampler *rs, size_t n);
/* Convert n frames; out must hold rs_max_out(n). Returns frames written. */
size_t rs_process(PcResampler *rs, const int16_t *in, size_t n, int16_t *out);

#endif
