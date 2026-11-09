/* src/rom/speech/gsm.c: the speech module's GSM 06.10 full-rate decoder (the frame level).
 *
 * Unpacks a frame's parameters (8 log-area ratios, then per 40-sample subframe the long-term lag and gain, the
 * RPE grid, its block maximum and 13 pulses) and runs the three kernels on them: RPE decoding and long-term
 * synthesis per subframe, short-term synthesis per frame. The kernels (gsm_rpe_decode, gsm_long_term_synthesis,
 * gsm_short_term_filter) were hand-written assembly in the boot segment; port/ultra/gsm.c has them in C, and
 * port/ultra/gsm.h documents the decoder state. The stream form reads the speech bank's own packing: frames as
 * LSB-first bit fields in a 4 KiB ring, with run-length silence and repeat codes (speech/decode.c fills the ring).
 * MusyX-side code with no MAT3 counterpart. The game never starts the speech module (nothing calls speech_init),
 * so on the PC only port/ultra/tests (test_gsm, which compiles this file) runs it. */
#include "../rom_tu.h"

#define GSM_FRAME_SAMPLES 160                     /* 20 ms at 8 kHz: four subframes of 40 */
#define GSM_FRAME_BYTES (GSM_FRAME_SAMPLES * 2)   /* one frame of 16-bit output */
#define GSM_RING_WORDS 1024                       /* the stream's input ring: 4 KiB of 32-bit words */

/* The decoder's state, 0x190 bytes (port/ultra/gsm.h has the map of dp0: the residual history, the short-term
 * filter state, the deemphasis state and the last long-term lag). LARpp holds the decoded log-area ratios of
 * this frame and the last, j says which is this frame's. repeat and silence are the stream form's run counters. */
typedef struct GsmState { short dp0[180]; short LARpp[2][8]; short j; short repeat; short silence; unsigned char unknown18E[2]; } GsmState;

/* PC: the stream (decode.c's SpeechStream) has host pointers after the decoder state, so the
 * input ring and the bit position are where its native layout puts them */
typedef struct GsmStream { GsmState gsm; void *callback; unsigned char *input; void *output; unsigned int capacity; unsigned int input_ring[GSM_RING_WORDS]; unsigned short remaining_blocks; unsigned short header_low; unsigned int remaining_input; int remaining; OSMesgQueue queue; OSMesg message; int bit_position; } GsmStream;

/* The kernels (port/ultra/gsm.c) */
extern void gsm_rpe_decode(short, short, short *, short *);
extern void gsm_long_term_synthesis(GsmState *, short, short, short *);
extern void gsm_short_term_filter(GsmState *, short *, short *);
/* This file's functions, in the order they call each other */
extern void gsm_short_term_synthesis(GsmState *, short *, short *, short *);
extern void gsm_decode_parameters(GsmState *, short *, short *, short *, short *, short *, short *, short *);
extern void bzero(void *, int);

/* Short-term synthesis of one frame: decodes the coded log-area ratios LARcr[8] into this frame's half of
 * S->LARpp (GSM 06.10 4.2.8: add MIC, scale by 1/A after removing B, the constants per coefficient), runs the
 * short-term filter over the frame's residual wt into the 160 output samples s, then swaps the halves. */
void gsm_short_term_synthesis(GsmState *S, short *LARcr, short *wt, short *s)
{
    short *LARpp_j = S->LARpp[S->j];

#define STEP(i, B, MIC, INVA) \
    LARpp_j[i] = (((((LARcr[i] + MIC) << 10) - (B << 1)) * INVA + 16384) >> 15) << 1

    STEP(0, 0, -32, 13107);
    STEP(1, 0, -32, 13107);
    STEP(2, 2048, -16, 13107);
    STEP(3, -2560, -16, 13107);
    STEP(4, 94, -8, 19223);
    STEP(5, -1792, -8, 17476);
    STEP(6, -341, -4, 31454);
    STEP(7, -1144, -4, 29708);
    gsm_short_term_filter(S, wt, s);
    S->j ^= 1;
}

