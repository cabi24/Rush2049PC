/* port/gfx/tests/clip_check.c: gfx_test's "clip" case, on the CPU (no GL): what pcgfx_tri_visible and
 * pcgfx_tri_output hand on for GL.
 * Its own file, since it needs the renderer's internal header, whose GBI names test_gbi.h defines again.
 *
 * Whatever a triangle is, every vertex GL gets lies in front of the eye (w >= 1e-4) and within the guard band
 * (|x| <= 2w as the view sees it, |y| <= 2w), as on the RSP: no vertex next to the eye projected millions of pixels
 * out, which GL drivers' clippers are free to handle worse than Mesa's (GFX.md "Clipping"). A triangle inside the
 * band passes untouched, one wholly behind the eye or outside one side is dropped. And every GL vertex is within
 * GL's own guard band, |x| <= 2w and |y| <= 2w in its clip coordinates, exactly: past it V3D clips the triangle
 * itself, with its colours and texture coordinates off (GFX.md "Clipping"). */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "gfx_internal.h"

static uint32_t s_seed = 12345;

static float rnd(float lo, float hi) {
    s_seed = s_seed * 1664525u + 1013904223u;
    return lo + (hi - lo) * (float)(s_seed >> 8) / 16777216.0f;
}

static void make(GfxVtx *v, float x, float y, float z, float w, float xs) {
    memset(v, 0, sizeof(*v));
    v->x = x;
    v->y = y;
    v->z = z;
    v->w = w;
    v->s = x * 0.25f;
    v->t = y * 0.25f;
    v->clip = pcgfx_outcode(v, xs);
}

int gfx_clip_check(void) {
    static const float xss[2] = {1.0f, 0.75f};   /* off; a view widened by 4/3 (16:9) */
    static GfxDrawState st;
    static GfxUnitInfo units[2];
    GfxTriCtx c;
    GfxVtx in[3], poly[8];
    GfxOutVtx out[GFX_TRI_OUT_MAX];
    int fails = 0, k, i, j, clipped = 0, gl_clipped = 0;
    memset(&c, 0, sizeof(c));
    c.vx = 160.0f;
    c.vp_scale[0] = 160.0f, c.vp_scale[1] = 120.0f, c.vp_scale[2] = 511.0f;
    c.vp_trans[0] = 160.0f, c.vp_trans[1] = 120.0f, c.vp_trans[2] = 511.0f;
    c.tw = 320, c.th = 240;
    for (j = 0; j < 2; j++) {
        c.xs = xss[j];
        for (k = 0; k < 20000; k++) {
            int n, kind = k % 4;
            for (i = 0; i < 3; i++) {
                /* kinds: anything; a road from behind the eye to far ahead; tiny w next to the eye; inside */
                float w = kind == 0 ? rnd(-2000.0f, 30000.0f)
                        : kind == 1 ? (i == 2 ? rnd(5000.0f, 32000.0f) : rnd(-300.0f, 0.0f))
                        : kind == 2 ? rnd(-1e-3f, 1e-3f)
                        : rnd(1.0f, 30000.0f);
                float r = kind == 3 ? w : kind == 2 ? 1e-3f : 30000.0f;
                make(&in[i], rnd(-r, r) / (kind == 3 ? c.xs : 1.0f), rnd(-r, r), rnd(-r, r), w, c.xs);
            }
            n = pcgfx_tri_visible(in, &c, poly);
            if (n > 8 || (n > 0 && n < 3)) {
                printf("  FAIL clip: %d vertices\n", n);
                fails++;
                continue;
            }
            if (kind == 3 && (n != 3 || memcmp(poly, in, sizeof(GfxVtx) * 3) != 0)) {
                printf("  FAIL clip: a triangle inside the guard band was changed (%d vertices)\n", n);
                fails++;
            }
            if (n != 3 || memcmp(poly, in, sizeof(GfxVtx) * 3) != 0)
                clipped++;
            if (n > 0) {   /* as GL gets it */
                int m = pcgfx_tri_output(poly, n, &in[0], &c, &st, units, out);
                gl_clipped += m != 3 * (n - 2);
                if (m % 3 != 0 || m > GFX_TRI_OUT_MAX) {
                    printf("  FAIL clip: %d GL vertices\n", m);
                    fails++;
                    m = 0;
                }
                for (i = 0; i < m; i++) {
                    const GfxOutVtx *o = &out[i];
                    if (!(fabsf(o->pos[0]) <= 2.0f * o->pos[3]) || !(fabsf(o->pos[1]) <= 2.0f * o->pos[3]) ||
                        !(o->pos[3] >= 1e-4f) || !isfinite(o->pos[2])) {
                        if (fails < 5)
                            printf("  FAIL clip: GL vertex %d of %d (kind %d, xs %.2f) at %g %g %g w %g\n", i, m, kind,
                                   c.xs, o->pos[0], o->pos[1], o->pos[2], o->pos[3]);
                        fails++;
                    }
                }
            }
            for (i = 0; i < n; i++) {
                const GfxVtx *v = &poly[i];
                /* on the band within float error: lerps from coordinates in the tens of thousands are off by
                 * about 1e-3 */
                float lim = 2.01f * v->w + 2e-3f;
                if (!(v->w >= 1e-4f) || fabsf(v->x * c.xs) > lim || fabsf(v->y) > lim ||
                    !isfinite(v->x) || !isfinite(v->y) || !isfinite(v->z) || !isfinite(v->s)) {
                    if (fails < 5)
                        printf("  FAIL clip: vertex %d of %d (kind %d, xs %.2f) at %g %g %g w %g\n", i, n, kind, c.xs,
                               v->x, v->y, v->z, v->w);
                    fails++;
                }
            }
        }
    }
    /* wholly behind the eye, and wholly past one side: nothing */
    c.xs = 1.0f;
    make(&in[0], 0, 0, 0, -1, 1.0f);
    make(&in[1], 100, 5, 0, -50, 1.0f);
    make(&in[2], -100, 5, 0, -2, 1.0f);
    if (pcgfx_tri_visible(in, &c, poly) != 0) {
        printf("  FAIL clip: a triangle behind the eye was drawn\n");
        fails++;
    }
    make(&in[0], 50, 0, 0, 10, 1.0f);
    make(&in[1], 5000, 5, 0, -50, 1.0f);
    make(&in[2], 70, -5, 0, 20, 1.0f);
    if (pcgfx_tri_visible(in, &c, poly) != 0) {
        printf("  FAIL clip: a triangle right of the view was drawn\n");
        fails++;
    }
    if (clipped < 10000) {
        printf("  FAIL clip: only %d of the triangles were clipped\n", clipped);
        fails++;
    }
    if (gl_clipped < 1000) {   /* the widened view's band reaches past GL's: 2 / 0.75 */
        printf("  FAIL clip: only %d of the polygons were clipped to GL's guard band\n", gl_clipped);
        fails++;
    }
    return fails;
}
