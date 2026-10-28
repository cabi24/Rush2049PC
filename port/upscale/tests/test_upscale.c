/* port/upscale/tests/test_upscale.c: ctest upscale.<case> (docs/port/UPSCALE.md). Synthetic textures only.
 *
 *   golden    four small textures (opaque, cut-out, translucent grey, intensity; wrapped, clamped, mixed) hash to a
 *             fixed FNV-1a: the output is the same bytes on every machine. After exporting new weights
 *             (tools/upscale/export_c.py), run `test_upscale golden` and put the hash it prints in GOLDEN.
 *   seams     a wrapped texture upscales to exactly the middle of its 3x3 (or 3x1) tiling upscaled clamped; a
 *             clamped edge takes nothing from the opposite edge, a wrapped one does
 *   alpha     binary alpha stays binary and keeps its shape, cut-outs keep their colour up to the edge (nothing of
 *             the transparent texels' colour), translucent alpha stays in range without ringing, grey stays grey,
 *             intensity textures keep alpha = intensity
 *   quality   on a synthetic edge-and-ramp image reduced 2x, the network beats bilinear and Catmull-Rom (PSNR)
 *   args      what it refuses
 */
#include "upscale.h"
#include "upscale_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GOLDEN 0x7a43f56bfeaa1e04ull

static int failures;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        if (!(c)) {                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
            failures++;                                                                                            \
        }                                                                                                          \
    } while (0)

static unsigned s_seed = 1;
static unsigned rnd(void) {
    s_seed = s_seed * 1103515245u + 12345u;
    return (s_seed >> 16) & 0x7FFF;
}

static uint8_t *up(const uint8_t *in, int w, int h, int ws, int wt, int method) {
    uint8_t *out = malloc((size_t)w * h * 16);
    int rc = pc_upscale_rgba2x_method(in, w, h, ws, wt, out, method);
    if (rc) {
        printf("FAIL: %dx%d refused\n", w, h);
        failures++;
    }
    return out;
}

/* A smooth colourful texture: low-frequency waves plus a little noise, integer-only so it is the same anywhere. */
static void pattern(uint8_t *p, int w, int h, int alpha_kind) {
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *t = p + (y * w + x) * 4;
            t[0] = (uint8_t)((x * 37 + y * 11 + (int)(rnd() % 24)) & 255);
            t[1] = (uint8_t)((x * x * 3 + y * 29 + (int)(rnd() % 24)) & 255);
            t[2] = (uint8_t)(((x ^ y) * 23 + 64 + (int)(rnd() % 24)) & 255);
            int dx = 2 * x - w + 1, dy = 2 * y - h + 1;
            t[3] = alpha_kind == 0 ? 255 : (dx * dx + dy * dy < w * h * 0.6 ? 255 : 0);
        }
}

static uint64_t fnv(uint64_t h, const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; i++)
        h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}

static uint64_t golden_hash(void) {
    uint64_t h = 0xCBF29CE484222325ull;
    s_seed = 7;
    {   /* opaque, wrapped both ways */
        uint8_t in[13 * 9 * 4];
        pattern(in, 13, 9, 0);
        uint8_t *o = up(in, 13, 9, 1, 1, PC_UPSCALE_NET);
        h = fnv(h, o, sizeof in * 4);
        free(o);
    }
    {   /* a cut-out, clamped */
        uint8_t in[10 * 12 * 4];
        pattern(in, 10, 12, 1);
        uint8_t *o = up(in, 10, 12, 0, 0, PC_UPSCALE_NET);
        h = fnv(h, o, sizeof in * 4);
        free(o);
    }
    {   /* translucent grey (IA8-like), wrapped in s only */
        uint8_t in[8 * 8 * 4];
        for (int i = 0; i < 64; i++) {
            in[i * 4] = in[i * 4 + 1] = in[i * 4 + 2] = (uint8_t)(rnd() % 16 * 17);
            in[i * 4 + 3] = (uint8_t)((i % 8) * 34);
        }
        uint8_t *o = up(in, 8, 8, 1, 0, PC_UPSCALE_NET);
        h = fnv(h, o, sizeof in * 4);
        free(o);
    }
    {   /* intensity (I4: alpha = intensity), clamped */
        uint8_t in[6 * 5 * 4];
        for (int i = 0; i < 30; i++)
            memset(in + i * 4, (int)(rnd() % 16 * 17), 4);
        uint8_t *o = up(in, 6, 5, 0, 0, PC_UPSCALE_NET);
        h = fnv(h, o, sizeof in * 4);
        free(o);
    }
    return h;
}

