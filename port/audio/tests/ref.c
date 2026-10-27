/* port/audio/tests/ref.c: references written from the record format (musyx_abi.h), independent of the
 * interpreter's code, plus a small ADPCM encoder, the game's voice builder and tone measurements. */
#include <stdlib.h>
#include <string.h>

#include "test.h"

static int16_t clamp16(long long v)
{
    return v > 32767 ? 32767 : v < -32768 ? -32768 : (int16_t)v;
}

/* Sample n of a frame (2..31): its nibble, signed and scaled by the header's shift. */
static int16_t residual(const uint8_t fr[20], int n)
{
    int shift = fr[4] & 15;
    int byte = fr[5 + (n - 2) / 2];
    int nibble = (n % 2 == 0) ? byte >> 4 : byte & 15;
    int value = nibble >= 8 ? nibble - 16 : nibble;   /* -8..7 */
    /* value * 2^12, as s16, arithmetic shift right */
    int scaled = value * 4096;
    return (int16_t)(scaled >= 0 ? scaled >> shift : -((-scaled + (1 << shift) - 1) >> shift));
}

void ref_adpcm_frame(const uint8_t fr[20], const int16_t book[128], int16_t out[32])
{
    const int16_t *entry = book + ((fr[4] >> 4) & 7) * 16;
    out[0] = (int16_t)(fr[0] * 256 + fr[1]);
    out[1] = (int16_t)(fr[2] * 256 + fr[3]);
    for (int n = 2; n < 32; n++) {
        int start = n < 8 ? 2 : n / 8 * 8;   /* groups: 2..7, 8..15, 16..23, 24..31 */
        int i = n - start;
        long long acc = (long long)residual(fr, n) * 2048 + (long long)entry[i] * out[start - 2] +
                        (long long)entry[8 + i] * out[start - 1];
        for (int m = start; m < n; m++)
            acc += (long long)entry[8 + (n - 1 - m)] * residual(fr, m);
        long long q = acc >= 0 ? acc / 2048 : -((-acc + 2047) / 2048);   /* floor */
        out[n] = clamp16(q);
    }
}

void ref_pack_block(const uint8_t a[20], const uint8_t b[20], uint8_t blk[40])
{
    memcpy(blk, a, 4);
    memcpy(blk + 4, b, 4);
    memcpy(blk + 8, a + 4, 16);
    memcpy(blk + 24, b + 4, 16);
}

void ref_make_book(int16_t entry[16], double a1, double a2)
{
    /* y[i] = c0[i] * y[-2] + c1[i] * y[-1] for the zero-input response */
    double c0[10] = {1, 0}, c1[10] = {0, 1};
    for (int i = 2; i < 10; i++) {
        c0[i] = a1 * c0[i - 1] + a2 * c0[i - 2];
        c1[i] = a1 * c1[i - 1] + a2 * c1[i - 2];
    }
    for (int i = 0; i < 8; i++) {
        entry[i] = clamp16(llround(c0[i + 2] * 2048));
        entry[8 + i] = clamp16(llround(c1[i + 2] * 2048));
    }
}

/* Closed-loop encode of one frame with a given shift, choosing each nibble against the decoder's output so
 * far; returns the squared error. */
static double encode_frame(const int16_t *x, const int16_t book[128], int idx, int shift, uint8_t fr[20])
{
    const int16_t *entry = book + idx * 16;
    long long out[32], res[32];
    double err = 0;
    memset(fr, 0, 20);
    fr[0] = (uint8_t)((uint16_t)x[0] >> 8);
    fr[1] = (uint8_t)x[0];
    fr[2] = (uint8_t)((uint16_t)x[1] >> 8);
    fr[3] = (uint8_t)x[1];
    fr[4] = (uint8_t)(idx << 4 | shift);
    out[0] = x[0];
    out[1] = x[1];
    for (int n = 2; n < 32; n++) {
        int start = n < 8 ? 2 : n / 8 * 8, i = n - start, best = 0;
        long long base = (long long)entry[i] * out[start - 2] + (long long)entry[8 + i] * out[start - 1];
        double best_e = 1e30;
        for (int m = start; m < n; m++)
            base += (long long)entry[8 + (n - 1 - m)] * res[m];
        for (int v = -8; v <= 7; v++) {
            int scaled = v * 4096;
            int r = scaled >= 0 ? scaled >> shift : -((-scaled + (1 << shift) - 1) >> shift);
            long long acc = (long long)r * 2048 + base;
            long long o = clamp16(acc >= 0 ? acc / 2048 : -((-acc + 2047) / 2048));
            double e = (double)(o - x[n]) * (o - x[n]);
            if (e < best_e) {
                best_e = e;
                best = v;
                res[n] = r;
                out[n] = o;
            }
        }
        err += best_e;
        uint8_t *byte = &fr[5 + (n - 2) / 2];
        if (n % 2 == 0)
            *byte = (uint8_t)((*byte & 0x0F) | (best & 15) << 4);
        else
            *byte = (uint8_t)((*byte & 0xF0) | (best & 15));
    }
    return err;
}

