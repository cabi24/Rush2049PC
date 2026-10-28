/* port/upscale/upscale.c: a stand-in 2x upscaler (classical) until the learned one lands; upscale.h is the
 * interface. Written for this port.
 *
 * Catmull-Rom bicubic at the two sub-texel phases (a 2x output texel's centre is a quarter texel from its source
 * texel's), separable; then each output value is held within the range of the 2 x 2 source texels nearest to it,
 * which removes bicubic's ringing (light halos along dark edges); then a light unsharp mask in the output, held
 * the same way. Every channel alike, alpha included. Edges wrap or clamp as the flags say. */
#include <stdlib.h>
#include <string.h>

#include "upscale.h"

#define UPSCALE_MAX_DIM 8192
#define SHARPEN 0.35f

/* Taps for the left output texel (centre at i - 1/4): source texels i-2 .. i+1. The right one (i + 1/4) takes
 * i-1 .. i+2 with the taps reversed. Catmull-Rom at t = 3/4. */
static const float k_taps[4] = {-0.0234375f, 0.2265625f, 0.8671875f, -0.0703125f};

static int addr(int i, int n, int wrap) {
    if (wrap) {
        i %= n;
        return i < 0 ? i + n : i;
    }
    return i < 0 ? 0 : i >= n ? n - 1 : i;
}

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

int pc_upscale_rgba2x(const uint8_t *in, int w, int h, int wrap_s, int wrap_t, uint8_t *out) {
    int W = 2 * w, H = 2 * h, x, y, c, k;
    float *hp, *res;
    if (in == NULL || out == NULL || w <= 0 || h <= 0 || w > UPSCALE_MAX_DIM || h > UPSCALE_MAX_DIM)
        return -1;
    hp = (float *)malloc(sizeof(float) * 4 * (size_t)W * h);    /* horizontal pass: W x h */
    res = (float *)malloc(sizeof(float) * 4 * (size_t)W * H);   /* both passes, held: W x H */
    if (hp == NULL || res == NULL) {
        free(hp);
        free(res);
        return -1;
    }
    for (y = 0; y < h; y++)
        for (x = 0; x < W; x++) {
            int sx = x >> 1, right = x & 1;
            float *o = hp + ((size_t)y * W + x) * 4;
            o[0] = o[1] = o[2] = o[3] = 0.0f;
            for (k = 0; k < 4; k++) {
                int xi = addr(right ? sx - 1 + k : sx - 2 + k, w, wrap_s);
                float wk = right ? k_taps[3 - k] : k_taps[k];
                const uint8_t *p = in + ((size_t)y * w + xi) * 4;
                for (c = 0; c < 4; c++)
                    o[c] += wk * p[c];
            }
        }
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            int sy = y >> 1, below = y & 1, sx = x >> 1;
            int nx = addr(sx + ((x & 1) ? 1 : -1), w, wrap_s), ny = addr(sy + (below ? 1 : -1), h, wrap_t);
            const uint8_t *q[4];
            float *o = res + ((size_t)y * W + x) * 4;
            q[0] = in + ((size_t)sy * w + sx) * 4;
            q[1] = in + ((size_t)sy * w + nx) * 4;
            q[2] = in + ((size_t)ny * w + sx) * 4;
            q[3] = in + ((size_t)ny * w + nx) * 4;
            for (c = 0; c < 4; c++) {
                float v = 0.0f, lo = 255.0f, hi = 0.0f;
                for (k = 0; k < 4; k++) {
                    int yi = addr(below ? sy - 1 + k : sy - 2 + k, h, wrap_t);
                    v += (below ? k_taps[3 - k] : k_taps[k]) * hp[((size_t)yi * W + x) * 4 + c];
                }
                for (k = 0; k < 4; k++) {
                    if (q[k][c] < lo) lo = q[k][c];
                    if (q[k][c] > hi) hi = q[k][c];
                }
                o[c] = clampf(v, lo, hi);
            }
        }
    /* Unsharp mask: v + SHARPEN (v - blur), the blur a 3 x 3 binomial in the output; held to the same range. */
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            int sy = y >> 1, sx = x >> 1;
            int nx = addr(sx + ((x & 1) ? 1 : -1), w, wrap_s), ny = addr(sy + ((y & 1) ? 1 : -1), h, wrap_t);
            int xm = addr(x - 1, W, wrap_s), xp = addr(x + 1, W, wrap_s);
            int ym = addr(y - 1, H, wrap_t), yp = addr(y + 1, H, wrap_t);
            const float *r = res;
            uint8_t *o = out + ((size_t)y * W + x) * 4;
            for (c = 0; c < 4; c++) {
#define R(xx, yy) r[((size_t)(yy) * W + (xx)) * 4 + c]
                float v = R(x, y);
                float blur = (R(xm, ym) + R(xp, ym) + R(xm, yp) + R(xp, yp) +
                              2.0f * (R(x, ym) + R(x, yp) + R(xm, y) + R(xp, y)) + 4.0f * v) / 16.0f;
#undef R
                float lo = 255.0f, hi = 0.0f, s;
                const uint8_t *q[4];
                q[0] = in + ((size_t)sy * w + sx) * 4;
                q[1] = in + ((size_t)sy * w + nx) * 4;
                q[2] = in + ((size_t)ny * w + sx) * 4;
                q[3] = in + ((size_t)ny * w + nx) * 4;
                for (k = 0; k < 4; k++) {
                    if (q[k][c] < lo) lo = q[k][c];
                    if (q[k][c] > hi) hi = q[k][c];
                }
                s = clampf(v + SHARPEN * (v - blur), lo, hi);
                o[c] = (uint8_t)(s + 0.5f);
            }
        }
    free(hp);
    free(res);
    return 0;
}