/* Decodes one frame from its unpacked parameters: for each of the four subframes, the RPE pulses (block
 * maximum xmaxcr, grid Mcr, 13 pulses xMcr) into the excitation, then long-term synthesis with lag Ncr and gain
 * bcr into the residual history; then short-term synthesis of the whole frame into s (160 samples). */
void gsm_decode_parameters(GsmState *S, short *LARcr, short *Ncr, short *bcr, short *Mcr, short *xmaxcr,
                   short *xMcr, short *s)
{
    int subframe;
    short erp[40];

    for (subframe = 0; subframe <= 3; subframe++, xmaxcr++, bcr++, Ncr++, Mcr++, xMcr += 13) {
        gsm_rpe_decode(*xmaxcr, *Mcr, xMcr, erp);
        gsm_long_term_synthesis(S, *Ncr, *bcr, erp);
    }
    gsm_short_term_synthesis(S, LARcr, S->dp0, s);
}

/* Decodes a standard packed 33-byte GSM frame c (a 0xD nibble, then the 260 bits MSB first) into 160 samples
 * at target. Returns 0. The game's streams use gsm_decode_stream_frame instead; port/ultra's test_gsm calls this
 * one to check the kernels frame by frame. */
int gsm_decode_frame(GsmState *s, unsigned char *c, short *target)
{
    short LARc[8], Nc[4], Mc[4], bc[4], xmaxc[4], xmc[13 * 4];
    int j;

    LARc[0] = ((c[0] & 0xF) << 2) | ((c[1] >> 6) & 0x3);
    LARc[1] = c[1] & 0x3F;
    LARc[2] = (c[2] >> 3) & 0x1F;
    LARc[3] = ((c[2] & 0x7) << 2) | ((c[3] >> 6) & 0x3);
    LARc[4] = (c[3] >> 2) & 0xF;
    LARc[5] = ((c[3] & 0x3) << 2) | ((c[4] >> 6) & 0x3);
    LARc[6] = (c[4] >> 3) & 0x7;
    LARc[7] = c[4] & 0x7;
    for (j = 0; j < 4; j++) {
        Nc[j] = (c[j * 7 + 5] >> 1) & 0x7F;
        bc[j] = ((c[j * 7 + 5] & 0x1) << 1) | ((c[j * 7 + 6] >> 7) & 0x1);
        Mc[j] = (c[j * 7 + 6] >> 5) & 0x3;
        xmaxc[j] = ((c[j * 7 + 6] & 0x1F) << 1) | ((c[j * 7 + 7] >> 7) & 0x1);
        xmc[j * 13 + 0] = (c[j * 7 + 7] >> 4) & 0x7;
        xmc[j * 13 + 1] = (c[j * 7 + 7] >> 1) & 0x7;
        xmc[j * 13 + 2] = ((c[j * 7 + 7] & 0x1) << 2) | ((c[j * 7 + 8] >> 6) & 0x3);
        xmc[j * 13 + 3] = (c[j * 7 + 8] >> 3) & 0x7;
        xmc[j * 13 + 4] = c[j * 7 + 8] & 0x7;
        xmc[j * 13 + 5] = (c[j * 7 + 9] >> 5) & 0x7;
        xmc[j * 13 + 6] = (c[j * 7 + 9] >> 2) & 0x7;
        xmc[j * 13 + 7] = ((c[j * 7 + 9] & 0x3) << 1) | ((c[j * 7 + 10] >> 7) & 0x1);
        xmc[j * 13 + 8] = (c[j * 7 + 10] >> 4) & 0x7;
        xmc[j * 13 + 9] = (c[j * 7 + 10] >> 1) & 0x7;
        xmc[j * 13 + 10] = ((c[j * 7 + 10] & 0x1) << 2) | ((c[j * 7 + 11] >> 6) & 0x3);
        xmc[j * 13 + 11] = (c[j * 7 + 11] >> 3) & 0x7;
        xmc[j * 13 + 12] = c[j * 7 + 11] & 0x7;
    }
    gsm_decode_parameters(s, LARc, Nc, bc, Mc, xmaxc, xmc, target);
    return 0;
}

