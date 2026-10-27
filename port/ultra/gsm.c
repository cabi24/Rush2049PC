/* port/ultra/gsm.c: the boot segment's hand-written GSM 06.10 decoder kernels in C (asm/us/boot_data/kernels.s,
 * docs/memory_map.md §1.1a). lib_26f60.c's frame decoder calls RPE decoding and long-term synthesis once per
 * subframe, then short-term synthesis (with deemphasis, upscaling and truncation) once per frame.
 *
 * Translated from the assembly, so the arithmetic is the assembly's, not quite the spec's. Products are the low
 * 32 bits of a 32 x 32 multiply, rounded as (p + 0x4000) >> 15. Sums wrap instead of saturating: drp wraps to 16 bits
 * when it is stored, and the filter state v[] and msr stay 32-bit within a frame (v[] across frames too). Only
 * the output's upscaling saturates. When the spec's decoder saturates nothing, the output is the spec's bit for
 * bit (port/ultra/tests/test_gsm.c). Written from the assembly and the GSM 06.10 specification; no libgsm code. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "gsm.h"

_Static_assert(offsetof(GsmState_26F60, LARpp) == 0x168 && offsetof(GsmState_26F60, j) == 0x188 &&
               sizeof(GsmState_26F60) == 0x190, "GsmState_26F60 must keep lib_26f60.c's N64 layout");

/* MIPS addu/subu and mult+mflo: 32-bit, wrapping. */
static inline int32_t add32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
static inline int32_t sub32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
/* mult, mflo, addiu 0x4000, sra 15: the spec's mult_r without its -32768 * -32768 case. */
static inline int32_t mult_r(int32_t a, int32_t b)
{
    return add32((int32_t)((uint32_t)a * (uint32_t)b), 0x4000) >> 15;
}

/* ---------------------------------------------------------------- RPE decoding (asm 0x800277D0) */

/* exp and mant of xmaxc 0..7 after the spec's normalization (4.2.15): the asm's inline table D_800278BC holds
 * them as (exp << 4) | mant, the bytes C7 D7 E3 E7 F1 F3 F5 F7. */
static const struct { int8_t exp, mant; } small_xmaxc[8] = {
    {-4, 7}, {-3, 7}, {-2, 3}, {-2, 7}, {-1, 1}, {-1, 3}, {-1, 5}, {-1, 7},
};

/* APCM inverse quantization (4.2.16) of the 13 RPE samples, in place in xMcr as the asm does, then their
 * placement on grid Mcr of erp[0..39] (4.2.17). xmaxcr is a 6-bit field, so never negative (the asm would index
 * before its table). */
void func_800277D0(short xmaxcr, short Mcr, short *xMcr, short *erp)
{
    int32_t exp, mant;
    if (xmaxcr > 7) {
        exp = (xmaxcr - 8) >> 3;
        mant = xmaxcr & 7;
    } else {
        exp = small_xmaxc[xmaxcr & 7].exp;
        mant = small_xmaxc[xmaxcr & 7].mant;
    }
    int32_t fac = (mant << 11) + 0x47FF;   /* FAC[mant] = 18431 + 2048 * mant */
    int32_t shift = 6 - exp;
    /* sllv takes the count mod 32: for exp 6 this is 1 << 31, which the 16-bit store drops (the spec has 0). */
    int32_t round = (int32_t)(1u << ((shift - 1) & 31));
    for (int i = 0; i < 13; i++) {
        int32_t t = sub32((int32_t)((uint32_t)xMcr[i] << 13), 0x7000);   /* (2 * xMc - 7) << 12 */
        t = add32(mult_r(t, fac), round);
        xMcr[i] = (short)(t >> (shift & 31));
    }
    for (int k = 0; k < 40; k++)
        erp[k] = 0;
    for (int i = 0; i < 13; i++)
        erp[Mcr + 3 * i] = xMcr[i];
}

/* ---------------------------------------------------------------- long-term synthesis (asm 0x800278C4) */

/* QLB, the LTP gain levels: the asm's inline table D_80027978 (0x0CCD 0x2CCD 0x5333 0x7FFF). */
static const uint16_t qlb[4] = {3277, 11469, 21299, 32767};