void ref_adpcm_encode(const int16_t *pcm, unsigned frames, const int16_t book[128], int idx, uint8_t *out)
{
    uint8_t fr[2][20];
    for (unsigned f = 0; f < frames; f++) {
        double best = 1e300;
        uint8_t trial[20];
        for (int shift = 0; shift <= 12; shift++) {
            double e = encode_frame(pcm + f * 32, book, idx, shift, trial);
            if (e < best) {
                best = e;
                memcpy(fr[f & 1], trial, 20);
            }
        }
        if (f & 1)
            ref_pack_block(fr[0], fr[1], out + (f / 2) * 40);
    }
}

/* ---------------------------------------------------------------- the game's builder */

void tb_voice(TbVoice *tv, PcMusyxVoice *cmd, const int16_t target[4])
{
    unsigned frames = ((unsigned)(tv->rate * 192 + 0xFFF) >> 12) + 6;
    unsigned position = (unsigned)floor(tv->position);
    unsigned end = tv->loop_start + tv->loop_length;
    unsigned address[2] = {0, 0}, length[2], padded[2] = {0, 0}, excess;
    int offset, mode;
    unsigned mainRem, loopRem = 0;

    memset(cmd, 0, sizeof *cmd);
    if (tv->loop_length != 0 && end >= position) {
        if (end >= position + frames) {
            offset = 0, address[0] = position, length[0] = frames - 6, length[1] = 0, mode = -1;
        } else if (tv->loop_start >= position) {
            address[0] = position, length[0] = tv->loop_start - position + tv->loop_length, length[1] = 0;
            offset = 0, mode = (int)(tv->loop_start - position);
        } else if (tv->loop_length < (excess = frames - end + position) || excess >= position - tv->loop_start) {
            offset = (int)(position - tv->loop_start), address[0] = tv->loop_start, length[0] = tv->loop_length;
            length[1] = 0, mode = 0;
        } else {
            offset = 0, address[0] = position, length[0] = end - position, address[1] = tv->loop_start;
            length[1] = excess - 6, mode = 0;
        }
    } else if (position + frames >= tv->length) {
        address[0] = position, offset = 0, length[0] = tv->length - position + 0x20, length[1] = 0, mode = -2;
    } else {
        address[0] = position, offset = 0, length[0] = frames - 6, length[1] = 0, mode = -1;
    }
    padded[0] = length[0] + 6;
    if (length[1])
        padded[1] = length[1] + 6;

    if (tv->format == 3) {
        if (length[1]) {
            loopRem = address[1] & 0x3F;
            cmd->src[1].ptr1 = (address[1] >> 6) * 40 + tv->data + 0x100;
            cmd->src[1].size1 = (int16_t)(((((padded[1] + (loopRem & 0x1F) + 0x1F) >> 5) + loopRem / 32 + 1) >> 1) * 40);
            cmd->skip[1] = (uint8_t)loopRem;
            loopRem &= 0x1F;
            cmd->frames[1] = (uint8_t)((padded[1] + loopRem + 0x1F) >> 5);
        }
        mainRem = address[0] & 0x3F;
        cmd->src[0].ptr1 = (address[0] >> 6) * 40 + tv->data + 0x100;
        cmd->src[0].size1 = (int16_t)(((((padded[0] + (mainRem & 0x1F) + 0x1F) >> 5) + mainRem / 32 + 1) >> 1) * 40);
        cmd->u.book = tv->data;
        cmd->skip[0] = (uint8_t)mainRem;
        mainRem &= 0x1F;
        cmd->frames[0] = (uint8_t)((padded[0] + mainRem + 0x1F) >> 5);
    } else {
        if (length[1]) {
            loopRem = address[1] & 3;
            cmd->src[1].ptr1 = (address[1] & 0xFFFC) * 2 + tv->data;
            cmd->src[1].size1 = (int16_t)(((padded[1] + loopRem + 3) & 0xFFFC) * 2);
            cmd->u.pcm.count[1] = (int16_t)padded[1];
            cmd->skip[1] = (uint8_t)loopRem;
        }
        mainRem = address[0] & 3;
        cmd->src[0].ptr1 = (address[0] & 0xFFFC) * 2 + tv->data;
        cmd->src[0].size1 = (int16_t)(((padded[0] + mainRem + 3) & 0xFFFC) * 2);
        cmd->u.pcm.count[0] = (int16_t)padded[0];
        cmd->frames[0] = 0;
        cmd->skip[0] = (uint8_t)mainRem;
    }
    cmd->start = (uint16_t)offset;
    if (mode == -1) {
        cmd->end = 0x4000;
    } else if (mode == -2) {
        cmd->end = (uint16_t)(length[0] + mainRem);
        cmd->restart = (uint16_t)(length[0] + mainRem - 0x20);
    } else {
        cmd->end = (uint16_t)(length[0] + mainRem);
        cmd->restart = (uint16_t)(cmd->src[1].ptr1 ? (loopRem + mode) | 0x8000 : mainRem + mode);
    }
    for (int k = 0; k < 4; k++) {
        if (tv->first) {
            cmd->env[k] = (int32_t)((uint32_t)(uint16_t)target[k] << 16);
            cmd->env_step[k] = 0;
        } else {
            cmd->env[k] = (int32_t)((uint32_t)(uint16_t)tv->level[k] << 16);
            cmd->env_step[k] = (int32_t)(((int64_t)target[k] * 65536 - (int64_t)tv->level[k] * 65536) / 192);
        }
        tv->level[k] = target[k];
    }
    tv->first = 0;
    cmd->pitch = tv->rate;
    cmd->pitch_frac = (uint16_t)((tv->position - floor(tv->position)) * 65536.0);
    cmd->unused4c = (uint16_t)(padded[0] * (tv->rate * 16u) + cmd->pitch_frac);
    cmd->output = NULL;

    /* func_80011D74 */
    double inc = (tv->rate * 192) * 0.000244140625;
    if (tv->loop_length != 0 && floor(tv->position) <= tv->loop_start + tv->loop_length) {
        double loop_end = tv->loop_start + tv->loop_length;
        if (tv->position + inc < loop_end) {
            tv->position += inc;
        } else {
            inc -= loop_end - tv->position;
            inc -= floor(inc / tv->loop_length) * tv->loop_length;
            tv->position = tv->loop_start + inc;
        }
    } else if (tv->position + inc >= tv->length) {
        tv->position = tv->length - 1;
    } else {
        tv->position += inc;
    }
}

