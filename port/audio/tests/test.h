/* port/audio/tests/test.h: shared by port/audio's tests. */
#ifndef PCAUDIO_TEST_H
#define PCAUDIO_TEST_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "musyx.h"
#include "pc_audio.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern int test_failures, test_checks;
#define CHECK(cond, ...)                                                                   \
    do {                                                                                   \
        test_checks++;                                                                     \
        if (!(cond)) {                                                                     \
            test_failures++;                                                               \
            fprintf(stderr, "%s:%d: FAIL: %s: ", __FILE__, __LINE__, #cond);               \
            fprintf(stderr, __VA_ARGS__);                                                  \
            fputc('\n', stderr);                                                           \
        }                                                                                  \
    } while (0)
int test_finish(const char *name);   /* prints the tally; the exit status */

/* A small deterministic generator: 0..0x7FFF. */
static inline int test_rand(unsigned *seed)
{
    *seed = *seed * 1103515245u + 12345u;
    return (int)(*seed >> 16 & 0x7FFF);
}

/* ---------------------------------------------------------------- stubs.c: port/ultra stand-ins */

/* A fake RDRAM at N64 address 0x80000000, for N64-form records. */
#define TEST_RDRAM_SIZE 0x400000u
extern uint8_t test_rdram[TEST_RDRAM_SIZE];
void *rdram_alloc(size_t size);   /* zeroed, 16-byte aligned */
void rdram_reset(void);
uint32_t rdram_addr(const void *p);   /* K0 address */
void test_set_rom(const uint8_t *rom, size_t size);

/* ---------------------------------------------------------------- ref.c */

/* Reference decode of one ADPCM frame given in frame order: raw[4], header, 15 nibble bytes. */
void ref_adpcm_frame(const uint8_t fr[20], const int16_t book[128], int16_t out[32]);
/* Two frames (frame order) into the 40-byte block layout the microcode reads. */
void ref_pack_block(const uint8_t a[20], const uint8_t b[20], uint8_t blk[40]);
/* Order-2 predictor x[n] = a1 x[n-1] + a2 x[n-2] as one codebook entry (16 s16, Q11). */
void ref_make_book(int16_t entry[16], double a1, double a2);
/* Encode frames (an even count) of pcm with book entry idx; out gets frames / 2 * 40 bytes. */
void ref_adpcm_encode(const int16_t *pcm, unsigned frames, const int16_t book[128], int idx, uint8_t *out);

/* The game's voice builder (func_80012D18, RAM-resident sample path) and position update (func_80011D74). */
typedef struct {
    const uint8_t *data;   /* PCM16: the samples; ADPCM: a 256-byte codebook, then blocks */
    int format;            /* 0 PCM16, 3 ADPCM */
    uint32_t length, loop_start, loop_length;
    double position;
    uint16_t rate;         /* Q4.12 */
    int16_t level[4];
    int first;             /* the first subframe starts at the target volume */
} TbVoice;
void tb_voice(TbVoice *tv, PcMusyxVoice *cmd, const int16_t target[4]);

/* Measurements over n frames of one channel of interleaved stereo (stride 2) or mono (stride 1). */
double tone_freq(const int16_t *x, size_t n, int stride, double rate);
/* Fit A sin(w n + p) + c at freq: returns A; *resid gets the residual RMS. */
double tone_fit(const int16_t *x, size_t n, int stride, double rate, double freq, double *resid);
/* The same, and *worst gets the largest deviation from the fit. */
double tone_fit2(const int16_t *x, size_t n, int stride, double rate, double freq, double *resid, double *worst);

#endif
