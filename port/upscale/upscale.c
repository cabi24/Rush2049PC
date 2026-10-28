/* port/upscale/upscale.c: 2x texture upscaling for the "upscaled textures" setting (docs/port/UPSCALE.md).
 *
 * Colour: a small convolutional network trained on CC0 photo textures degraded to look like N64 textures
 * (tools/upscale), run on the texture's own grid, predicting the 2x2 texels around each texel as a correction to a
 * Catmull-Rom 2x. Alpha: separately, without the network. A binary alpha (cut-outs) stays binary: EPX on the mask,
 * which keeps thin lines and single texels and rounds diagonal staircases. Any other alpha is Catmull-Rom limited
 * to the range of the four nearest texels (no ringing, no halos). Before the colour is
 * scaled, the colour under fully transparent texels is filled in from their visible neighbours, so it doesn't bleed
 * into what is visible; the new texels that come out fully transparent take the original colours again (bilinear),
 * since the renderer may still blend them in at an edge. Grey textures stay grey, and intensity textures (I4/I8:
 * alpha = intensity) keep alpha = intensity.
 *
 * Edges: every step reads past an edge by wrapping (wrap_s / wrap_t) or by repeating the edge texel, so a repeating
 * texture upscales to a repeating texture with no seam.
 *
 * Plain C, single precision, no FMA contraction (CMake builds this file with -ffp-contract=off): the result is the
 * same bytes on every machine and compiler, which tests/test_upscale.c checks. Reentrant: no state between calls.
 */
#define _POSIX_C_SOURCE 200809L   /* sysconf, pthreads under a strict -std */
#include "upscale.h"
#include "upscale_internal.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define UPS_ALIGN __attribute__((aligned(16)))
#include "upscale_weights.h"   /* the trained weights (tools/upscale/export_c.py) */

#if !defined(__GNUC__)
#error "upscale.c uses GCC's vector extensions (GCC or Clang)"
#endif
#define UPS_INLINE static inline __attribute__((always_inline))
/* Unrolled loops over the texels and channels of a group keep the sums in registers. */
#define UPS_PRAGMA(x) _Pragma(#x)
#if defined(__clang__)
#define UPS_UNROLL UPS_PRAGMA(clang loop unroll(full))
#else
#define UPS_UNROLL UPS_PRAGMA(GCC unroll 16)
#endif

#define CH UPS_CHANNELS
#if UPS_BLOCKS < 1 || UPS_CHANNELS > 64
#error "upscale.c expects at least one body block and at most 64 channels"
#endif
#define TAIL_OUT 12   /* 3 colours x the 2x2 new texels of each texel */

const char pc_upscale_model_name[] = UPS_MODEL_NAME;
const int pc_upscale_model_params = UPS_MODEL_PARAMS;

static int edge(int i, int n, int wrap) {
    if (wrap) {
        i %= n;
        return i < 0 ? i + n : i;
    }
    return i < 0 ? 0 : i >= n ? n - 1 : i;
}

static uint8_t to8(float v) {
    v = v * 255.0f + 0.5f;
    return v <= 0.0f ? 0 : v >= 255.0f ? 255 : (uint8_t)v;
}

/* ---------------------------------------------------------------- separable 2x (Catmull-Rom, bilinear)
 * New texel 2i sits at old texel i - 1/4, 2i+1 at i + 1/4 (both axes). Phase 0 reads texels i-2..i+1, phase 1
 * i-1..i+2. The Catmull-Rom weights are exact binary fractions (n/128). */
static const float k_cubic[2][4] = { { -3 / 128.0f, 29 / 128.0f, 111 / 128.0f, -9 / 128.0f },
                                     { -9 / 128.0f, 111 / 128.0f, 29 / 128.0f, -3 / 128.0f } };
static const float k_bilinear[2][4] = { { 0.0f, 0.25f, 0.75f, 0.0f }, { 0.0f, 0.75f, 0.25f, 0.0f } };