/* ---------------------------------------------------------------- measurements */

double tone_freq(const int16_t *x, size_t n, int stride, double rate)
{
    double first = -1, last = -1;
    long crossings = 0;
    for (size_t i = 1; i < n; i++) {
        double a = x[(i - 1) * stride], b = x[i * stride];
        if (a < 0 && b >= 0) {
            double t = (i - 1) + a / (a - b);
            if (first < 0)
                first = t;
            last = t;
            crossings++;
        }
    }
    if (crossings < 2)
        return 0;
    return (crossings - 1) * rate / (last - first);
}

double tone_fit(const int16_t *x, size_t n, int stride, double rate, double freq, double *resid)
{
    return tone_fit2(x, n, stride, rate, freq, resid, NULL);
}

double tone_fit2(const int16_t *x, size_t n, int stride, double rate, double freq, double *resid, double *worst)
{
    /* least squares on [sin, cos, 1] */
    double w = 2 * M_PI * freq / rate;
    double m[3][3] = {{0}}, v[3] = {0};
    for (size_t i = 0; i < n; i++) {
        double b[3] = {sin(w * i), cos(w * i), 1};
        for (int r = 0; r < 3; r++) {
            v[r] += b[r] * x[i * stride];
            for (int c = 0; c < 3; c++)
                m[r][c] += b[r] * b[c];
        }
    }
    /* solve 3x3 */
    for (int c = 0; c < 3; c++) {
        for (int r = c + 1; r < 3; r++) {
            double f = m[r][c] / m[c][c];
            for (int k = c; k < 3; k++)
                m[r][k] -= f * m[c][k];
            v[r] -= f * v[c];
        }
    }
    double s[3];
    for (int r = 2; r >= 0; r--) {
        double acc = v[r];
        for (int k = r + 1; k < 3; k++)
            acc -= m[r][k] * s[k];
        s[r] = acc / m[r][r];
    }
    if (resid || worst) {
        double e = 0, m = 0;
        for (size_t i = 0; i < n; i++) {
            double d = x[i * stride] - (s[0] * sin(w * i) + s[1] * cos(w * i) + s[2]);
            e += d * d;
            m = fabs(d) > m ? fabs(d) : m;
        }
        if (resid)
            *resid = sqrt(e / n);
        if (worst)
            *worst = m;
    }
    return sqrt(s[0] * s[0] + s[1] * s[1]);
}