/* Reads the next n bits (LSB first) of the input ring into dst, refilling `word` from the ring when the current
 * word runs out. Uses the locals word, left (bits left in word), pos (the bit position of word's start) and ring. */
#define GETBITS(dst, n) if (left > n) { dst = word & ((1 << n) - 1); word >>= n; left -= n; } else { dst = ((1 << left) - 1) & word; pos += 32; word = ring[(pos >> 5) & (GSM_RING_WORDS - 1)]; if (left != n) dst |= (word & ((1 << (n - left)) - 1)) << left; word >>= n - left; left += 32 - n; }

/* Decodes the stream's next frame into 160 samples at out, from its input ring at its bit position, and moves the
 * bit position past what it read. Between runs of frames the bank codes a run: a 1 bit, then a 4-bit count of
 * silent frames less one, or a 0 bit, then a 7-bit count of coded frames less one. A silent frame is zeroes and
 * reads no bits. The coded frame's fields are those of gsm_decode_frame, LSB first. Returns 0. */
int gsm_decode_stream_frame(GsmStream *stream, short *out)
{
    short LARc[8], Nc[4], Mc[4], bc[4], xmaxc[4], xmc[13 * 4];
    int j;
    int left;
    unsigned int word;
    int pos;
    GsmState *S;
    unsigned int *ring;

    S = &stream->gsm;
    ring = stream->input_ring;
    pos = stream->bit_position;
    word = ring[(pos >> 5) & (GSM_RING_WORDS - 1)] >> (pos & 31);
    left = 32 - (pos & 31);
    if (S->repeat == 0 && S->silence == 0) {
        GETBITS(j, 1);
        if (j) {
            GETBITS(S->silence, 4);
            S->silence++;
        } else {
            GETBITS(S->repeat, 7);
            S->repeat++;
        }
    }
    if (S->silence != 0) {
        bzero(out, GSM_FRAME_BYTES);
        S->silence--;
    } else {
        GETBITS(LARc[0], 6);
        GETBITS(LARc[1], 6);
        GETBITS(LARc[2], 5);
        GETBITS(LARc[3], 5);
        GETBITS(LARc[4], 4);
        GETBITS(LARc[5], 4);
        GETBITS(LARc[6], 3);
        GETBITS(LARc[7], 3);
        for (j = 0; j < 4; j++) {
            GETBITS(Nc[j], 7);
            GETBITS(bc[j], 2);
            GETBITS(Mc[j], 2);
            GETBITS(xmaxc[j], 6);
            GETBITS(xmc[j * 13 + 0], 3);
            GETBITS(xmc[j * 13 + 1], 3);
            GETBITS(xmc[j * 13 + 2], 3);
            GETBITS(xmc[j * 13 + 3], 3);
            GETBITS(xmc[j * 13 + 4], 3);
            GETBITS(xmc[j * 13 + 5], 3);
            GETBITS(xmc[j * 13 + 6], 3);
            GETBITS(xmc[j * 13 + 7], 3);
            GETBITS(xmc[j * 13 + 8], 3);
            GETBITS(xmc[j * 13 + 9], 3);
            GETBITS(xmc[j * 13 + 10], 3);
            GETBITS(xmc[j * 13 + 11], 3);
            GETBITS(xmc[j * 13 + 12], 3);
        }
        S->repeat--;
        gsm_decode_parameters(S, LARc, Nc, bc, Mc, xmaxc, xmc, out);
    }
    stream->bit_position = (pos & ~31) - left + 32;
    return 0;
}