/* src: h x w x c; dst: 2h x 2w x c. tmp: h x 2w x c. map: w + 4 + h + 4 ints. */
static void scale2x(const float *src, int w, int h, int c, int ws, int wt, const float k[2][4], float *dst,
                    float *tmp, int *map) {
    int *mx = map + 2, *my = map + w + 4 + 2;   /* mx[-2..w+1], my[-2..h+1] */
    for (int i = -2; i < w + 2; i++)
        mx[i] = edge(i, w, ws);
    for (int i = -2; i < h + 2; i++)
        my[i] = edge(i, h, wt);
    for (int y = 0; y < h; y++) {
        const float *row = src + (size_t)y * w * c;
        float *o = tmp + (size_t)y * 2 * w * c;
        for (int x = 0; x < w; x++)
            for (int p = 0; p < 2; p++)
                for (int ch = 0; ch < c; ch++) {
                    float s = 0.0f;
                    for (int t = 0; t < 4; t++)
                        s += k[p][t] * row[mx[x + p - 2 + t] * c + ch];
                    o[(2 * x + p) * c + ch] = s;
                }
    }
    const int rw = 2 * w * c;
    for (int y = 0; y < h; y++)
        for (int p = 0; p < 2; p++) {
            float *o = dst + (size_t)(2 * y + p) * rw;
            const float *r0 = tmp + (size_t)my[y + p - 2] * rw, *r1 = tmp + (size_t)my[y + p - 1] * rw;
            const float *r2 = tmp + (size_t)my[y + p] * rw, *r3 = tmp + (size_t)my[y + p + 1] * rw;
            for (int i = 0; i < rw; i++)
                o[i] = k[p][0] * r0[i] + k[p][1] * r1[i] + k[p][2] * r2[i] + k[p][3] * r3[i];
        }
}

/* ---------------------------------------------------------------- the network
 * Feature maps are (h + 2) x (w + 2) x channels, the texture in the middle and a one-texel border around it,
 * filled by wrapping or repeating, as tools/upscale/model.py's pad2d: first each row's ends (fill_ends), then the
 * rows above and below (fill_row), corners included. */
static void fill_ends(float *m, int w, int c, int ws, int y0, int y1) {
    const size_t stride = (size_t)(w + 2) * c, px = (size_t)c * sizeof(float);
    for (int y = y0 + 1; y <= y1; y++) {
        float *row = m + y * stride;
        memcpy(row, row + (ws ? w : 1) * c, px);
        memcpy(row + (w + 1) * c, row + (ws ? 1 : w) * c, px);
    }
}

static void fill_row(float *m, int w, int h, int c, int wt, int bottom) {
    const size_t stride = (size_t)(w + 2) * c;
    if (bottom)
        memcpy(m + (h + 1) * stride, m + (wt ? 1 : h) * stride, stride * sizeof(float));
    else
        memcpy(m, m + (wt ? h : 1) * stride, stride * sizeof(float));
}

/* A 3x3 convolution, cin -> cout, of a padded map into the middle of another (rows y0..y1-1), with PReLU when
 * slope is given and then + add (a map like out) when add is given. Four output channels at a time (GCC's generic
 * vectors: SSE2 on x86-64, NEON on aarch64), UPS_PB texels at a time sharing each weight load. Every output adds
 * its 9 x cin products to the bias one at a time in one fixed order, however the texels are grouped or the rows
 * shared out, so neither changes any result. cin and cout are constants at every call. */
typedef float v4f __attribute__((vector_size(16)));
#ifndef UPS_PB
#if defined(__aarch64__)
#define UPS_PB 4   /* 32 vector registers */
#else
#define UPS_PB 2   /* 16 on x86-64 */
#endif
#endif

UPS_INLINE void conv_px(const float *px, size_t sin, int cin, int cout, const v4f *wv, const v4f *bv,
                        const float *slope, const float *add, float *d, const int np) {
    const int nv = cout / 4;
    v4f acc[UPS_PB][64 / 4];
    UPS_UNROLL for (int p = 0; p < np; p++)
        UPS_UNROLL for (int j = 0; j < nv; j++)
            acc[p][j] = bv[j];
    for (int ky = 0; ky < 3; ky++) {
        const float *row = px + ky * sin;   /* the window's row: 3 texels x cin, contiguous */
        const v4f *wk = wv + ky * 3 * cin * nv;
        for (int k = 0; k < 3 * cin; k++) {
            const v4f *wc = wk + k * nv;
            UPS_UNROLL for (int p = 0; p < np; p++) {
                const float v = row[p * cin + k];
                const v4f s = { v, v, v, v };
                UPS_UNROLL for (int j = 0; j < nv; j++)
                    acc[p][j] += s * wc[j];
            }
        }
    }
    UPS_UNROLL for (int p = 0; p < np; p++)
        UPS_UNROLL for (int j = 0; j < nv; j++)
            UPS_UNROLL for (int l = 0; l < 4; l++) {
                float v = acc[p][j][l];
                if (slope && v <= 0.0f)
                    v *= slope[j * 4 + l];
                if (add)
                    v += add[p * cout + j * 4 + l];
                d[p * cout + j * 4 + l] = v;
            }
}