static int t_golden(void) {
    uint64_t h[3];
    for (int i = 0; i < 3; i++) {   /* threads: the default, one, three */
        pc_upscale_set_threads(i == 0 ? 0 : i == 1 ? 1 : 3);
        h[i] = golden_hash();
    }
    pc_upscale_set_threads(0);
    printf("golden: 0x%016llxull (expected 0x%016llxull), %s\n", (unsigned long long)h[0],
           (unsigned long long)GOLDEN, pc_upscale_model_name);
    CHECK(h[0] == GOLDEN, "the output changed: new weights (update GOLDEN) or a change in upscale.c's arithmetic");
    CHECK(h[1] == h[0] && h[2] == h[0], "the thread count changed the output (%016llx %016llx)",
          (unsigned long long)h[1], (unsigned long long)h[2]);
    return 0;
}

static void tile(const uint8_t *in, int w, int h, int nx, int ny, uint8_t *out) {
    for (int y = 0; y < h * ny; y++)
        for (int x = 0; x < w * nx; x++)
            memcpy(out + ((size_t)y * w * nx + x) * 4, in + ((y % h) * w + x % w) * 4, 4);
}

/* The middle copy of a (nx x ny) tiling's 2x output against the 2x output of the texture itself. */
static int same_as_middle(const uint8_t *small, const uint8_t *big, int w, int h, int nx, int ny) {
    const int W = 2 * w, bw = 2 * w * nx, ox = nx > 1 ? W : 0, oy = ny > 1 ? 2 * h : 0;
    for (int y = 0; y < 2 * h; y++)
        if (memcmp(small + (size_t)y * W * 4, big + ((size_t)(y + oy) * bw + ox) * 4, (size_t)W * 4))
            return 0;
    return 1;
}

static int t_seams(void) {
    enum { W = 16, H = 12 };
    for (int kind = 0; kind < 2; kind++) {
        uint8_t in[W * H * 4], *big = malloc(W * H * 9 * 4);
        s_seed = 3 + kind;
        pattern(in, W, H, kind);
        uint8_t *o = up(in, W, H, 1, 1, PC_UPSCALE_NET);
        tile(in, W, H, 3, 3, big);
        uint8_t *ob = up(big, W * 3, H * 3, 0, 0, PC_UPSCALE_NET);
        CHECK(same_as_middle(o, ob, W, H, 3, 3), "wrapped both ways (%s) differs from its tiling",
              kind ? "cut-out" : "opaque");
        free(o);
        free(ob);
        o = up(in, W, H, 1, 0, PC_UPSCALE_NET);
        tile(in, W, H, 3, 1, big);
        ob = up(big, W * 3, H, 0, 0, PC_UPSCALE_NET);
        CHECK(same_as_middle(o, ob, W, H, 3, 1), "wrapped in s only (%s) differs from its tiling",
              kind ? "cut-out" : "opaque");
        free(o);
        free(ob);
        o = up(in, W, H, 0, 1, PC_UPSCALE_NET);
        tile(in, W, H, 1, 3, big);
        ob = up(big, W, H * 3, 0, 0, PC_UPSCALE_NET);
        CHECK(same_as_middle(o, ob, W, H, 1, 3), "wrapped in t only (%s) differs from its tiling",
              kind ? "cut-out" : "opaque");
        free(o);
        free(ob);
        free(big);
    }
    /* Black left half, white right half (then top and bottom): a clamped edge stays black, a wrapped one
     * takes some of the white from the far edge. */
    for (int axis = 0; axis < 2; axis++) {
        uint8_t in[W * W * 4];
        for (int y = 0; y < W; y++)
            for (int x = 0; x < W; x++)
                memset(in + (y * W + x) * 4, (axis ? y : x) < W / 2 ? 0 : 255, 3), in[(y * W + x) * 4 + 3] = 255;
        uint8_t *c = up(in, W, W, 0, 0, PC_UPSCALE_NET);
        uint8_t *r = up(in, W, W, !axis, axis, PC_UPSCALE_NET);
        int cmax = 0, rmin = 255;
        for (int i = 0; i < 2 * W; i++) {
            const int o = axis ? i * 4 : i * 2 * W * 4;   /* first row or first column */
            cmax = c[o] > cmax ? c[o] : cmax;
            rmin = r[o] < rmin ? r[o] : rmin;
        }
        CHECK(cmax <= 8, "clamped %s edge took colour from the far edge (%d)", axis ? "top" : "left", cmax);
        CHECK(rmin >= cmax + 8, "wrapped %s edge didn't take colour from the far edge (%d)", axis ? "top" : "left",
              rmin);
        free(c);
        free(r);
    }
    return 0;
}