/* 4.3.2. The history moves first (the spec moves it after), so dp0[120..159] receives drp[0..39] and the four
 * subframes of a frame end up in dp0[0..159] in order: the short-term filter's input. bcr is a 2-bit field. */
void func_800278C4(GsmState_26F60 *S, short Ncr, short bcr, short *erp)
{
    short *dp = S->dp0, *drp = S->dp0 + 120;
    int32_t Nr = (Ncr >= 40 && Ncr <= 120) ? Ncr : dp[GSM_NRP];
    dp[GSM_NRP] = (short)Nr;
    int32_t brp = qlb[bcr & 3];
    memmove(dp, dp + 40, 120 * sizeof *dp);   /* the asm copies upward a halfword at a time */
    for (int k = 0; k < 40; k++)
        drp[k] = (short)add32(mult_r(drp[k - Nr], brp), erp[k]);
}

/* ---------------------------------------------------------------- short-term synthesis (asm 0x80027B44) */

/* LARp for each part of the frame (4.2.9.1) from LARpp(j-1) and LARpp(j): the asm's helpers at 0x80027D3C,
 * 0x80027D54 and 0x80027D64, and LARpp(j) itself for k 40..159. */
static int32_t larp(int part, int32_t cur, int32_t prev)
{
    switch (part) {
    case 0: return (cur >> 2) + (prev >> 1) + (prev >> 2);   /* k 0..12 */
    case 1: return (cur >> 1) + (prev >> 1);                 /* k 13..26 */
    case 2: return (prev >> 2) + (cur >> 1) + (cur >> 2);    /* k 27..39 */
    default: return cur;
    }
}

/* LARp to rp (4.2.9.2), the asm's 0x80027D7C. The clamp is the spec's saturating add. */
static int32_t larp_to_rp(int32_t x)
{
    int32_t a = x < 0 ? sub32(0, x) : x;
    int32_t r = a < 11059 ? a * 2 : a < 20070 ? a + 11059 : (a >> 2) + 26112;
    if (r > 0x7FFF)
        r = 0x7FFF;
    return (short)(x < 0 ? -r : r);
}

/* The lattice filter (4.3.4) over wt[0..159] into s[0..159], with deemphasis (4.3.5), upscaling (4.3.6) and
 * truncation (4.3.7) per sample. rp is recomputed for each of the four parts (the asm's 0x80027C98 packs it in
 * t6..t9; 0x80027980 filters). v[8] is never read, so it isn't computed. */
void func_80027B44(GsmState_26F60 *S, short *wt, short *s)
{
    static const struct { int start, n; } parts[4] = {{0, 13}, {13, 14}, {27, 13}, {40, 120}};
    const short *cur = S->LARpp[S->j != 0], *prev = S->LARpp[S->j == 0];
    int32_t v[8];
    memcpy(v, (unsigned char *)S + GSM_V_OFFSET, sizeof v);
    int32_t msr = S->dp0[GSM_MSR];

    for (int p = 0; p < 4; p++) {
        int32_t rp[8];
        for (int i = 0; i < 8; i++)
            rp[i] = larp_to_rp(larp(p, cur[i], prev[i]));
        for (int k = parts[p].start; k < parts[p].start + parts[p].n; k++) {
            int32_t sri = wt[k];
            for (int i = 7; i >= 0; i--) {
                sri = sub32(sri, mult_r(rp[i], v[i]));
                if (i < 7)
                    v[i + 1] = add32(v[i], mult_r(rp[i], sri));
            }
            v[0] = sri;
            msr = add32(sri, mult_r(msr, 28180));
            int32_t d = (int32_t)((uint32_t)msr << 1);
            d = d > 32767 ? 32767 : d < -32768 ? -32768 : d;
            s[k] = (short)(d & 0xFFF8);
        }
    }
    memcpy((unsigned char *)S + GSM_V_OFFSET, v, sizeof v);
    S->dp0[GSM_MSR] = (short)msr;   /* sh: 16 bits between frames */
}