UPS_INLINE void conv3x3(const float *in, int w, int y0, int y1, int cin, int cout, const float *wt,
                        const float *bias, const float *slope, const float *add, float *out) {
    const size_t sin = (size_t)(w + 2) * cin, sout = (size_t)(w + 2) * cout;
    const v4f *wv = (const v4f *)wt, *bv = (const v4f *)bias;
    for (int y = y0; y < y1; y++) {
        const float *row = in + y * sin;
        const size_t o = (y + 1) * sout + cout;
        int x = 0;
        for (; x + UPS_PB <= w; x += UPS_PB)
            conv_px(row + (size_t)x * cin, sin, cin, cout, wv, bv, slope, add ? add + o + (size_t)x * cout : NULL,
                    out + o + (size_t)x * cout, UPS_PB);
        for (; x < w; x++)
            conv_px(row + (size_t)x * cin, sin, cin, cout, wv, bv, slope, add ? add + o + (size_t)x * cout : NULL,
                    out + o + (size_t)x * cout, 1);
    }
}

static void conv_head(const float *in, int w, int y0, int y1, float *out) {
    conv3x3(in, w, y0, y1, 3, CH, ups_head_w, ups_head_b, ups_head_a, NULL, out);
}

static void conv_body(const float *in, int w, int y0, int y1, int b, float *out) {
    conv3x3(in, w, y0, y1, CH, CH, ups_body_w[b], ups_body_b[b], ups_body_a[b], NULL, out);
}

static void conv_body_skip(const float *in, int w, int y0, int y1, int b, const float *add, float *out) {
    conv3x3(in, w, y0, y1, CH, CH, ups_body_w[b], ups_body_b[b], ups_body_a[b], add, out);
}

static void conv_tail(const float *in, int w, int y0, int y1, float *out) {
    conv3x3(in, w, y0, y1, CH, TAIL_OUT, ups_tail_w, ups_tail_b, NULL, NULL, out);
}

/* One network run, its rows shared out among threads: each computes its rows of a layer and their ends, waits
 * for the others, then the first fills the row above and the last the row below (only they read those). The
 * threads start together once all are created (a thread that couldn't be created leaves its rows to the others). */
typedef struct {
    const float *lr;
    int w, h, ws, wt, threads;
    float *in, *f, *a, *b, *r;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int go, waiting, generation;
} NetRun;

typedef struct {
    NetRun *n;
    int id;
} NetJob;

static void net_sync(NetRun *n, int id, float *m, int c) {
    if (n->threads > 1) {
        pthread_mutex_lock(&n->lock);
        const int gen = n->generation;
        if (++n->waiting == n->threads) {
            n->waiting = 0;
            n->generation++;
            pthread_cond_broadcast(&n->cond);
        } else {
            while (gen == n->generation)
                pthread_cond_wait(&n->cond, &n->lock);
        }
        pthread_mutex_unlock(&n->lock);
    }
    if (id == 0)
        fill_row(m, n->w, n->h, c, n->wt, 0);
    if (id == n->threads - 1)
        fill_row(m, n->w, n->h, c, n->wt, 1);
}