static int t_alpha(void) {
    enum { W = 24, H = 20 };
    for (int method = 0; method < PC_UPSCALE_METHODS; method++) {
        /* A green blob with holes on magenta transparent texels. */
        uint8_t in[W * H * 4];
        int n_in = 0;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                uint8_t *t = in + (y * W + x) * 4;
                const int dx = 2 * x - 23, dy = 2 * y - 19, d = dx * dx + dy * dy;
                const int on = d < 300 && !(x % 7 == 3 && y % 5 == 2) && !(x == 5 && y == 9);
                t[0] = on ? 40 : 255;
                t[1] = on ? 200 : 0;
                t[2] = on ? 60 : 255;
                t[3] = on ? 255 : 0;
                n_in += on;
            }
        uint8_t *o = up(in, W, H, 0, 0, method);
        int binary = 1, n_out = 0, kept = 1, cleared = 1, worst = 0;
        for (int y = 0; y < 2 * H; y++)
            for (int x = 0; x < 2 * W; x++) {
                const uint8_t *t = o + (y * 2 * W + x) * 4;
                binary &= t[3] == 0 || t[3] == 255;
                n_out += t[3] == 255;
                if (t[3]) {
                    const int e = abs(t[0] - 40) + abs(t[1] - 200) + abs(t[2] - 60);
                    worst = e > worst ? e : worst;
                }
                /* a texel whose 3x3 neighbourhood is all one alpha keeps it in all four new texels */
                const int sx = x / 2, sy = y / 2;
                int all_on = 1, all_off = 1;
                for (int j = -1; j <= 1; j++)
                    for (int i = -1; i <= 1; i++) {
                        const int xx = sx + i < 0 ? 0 : sx + i >= W ? W - 1 : sx + i;
                        const int yy = sy + j < 0 ? 0 : sy + j >= H ? H - 1 : sy + j;
                        all_on &= in[(yy * W + xx) * 4 + 3] == 255;
                        all_off &= in[(yy * W + xx) * 4 + 3] == 0;
                    }
                kept &= !all_on || t[3] == 255;
                cleared &= !all_off || t[3] == 0;
            }
        CHECK(binary, "method %d: binary alpha came out with other values", method);
        CHECK(kept && cleared, "method %d: alpha changed inside a cut-out (kept %d, cleared %d)", method, kept,
              cleared);
        CHECK(abs(n_out - 4 * n_in) * 10 <= 4 * n_in, "method %d: coverage %d, expected about %d", method, n_out,
              4 * n_in);
        CHECK(worst <= 24, "method %d: visible texels took the transparent colour (off by %d)", method, worst);
        free(o);
    }
    {   /* translucent: a horizontal alpha ramp in IA8 steps stays monotonic and in range, a flat alpha flat */
        uint8_t in[16 * 4 * 4];
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 16; x++) {
                uint8_t *t = in + (y * 16 + x) * 4;
                t[0] = 200, t[1] = 100, t[2] = 50, t[3] = (uint8_t)(x * 17);
            }
        uint8_t *o = up(in, 16, 4, 0, 0, PC_UPSCALE_NET);
        int mono = 1;
        for (int y = 0; y < 8; y++)
            for (int x = 1; x < 32; x++)
                mono &= o[(y * 32 + x) * 4 + 3] >= o[(y * 32 + x - 1) * 4 + 3];
        CHECK(mono, "translucent ramp rings");
        CHECK(o[3] == 0 && o[(31) * 4 + 3] == 255, "translucent ramp ends moved (%d, %d)", o[3], o[31 * 4 + 3]);
        free(o);
        for (int i = 0; i < 64; i++)
            in[i * 4 + 3] = 128;
        o = up(in, 16, 4, 1, 1, PC_UPSCALE_NET);
        int flat = 1;
        for (int i = 0; i < 256; i++)
            flat &= o[i * 4 + 3] == 128;
        CHECK(flat, "a flat alpha of 128 changed");
        free(o);
    }
    {   /* grey stays grey; intensity keeps alpha = intensity; fully transparent stays transparent */
        uint8_t g[12 * 12 * 4], it[12 * 12 * 4], z[12 * 12 * 4];
        s_seed = 11;
        for (int i = 0; i < 144; i++) {
            memset(g + i * 4, (int)(rnd() & 255), 3), g[i * 4 + 3] = 255;
            memset(it + i * 4, (int)(rnd() % 16 * 17), 4);
            z[i * 4] = (uint8_t)rnd(), z[i * 4 + 1] = (uint8_t)rnd(), z[i * 4 + 2] = (uint8_t)rnd(), z[i * 4 + 3] = 0;
        }
        uint8_t *og = up(g, 12, 12, 1, 1, PC_UPSCALE_NET), *oi = up(it, 12, 12, 0, 0, PC_UPSCALE_NET);
        uint8_t *oz = up(z, 12, 12, 0, 0, PC_UPSCALE_NET);
        int grey = 1, inten = 1, clear = 1;
        for (int i = 0; i < 576; i++) {
            grey &= og[i * 4] == og[i * 4 + 1] && og[i * 4 + 1] == og[i * 4 + 2] && og[i * 4 + 3] == 255;
            inten &= oi[i * 4] == oi[i * 4 + 1] && oi[i * 4 + 1] == oi[i * 4 + 2] && oi[i * 4 + 2] == oi[i * 4 + 3];
            clear &= oz[i * 4 + 3] == 0;
        }
        CHECK(grey, "a grey texture came out coloured");
        CHECK(inten, "an intensity texture's alpha left its intensity");
        CHECK(clear, "a fully transparent texture got alpha");
        free(og);
        free(oi);
        free(oz);
    }
    return 0;
}

