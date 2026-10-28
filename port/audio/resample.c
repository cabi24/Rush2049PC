/* port/audio/resample.c: polyphase windowed-sinc rate conversion (32 taps, 256 phases interpolated linearly,
 * Kaiser window beta 6, cutoff 0.45 of the lower rate). */
#include <math.h>
#include <string.h>

#include "resample.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double bessel_i0(double x)
{
    double sum = 1, term = 1;
    for (int k = 1; k < 32; k++) {
        term *= (x / (2 * k)) * (x / (2 * k));
        sum += term;
    }
    return sum;
}

void rs_init(PcResampler *rs, uint32_t in_rate, uint32_t out_rate)
{
    const double beta = 6.0;
    double fc;   /* cycles per input frame */

    memset(rs, 0, sizeof *rs);
    rs->in_rate = in_rate;
    rs->out_rate = out_rate;
    if (!in_rate || !out_rate || in_rate == out_rate)
        return;
    rs->step = ((uint64_t)in_rate << 32) / out_rate;
    fc = 0.45 * (out_rate < in_rate ? (double)out_rate / in_rate : 1.0);
    for (int p = 0; p <= RS_PHASES; p++) {
        double phase = (double)p / RS_PHASES, sum = 0;
        for (int k = 0; k < RS_TAPS; k++) {
            double d = k - (RS_TAPS / 2 - 1) - phase;   /* tap distance from the output point */
            double x = 2 * M_PI * fc * d;
            double sinc = fabs(x) < 1e-12 ? 1.0 : sin(x) / x;
            double u = d / (RS_TAPS / 2);
            double w = fabs(u) < 1 ? bessel_i0(beta * sqrt(1 - u * u)) / bessel_i0(beta) : 0;
            rs->coef[p][k] = (float)(sinc * w);
            sum += sinc * w;
        }
        for (int k = 0; k < RS_TAPS; k++)
            rs->coef[p][k] = (float)(rs->coef[p][k] / sum);
    }
    /* Start with silence as history: the first output sits on the first input frame. */
    rs->len = RS_TAPS / 2 - 1;
    rs->pos = (uint64_t)(RS_TAPS / 2 - 1) << 32;
}

size_t rs_max_out(const PcResampler *rs, size_t n)
{
    if (!rs->step)
        return n;
    return (size_t)((((uint64_t)n + RS_TAPS) << 32) / rs->step) + 2;
}

/* Either output: s16 (clamped, counted in rs->clipped) or int32 (as computed: the filter's overshoot of loud input
 * is kept, for an output that can carry it). */
static size_t process(PcResampler *rs, const int16_t *in, size_t n, int16_t *out16, int32_t *out32)
{
    size_t made = 0;
    if (!rs->step) {
        if (out16)
            memcpy(out16, in, n * 4);
        else
            for (size_t i = 0; i < 2 * n; i++)
                out32[i] = in[i];
        return n;
    }
    while (n) {
        size_t take = RS_TAPS + RS_CHUNK - rs->len;
        if (take > n)
            take = n;
        for (size_t i = 0; i < take; i++) {
            rs->hist[rs->len + i][0] = in[2 * i];
            rs->hist[rs->len + i][1] = in[2 * i + 1];
        }
        rs->len += take;
        in += 2 * take;
        n -= take;

        /* Outputs whose taps are all in hist. */
        for (;;) {
            size_t at = (size_t)(rs->pos >> 32);
            if (at + RS_TAPS / 2 >= rs->len)
                break;
            uint32_t frac = (uint32_t)rs->pos;
            double pf = (double)frac * RS_PHASES / 4294967296.0;
            int p = (int)pf;
            float t = (float)(pf - p);
            const float *c0 = rs->coef[p], *c1 = rs->coef[p + 1];
            const float(*x)[2] = (const float(*)[2])rs->hist[at - (RS_TAPS / 2 - 1)];
            float l = 0, r = 0;
            for (int k = 0; k < RS_TAPS; k++) {
                float c = c0[k] + (c1[k] - c0[k]) * t;
                l += c * x[k][0];
                r += c * x[k][1];
            }
            long li = lrintf(l), ri = lrintf(r);
            if (out16) {
                rs->clipped += (li > 32767 || li < -32768) + (ri > 32767 || ri < -32768);
                out16[2 * made] = (int16_t)(li > 32767 ? 32767 : li < -32768 ? -32768 : li);
                out16[2 * made + 1] = (int16_t)(ri > 32767 ? 32767 : ri < -32768 ? -32768 : ri);
            } else {
                out32[2 * made] = (int32_t)li;
                out32[2 * made + 1] = (int32_t)ri;
            }
            made++;
            rs->pos += rs->step;
        }

        /* Keep the history the next output needs. */
        size_t keep_from = (size_t)(rs->pos >> 32) - (RS_TAPS / 2 - 1);
        if (keep_from > rs->len)
            keep_from = rs->len;
        memmove(rs->hist, rs->hist[keep_from], (rs->len - keep_from) * sizeof rs->hist[0]);
        rs->len -= keep_from;
        rs->pos -= (uint64_t)keep_from << 32;
    }
    return made;
}

size_t rs_process(PcResampler *rs, const int16_t *in, size_t n, int16_t *out)
{
    return process(rs, in, n, out, NULL);
}

size_t rs_process32(PcResampler *rs, const int16_t *in, size_t n, int32_t *out)
{
    return process(rs, in, n, NULL, out);
}