static void *net_rows(void *arg) {
    NetJob *job = arg;
    NetRun *n = job->n;
    pthread_mutex_lock(&n->lock);
    while (!n->go)
        pthread_cond_wait(&n->cond, &n->lock);
    pthread_mutex_unlock(&n->lock);
    const int id = job->id, w = n->w, h = n->h;
    if (id >= n->threads)
        return NULL;
    const int y0 = h * id / n->threads, y1 = h * (id + 1) / n->threads;
    for (int y = y0; y < y1; y++)
        for (int x = 0; x < w; x++)
            for (int c = 0; c < 3; c++)
                n->in[((size_t)(y + 1) * (w + 2) + x + 1) * 3 + c] = n->lr[((size_t)y * w + x) * 3 + c] - 0.5f;
    fill_ends(n->in, w, 3, n->ws, y0, y1);
    net_sync(n, id, n->in, 3);
    conv_head(n->in, w, y0, y1, n->f);
    fill_ends(n->f, w, CH, n->ws, y0, y1);
    net_sync(n, id, n->f, CH);
    const float *src = n->f;
    float *dst = n->a;
    for (int blk = 0; blk < UPS_BLOCKS; blk++) {   /* the last block adds the skip from the first layer */
        if (blk == UPS_BLOCKS - 1)
            conv_body_skip(src, w, y0, y1, blk, n->f, dst);
        else
            conv_body(src, w, y0, y1, blk, dst);
        fill_ends(dst, w, CH, n->ws, y0, y1);
        net_sync(n, id, dst, CH);
        src = dst;
        dst = dst == n->a ? n->b : n->a;
    }
    conv_tail(src, w, y0, y1, n->r);
    return NULL;
}

#define UPS_MAX_THREADS 16
static int s_threads;   /* pc_upscale_set_threads(); 0: one per processor, up to 8 */

void pc_upscale_set_threads(int n) {
    s_threads = n < 0 ? 0 : n > UPS_MAX_THREADS ? UPS_MAX_THREADS : n;
}

static int processors(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n < 1 ? 1 : (int)n;
#endif
}

/* lr: h x w x 3 in [0, 1] -> hr: 2h x 2w x 3 (not clamped). Returns -1 when out of memory. */
static int run_net(const float *lr, int w, int h, int ws, int wt, float *hr, float *tmp, int *map) {
    const size_t n = (size_t)(w + 2) * (h + 2);
    NetRun run;
    memset(&run, 0, sizeof run);
    run.lr = lr, run.w = w, run.h = h, run.ws = ws, run.wt = wt;
    run.in = malloc(n * 3 * sizeof(float));
    run.f = malloc(n * CH * sizeof(float));
    run.a = malloc(n * CH * sizeof(float));
    run.b = malloc(n * CH * sizeof(float));
    run.r = malloc(n * TAIL_OUT * sizeof(float));
    int rc = -1;
    if (!run.in || !run.f || !run.a || !run.b || !run.r)
        goto done;
    /* Threads when it's worth it: at least 4 rows and 256 texels each. */
    int t = s_threads ? s_threads : processors() < 8 ? processors() : 8;
    t = t > h / 4 ? h / 4 : t;
    t = t > w * h / 256 ? w * h / 256 : t;
    t = t < 1 ? 1 : t;
    NetJob jobs[UPS_MAX_THREADS];
    pthread_t tid[UPS_MAX_THREADS];
    int started = 1;
    pthread_mutex_init(&run.lock, NULL);
    pthread_cond_init(&run.cond, NULL);
    for (int i = 0; i < t; i++)
        jobs[i] = (NetJob){ &run, i };
    while (started < t && !pthread_create(&tid[started], NULL, net_rows, &jobs[started]))
        started++;
    pthread_mutex_lock(&run.lock);
    run.threads = started;
    run.go = 1;
    pthread_cond_broadcast(&run.cond);
    pthread_mutex_unlock(&run.lock);
    net_rows(&jobs[0]);
    for (int i = 1; i < started; i++)
        pthread_join(tid[i], NULL);
    pthread_cond_destroy(&run.cond);
    pthread_mutex_destroy(&run.lock);
    scale2x(lr, w, h, 3, ws, wt, k_cubic, hr, tmp, map);
    for (int y = 0; y < h; y++)   /* pixel shuffle: channel c * 4 + dy * 2 + dx -> (2y + dy, 2x + dx, c) */
        for (int x = 0; x < w; x++) {
            const float *v = run.r + ((size_t)(y + 1) * (w + 2) + x + 1) * TAIL_OUT;
            for (int c = 0; c < 3; c++)
                for (int dy = 0; dy < 2; dy++)
                    for (int dx = 0; dx < 2; dx++)
                        hr[((size_t)(2 * y + dy) * 2 * w + 2 * x + dx) * 3 + c] += v[c * 4 + dy * 2 + dx];
        }
    rc = 0;
done:
    free(run.in);
    free(run.f);
    free(run.a);
    free(run.b);
    free(run.r);
    return rc;
}