/* ---------------------------------------------------------------- quality */
static double coverage_disc(double px, double py, double cx, double cy, double r) {
    int in = 0;
    for (int j = 0; j < 4; j++)
        for (int i = 0; i < 4; i++) {
            const double x = px + (i + 0.5) / 4, y = py + (j + 0.5) / 4;
            in += (x - cx) * (x - cx) + (y - cy) * (y - cy) < r * r;
        }
    return in / 16.0;
}

static double psnr(const uint8_t *a, const uint8_t *b, int n) {
    double se = 0;
    for (int i = 0; i < n; i++)
        if (i % 4 != 3)
            se += (double)(a[i] - b[i]) * (a[i] - b[i]);
    se /= n * 3 / 4.0;
    return se == 0 ? 99 : 10 * log10(255.0 * 255.0 / se);
}

static int t_quality(void) {
    enum { S = 64 };
    uint8_t *hr = malloc(S * S * 4), lr[S / 2 * S / 2 * 4];
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {   /* a ramp background, a disc, a diagonal bar */
            double r = 40 + 2.5 * x, g = 60 + 1.5 * y, b = 90;
            const double d = coverage_disc(x, y, 30.3, 33.7, 17.0);
            r = r * (1 - d) + 230 * d, g = g * (1 - d) + 40 * d, b = b * (1 - d) + 30 * d;
            int on = 0;
            for (int j = 0; j < 4; j++)
                for (int i = 0; i < 4; i++) {
                    const double u = x + (i + 0.5) / 4, v = y + (j + 0.5) / 4;
                    on += fabs(u - v * 0.6 - 30) < 2.2;
                }
            const double e = on / 16.0;
            r = r * (1 - e) + 250 * e, g = g * (1 - e) + 250 * e, b = b * (1 - e) + 250 * e;
            uint8_t *t = hr + (y * S + x) * 4;
            t[0] = (uint8_t)(r + 0.5), t[1] = (uint8_t)(g + 0.5), t[2] = (uint8_t)(b + 0.5), t[3] = 255;
        }
    for (int y = 0; y < S / 2; y++)   /* box 2x, then RGBA5551 */
        for (int x = 0; x < S / 2; x++)
            for (int c = 0; c < 4; c++) {
                const int s = hr[((2 * y) * S + 2 * x) * 4 + c] + hr[((2 * y) * S + 2 * x + 1) * 4 + c] +
                              hr[((2 * y + 1) * S + 2 * x) * 4 + c] + hr[((2 * y + 1) * S + 2 * x + 1) * 4 + c];
                int v = (s + 2) / 4;
                if (c < 3) {
                    v = (v * 31 + 127) / 255;
                    v = v << 3 | v >> 2;
                }
                lr[(y * S / 2 + x) * 4 + c] = (uint8_t)v;
            }
    double p[PC_UPSCALE_METHODS];
    for (int m = 0; m < PC_UPSCALE_METHODS; m++) {
        uint8_t *o = up(lr, S / 2, S / 2, 0, 0, m);
        p[m] = psnr(o, hr, S * S * 4);
        free(o);
    }
    printf("quality: network %.2f dB, Catmull-Rom %.2f dB, bilinear %.2f dB\n", p[PC_UPSCALE_NET],
           p[PC_UPSCALE_CUBIC], p[PC_UPSCALE_BILINEAR]);
    CHECK(p[PC_UPSCALE_NET] > p[PC_UPSCALE_BILINEAR] + 1.0, "the network doesn't beat bilinear by 1 dB");
    CHECK(p[PC_UPSCALE_NET] > p[PC_UPSCALE_CUBIC], "the network doesn't beat Catmull-Rom");
    free(hr);
    return 0;
}