/* ---------------------------------------------------------------- transparent texels
 * Fully transparent texels take the mean colour of their visible 8-neighbours, in rings outward, until every texel
 * has one (texels with no visible texel at all keep theirs). */
static int fill_transparent(float *lr, const uint8_t *in, int w, int h, int ws, int wt) {
    const int n = w * h;
    uint8_t *ok = malloc((size_t)n * 2);
    float *col = malloc((size_t)n * 3 * sizeof(float));
    if (!ok || !col) {
        free(ok);
        free(col);
        return -1;
    }
    uint8_t *next = ok + n;
    for (int i = 0; i < n; i++)
        ok[i] = in[i * 4 + 3] != 0;
    for (;;) {
        int changed = 0, left = 0;
        memcpy(next, ok, (size_t)n);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                const int i = y * w + x;
                if (ok[i])
                    continue;
                float s[3] = { 0.0f, 0.0f, 0.0f };
                int cnt = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int yy = y + dy, xx = x + dx;
                        if ((dx == 0 && dy == 0) || (!wt && (yy < 0 || yy >= h)) || (!ws && (xx < 0 || xx >= w)))
                            continue;
                        const int j = edge(yy, h, wt) * w + edge(xx, w, ws);
                        if (!ok[j])
                            continue;
                        s[0] += lr[j * 3];
                        s[1] += lr[j * 3 + 1];
                        s[2] += lr[j * 3 + 2];
                        cnt++;
                    }
                if (cnt) {
                    for (int c = 0; c < 3; c++)
                        col[i * 3 + c] = s[c] / (float)cnt;
                    next[i] = 1;
                    changed = 1;
                } else {
                    left = 1;
                }
            }
        for (int i = 0; i < n; i++)
            if (next[i] && !ok[i])
                memcpy(lr + i * 3, col + i * 3, 3 * sizeof(float));
        memcpy(ok, next, (size_t)n);
        if (!left || !changed)
            break;
    }
    free(ok);
    free(col);
    return 0;
}

/* ---------------------------------------------------------------- binary alpha
 * EPX (Scale2x) on the cut-out's mask: corner (qx, qy) of texel (x, y) takes the value of the two neighbours
 * beside that corner when they agree with each other and not with the two opposite ones. Isolated texels and
 * one-texel lines stay whole, straight edges stay put, and diagonal staircases get their corners filled or cut.
 * Thresholding any interpolation of the mask at one half (bilinear, Catmull-Rom) comes out as nearest neighbour,
 * and thresholding a smoothed one loses thin lines and matches the held-out cut-outs less well (docs/port/UPSCALE.md). */
static int epx(const uint8_t *in, int w, int h, int ws, int wt, int x, int y, int qx, int qy) {
#define UPS_ON(xx, yy) (in[(edge(yy, h, wt) * w + edge(xx, w, ws)) * 4 + 3] != 0)
    const int p = UPS_ON(x, y);
    const int v = UPS_ON(x, qy ? y + 1 : y - 1), vo = UPS_ON(x, qy ? y - 1 : y + 1);   /* beside, opposite */
    const int hz = UPS_ON(qx ? x + 1 : x - 1, y), ho = UPS_ON(qx ? x - 1 : x + 1, y);
#undef UPS_ON
    return v == hz && v != vo && hz != ho ? v : p;
}