static int t_args(void) {
    uint8_t in[4 * 4] = { 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255, 10, 20, 30, 255 }, out[64];
    CHECK(pc_upscale_rgba2x(NULL, 2, 2, 0, 0, out) == -1, "NULL input accepted");
    CHECK(pc_upscale_rgba2x(in, 2, 2, 0, 0, NULL) == -1, "NULL output accepted");
    CHECK(pc_upscale_rgba2x(in, 0, 2, 0, 0, out) == -1, "zero width accepted");
    CHECK(pc_upscale_rgba2x(in, 2, -1, 0, 0, out) == -1, "negative height accepted");
    CHECK(pc_upscale_rgba2x(in, PC_UPSCALE_MAX_TEXELS, 2, 0, 0, out) == -1, "too large accepted");
    CHECK(pc_upscale_rgba2x_method(in, 2, 2, 0, 0, out, PC_UPSCALE_METHODS) == -1, "bad method accepted");
    CHECK(pc_upscale_rgba2x(in, 1, 1, 1, 0, out) == 0, "1x1 refused");
    int flat = 1;
    for (int i = 0; i < 4; i++)
        flat &= abs(out[i * 4] - 10) <= 3 && abs(out[i * 4 + 1] - 20) <= 3 && abs(out[i * 4 + 2] - 30) <= 3 &&
                out[i * 4 + 3] == 255;
    CHECK(flat, "1x1 came out %d %d %d %d", out[0], out[1], out[2], out[3]);
    CHECK(pc_upscale_rgba2x(in, 2, 2, 0, 0, out) == 0, "2x2 refused");
    return 0;
}

int main(int argc, char **argv) {
    static const struct {
        const char *name;
        int (*fn)(void);
    } cases[] = { { "golden", t_golden }, { "seams", t_seams }, { "alpha", t_alpha }, { "quality", t_quality },
                  { "args", t_args } };
    int ran = 0;
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++)
        if (argc < 2 || !strcmp(argv[1], cases[i].name)) {
            cases[i].fn();
            ran++;
        }
    if (!ran) {
        fprintf(stderr, "unknown case %s\n", argv[1]);
        return 2;
    }
    printf("%s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