/* ---------------------------------------------------------------- the whole texture */
int pc_upscale_rgba2x_method(const uint8_t *in, int w, int h, int wrap_s, int wrap_t, uint8_t *out, int method) {
    if (!in || !out || w < 1 || h < 1 || w > PC_UPSCALE_MAX_TEXELS || h > PC_UPSCALE_MAX_TEXELS ||
        (long long)w * h > PC_UPSCALE_MAX_TEXELS || method < 0 || method >= PC_UPSCALE_METHODS)
        return -1;
    const int ws = wrap_s != 0, wt = wrap_t != 0, n = w * h, W = 2 * w;
    const size_t N = (size_t)4 * n;
    int opaque = 1, grey = 1, intensity = 1, binary = 1, any_clear = 0, any_seen = 0;
    for (int i = 0; i < n; i++) {
        const uint8_t *p = in + i * 4;
        opaque &= p[3] == 255;
        grey &= p[0] == p[1] && p[1] == p[2];
        intensity &= p[0] == p[1] && p[1] == p[2] && p[2] == p[3];
        binary &= p[3] == 0 || p[3] == 255;
        any_clear |= p[3] == 0;
        any_seen |= p[3] != 0;
    }
    /* scratch: lr colour, hr colour, lr alpha, hr alpha, the separable pass's rows, the edge maps */
    float *lr = malloc((size_t)n * 3 * sizeof(float)), *hr = malloc(N * 3 * sizeof(float));
    float *alr = malloc((size_t)n * sizeof(float)), *ahr = malloc(N * sizeof(float));
    float *tmp = malloc((size_t)n * 2 * 3 * sizeof(float));
    int *map = malloc((size_t)(w + h + 8) * sizeof(int));
    int rc = -1;
    if (!lr || !hr || !alr || !ahr || !tmp || !map)
        goto done;
    for (int i = 0; i < n * 3; i++)
        lr[i] = in[(i / 3) * 4 + i % 3] * (1.0f / 255.0f);
    const int filled = !intensity && any_clear && any_seen;
    if (filled && fill_transparent(lr, in, w, h, ws, wt))
        goto done;
    if (method == PC_UPSCALE_NET) {
        if (run_net(lr, w, h, ws, wt, hr, tmp, map))
            goto done;
    } else {
        scale2x(lr, w, h, 3, ws, wt, method == PC_UPSCALE_CUBIC ? k_cubic : k_bilinear, hr, tmp, map);
    }
    if (!opaque && !intensity && !binary) {
        for (int i = 0; i < n; i++)
            alr[i] = in[i * 4 + 3] * (1.0f / 255.0f);
        scale2x(alr, w, h, 1, ws, wt, k_cubic, ahr, tmp, map);
    }
    for (int Y = 0; Y < 2 * h; Y++) {
        /* the two nearest texel rows and columns of each new texel: (y, y -/+ 1) */
        const int y0 = Y >> 1, y1 = edge(y0 - 1 + 2 * (Y & 1), h, wt);
        for (int X = 0; X < W; X++) {
            const int x0 = X >> 1, x1 = edge(x0 - 1 + 2 * (X & 1), w, ws);
            const size_t o = (size_t)Y * W + X;
            float r = hr[o * 3], g = hr[o * 3 + 1], b = hr[o * 3 + 2];
            uint8_t a;
            if (grey || intensity)
                r = g = b = (r + g + b) * (1.0f / 3.0f);
            if (intensity) {
                a = to8(r);
            } else if (opaque) {
                a = 255;
            } else if (binary) {
                a = epx(in, w, h, ws, wt, x0, Y >> 1, X & 1, Y & 1) ? 255 : 0;
            } else {
                const uint8_t q[4] = { in[(y0 * w + x0) * 4 + 3], in[(y0 * w + x1) * 4 + 3],
                                       in[(y1 * w + x0) * 4 + 3], in[(y1 * w + x1) * 4 + 3] };
                uint8_t lo = q[0], hi = q[0];
                for (int k = 1; k < 4; k++) {
                    lo = q[k] < lo ? q[k] : lo;
                    hi = q[k] > hi ? q[k] : hi;
                }
                a = to8(ahr[o]);
                a = a < lo ? lo : a > hi ? hi : a;
            }
            uint8_t *d = out + o * 4;
            if (filled && a == 0) {   /* invisible: the original colours, bilinear */
                const uint8_t *p00 = in + (y0 * w + x0) * 4, *p01 = in + (y0 * w + x1) * 4;
                const uint8_t *p10 = in + (y1 * w + x0) * 4, *p11 = in + (y1 * w + x1) * 4;
                for (int c = 0; c < 3; c++)
                    d[c] = (uint8_t)((9 * p00[c] + 3 * p01[c] + 3 * p10[c] + p11[c] + 8) >> 4);
            } else {
                d[0] = to8(r);
                d[1] = to8(g);
                d[2] = to8(b);
            }
            d[3] = a;
        }
    }
    rc = 0;
done:
    free(lr);
    free(hr);
    free(alr);
    free(ahr);
    free(tmp);
    free(map);
    return rc;
}

int pc_upscale_rgba2x(const uint8_t *in, int w, int h, int wrap_s, int wrap_t, uint8_t *out) {
    return pc_upscale_rgba2x_method(in, w, h, wrap_s, wrap_t, out, PC_UPSCALE_NET);
}
