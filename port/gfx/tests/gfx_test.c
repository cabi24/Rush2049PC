/* port/gfx/tests/gfx_test.c: renders display lists headless and checks pixels.
 *
 *   gfx_test <case>|all [--png DIR] [--window]
 *
 * --window initializes with a window (SDL2 builds; SDL_VIDEODRIVER=offscreen works without a display) and
 * presents to it; the checks still read the render targets.
 * Each case builds its lists with test_gbi.h, runs them through pc_gfx_run_dl, presents, and compares sample
 * points against values worked out from the N64 semantics. Exit status 0 = pass. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_port.h"
#include "test_gbi.h"

int pc_gfx_read_framebuffer(const void *cimg, uint8_t *rgba, int *w, int *h);
int pc_gfx_read_window(uint8_t *rgba, int *w, int *h);
void pc_gfx_set_widescreen(float aspect);
float pc_gfx_parse_aspect(const char *text, int *ok);
void pc_gfx_set_scale(int scale);
void pc_gfx_set_window_size(int w, int h);

/* ---------------------------------------------------------------- the memory functions port/ultra provides */

static uint8_t *s_rdram;

void *pc_n64_ptr(uint32_t a) {
    uint32_t p = a & 0x1FFFFFFFu;
    return p < PC_RDRAM_SIZE ? s_rdram + p : NULL;
}

uint32_t pc_n64_addr(const void *h) {
    const uint8_t *p = (const uint8_t *)h;
    return p >= s_rdram && p < s_rdram + PC_RDRAM_SIZE ? PC_RDRAM_BASE + (uint32_t)(p - s_rdram) : 0;
}

static struct {
    const uint8_t *p;
    size_t n;
} s_be[32];

void pc_mem_register_be(const void *start, size_t size) {
    int i;
    for (i = 0; i < 32; i++)
        if (s_be[i].p == NULL) {
            s_be[i].p = (const uint8_t *)start;
            s_be[i].n = size;
            return;
        }
}

void pc_mem_unregister_be(const void *start) {
    int i;
    for (i = 0; i < 32; i++)
        if (s_be[i].p == start)
            s_be[i].p = NULL;
}

int pc_mem_is_be(const void *p) {
    int i;
    for (i = 0; i < 32; i++)
        if (s_be[i].p != NULL && (const uint8_t *)p >= s_be[i].p && (const uint8_t *)p < s_be[i].p + s_be[i].n)
            return 1;
    return 0;
}

/* ---------------------------------------------------------------- frame and helpers */

#define FB_W 320
#define FB_H 240

static uint16_t s_fb[FB_W * FB_H], s_zb[FB_W * FB_H];
static PcGfxWords s_list[16384], s_sub[4096];
static uint8_t *s_px;
static int s_px_w, s_px_h, s_fail, s_window;
static const char *s_case, *s_png_dir;

typedef struct {
    int16_t ob[3];
    uint16_t flag;
    int16_t tc[2];
    uint8_t cn[4];
} Vtx;

typedef struct {
    int16_t vscale[4], vtrans[4];
} Vp;

typedef struct {
    uint8_t col[3], pad1, colc[3], pad2;
    int8_t dir[3], pad3;
} Light;

typedef struct {
    uint32_t m[16];   /* guMtxF2L layout: integer halves then fractions, two per word */
} Mtx;

static Vp s_vp = {{FB_W * 2, FB_H * 2, 511, 0}, {FB_W * 2, FB_H * 2, 511, 0}};
static Mtx s_ident, s_ortho;

static void mtx_f2l(const float f[4][4], Mtx *m) {
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 2; j++) {
            int32_t e1 = (int32_t)lrintf(f[i][j * 2] * 65536.0f), e2 = (int32_t)lrintf(f[i][j * 2 + 1] * 65536.0f);
            m->m[i * 2 + j] = ((uint32_t)e1 & 0xFFFF0000u) | (((uint32_t)e2 >> 16) & 0xFFFF);
            m->m[8 + i * 2 + j] = ((uint32_t)e1 << 16) | ((uint32_t)e2 & 0xFFFF);
        }
}

static void mtx_ident_f(float f[4][4]) {
    memset(f, 0, sizeof(float) * 16);
    f[0][0] = f[1][1] = f[2][2] = f[3][3] = 1.0f;
}

static void mtx_trans(Mtx *m, float x, float y, float z) {
    float f[4][4];
    mtx_ident_f(f);
    f[3][0] = x;
    f[3][1] = y;
    f[3][2] = z;
    mtx_f2l(f, m);
}

/* guOrtho */
static void mtx_ortho(Mtx *m, float l, float r, float b, float t, float n, float fa) {
    float f[4][4];
    mtx_ident_f(f);
    f[0][0] = 2.0f / (r - l);
    f[1][1] = 2.0f / (t - b);
    f[2][2] = -2.0f / (fa - n);
    f[3][0] = -(r + l) / (r - l);
    f[3][1] = -(t + b) / (t - b);
    f[3][2] = -(fa + n) / (fa - n);
    mtx_f2l(f, m);
}

/* guPerspective (no perspNorm scaling) */
static void mtx_persp(Mtx *m, float fovy, float aspect, float n, float fa) {
    float f[4][4], c = 1.0f / tanf(fovy * 3.14159265f / 360.0f);
    memset(f, 0, sizeof(f));
    f[0][0] = c / aspect;
    f[1][1] = c;
    f[2][2] = (n + fa) / (n - fa);
    f[2][3] = -1.0f;
    f[3][2] = 2.0f * n * fa / (n - fa);
    mtx_f2l(f, m);
}

static void vtx(Vtx *v, int x, int y, int z, int s, int t, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    v->ob[0] = (int16_t)x;
    v->ob[1] = (int16_t)y;
    v->ob[2] = (int16_t)z;
    v->flag = 0;
    v->tc[0] = (int16_t)s;
    v->tc[1] = (int16_t)t;
    v->cn[0] = r;
    v->cn[1] = g;
    v->cn[2] = b;
    v->cn[3] = a;
}

static uint16_t rgba16(int r, int g, int b, int a) {
    return (uint16_t)((r >> 3) << 11 | (g >> 3) << 6 | (b >> 3) << 1 | (a ? 1 : 0));
}

static int x5(int v) {   /* what a 5-bit channel decodes to */
    v >>= 3;
    return v << 3 | v >> 2;
}

/* Clears z and color, full scissor, viewport, ortho projection (pixels, y down) and identity modelview. */
static void frame_begin(int r, int g, int b) {
    uint16_t c = rgba16(r, g, b, 1);
    gp = s_list;
    g_be_mode = 0;
    gsDPPipeSync();
    gsDPSetScissor(0, 0, FB_W, FB_H);
    gsDPSetCycleType(G_CYC_FILL);
    gsDPSetRenderMode(0, 0);
    gsDPSetDepthImage((uintptr_t)s_zb);
    gsDPSetColorImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, FB_W, (uintptr_t)s_zb);
    gsDPSetFillColor(0xFFFCFFFCu);
    gsDPFillRectangle(0, 0, FB_W - 1, FB_H - 1);
    gsDPPipeSync();
    gsDPSetColorImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, FB_W, (uintptr_t)s_fb);
    gsDPSetFillColor((uint32_t)c << 16 | c);
    gsDPFillRectangle(0, 0, FB_W - 1, FB_H - 1);
    gsDPPipeSync();
    gsSPViewport((uintptr_t)&s_vp);
    gsSPMatrix((uintptr_t)&s_ortho, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gsSPMatrix((uintptr_t)&s_ident, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gsSPGeometryMode(0xFFFFFFFF, G_SHADE | G_SHADING_SMOOTH);
    gsDPSetCycleType(G_CYC_1CYCLE);
    gsDPSetRenderMode(RM_OPA_SURF(1), RM_OPA_SURF(2));
    gsDPSetTextureLUT(G_TT_NONE);
    gsDPSetTextureFilter(G_TF_POINT);
    gsDPSetAlphaCompare(G_AC_NONE);
    gsDPSetDepthSource(G_ZS_PIXEL);
}

static void frame_end(void) {
    char path[512];
    gsDPFullSync();
    gsSPEndDisplayList();
    pc_gfx_run_dl(s_list);
    pc_gfx_present(s_fb);
    free(s_px);
    pc_gfx_read_framebuffer(s_fb, NULL, &s_px_w, &s_px_h);
    s_px = (uint8_t *)malloc((size_t)s_px_w * s_px_h * 4);
    pc_gfx_read_framebuffer(s_fb, s_px, &s_px_w, &s_px_h);
    if (s_window && s_px_w == FB_W) {   /* 640x480 window, 320x240 frame: shown at exactly 2x */
        int ww, wh, x, y, bad = 0;
        uint8_t *win;
        pc_gfx_read_window(NULL, &ww, &wh);
        win = (uint8_t *)malloc((size_t)ww * wh * 4);
        if (pc_gfx_read_window(win, &ww, &wh) != 0 || ww != 2 * FB_W || wh != 2 * FB_H) {
            printf("  FAIL %s window readback (%dx%d)\n", s_case, ww, wh);
            s_fail++;
        } else {
            for (y = 0; y < FB_H; y += 7)
                for (x = 0; x < FB_W; x += 7)
                    if (memcmp(win + ((size_t)(2 * y + 1) * ww + 2 * x + 1) * 4, s_px + ((size_t)y * s_px_w + x) * 4, 3))
                        bad++;
            if (bad) {
                printf("  FAIL %s window differs from the frame at %d samples\n", s_case, bad);
                s_fail++;
            }
        }
        free(win);
    }
    if (s_png_dir != NULL) {
        snprintf(path, sizeof(path), "%s/%s.png", s_png_dir, s_case);
        pc_gfx_save_screenshot(path);
    }
}

/* Target pixel (x, y) in N64 pixels: the middle of its block at the internal scale (RUSH_GFX_SCALE). */
static const uint8_t *tpx(int x, int y) {
    int k = s_px_h / FB_H;
    return s_px + ((size_t)(y * k + k / 2) * s_px_w + x * k + k / 2) * 4;
}

/* The 4:3 frame's left side in the target: 0, or in widescreen (RUSH_GFX_WIDESCREEN) the pixels added on each side. */
static int frame_x0(void) {
    return (s_px_w / (s_px_h / FB_H) - FB_W) / 2;
}

/* N64 pixel (x, y) of the 4:3 frame */
static const uint8_t *px(int x, int y) {
    return tpx(x + frame_x0(), y);
}

static void check_px(const char *what, const uint8_t *p, int x, int y, int r, int g, int b, int tol) {
    if (abs(p[0] - r) > tol || abs(p[1] - g) > tol || abs(p[2] - b) > tol) {
        printf("  FAIL %s %s at (%d,%d): got %d,%d,%d want %d,%d,%d (tol %d)\n", s_case, what, x, y, p[0], p[1],
               p[2], r, g, b, tol);
        s_fail++;
    }
}

static void check(const char *what, int x, int y, int r, int g, int b, int tol) {
    check_px(what, px(x, y), x, y, r, g, b, tol);
}

/* the same at a target pixel (widescreen cases) */
static void check_at(const char *what, int x, int y, int r, int g, int b, int tol) {
    check_px(what, tpx(x, y), x, y, r, g, b, tol);
}

/* ---------------------------------------------------------------- cases */

static void case_fill(void) {
    frame_begin(0, 0, 255);
    gsDPSetCycleType(G_CYC_FILL);
    gsDPSetFillColor((uint32_t)rgba16(255, 0, 0, 1) << 16 | rgba16(255, 0, 0, 1));
    gsDPFillRectangle(10, 10, 19, 19);   /* fill mode: lower-right inclusive */
    gsDPPipeSync();
    gsDPSetCycleType(G_CYC_1CYCLE);
    gsDPSetPrimColor(0, 0, 0, 255, 0, 255);
    cc_prim();
    gsDPFillRectangle(40, 10, 50, 20);   /* 1-cycle: exclusive */
    gsDPSetScissor(100, 0, 110, FB_H);
    gsDPSetCycleType(G_CYC_FILL);
    gsDPFillRectangle(90, 30, 120, 40);  /* clipped to the scissor */
    frame_end();
    check("clear", 5, 5, 0, 0, 255, 0);
    check("fill inside", 10, 10, 255, 0, 0, 0);
    check("fill lr inclusive", 19, 19, 255, 0, 0, 0);
    check("fill outside", 20, 20, 0, 0, 255, 0);
    check("1cyc rect", 49, 19, 0, 255, 0, 2);
    check("1cyc rect lr exclusive", 50, 20, 0, 0, 255, 0);
    check("scissored", 105, 35, 255, 0, 0, 0);
    check("scissor left", 99, 35, 0, 0, 255, 0);
    check("scissor right", 110, 35, 0, 0, 255, 0);
}

static Vtx s_v[64];

static void case_shade(void) {
    frame_begin(0, 0, 0);
    cc_shade();
    vtx(&s_v[0], 20, 20, 0, 0, 0, 255, 0, 0, 255);
    vtx(&s_v[1], 20, 220, 0, 0, 0, 0, 255, 0, 255);
    vtx(&s_v[2], 220, 20, 0, 0, 0, 0, 0, 255, 255);
    /* flat triangle: first vertex's color */
    vtx(&s_v[3], 240, 20, 0, 0, 0, 255, 255, 0, 255);
    vtx(&s_v[4], 240, 120, 0, 0, 0, 0, 0, 0, 255);
    vtx(&s_v[5], 300, 20, 0, 0, 0, 0, 0, 0, 255);
    gsSPVertex((uintptr_t)s_v, 6, 0);
    gsSP1Triangle(0, 1, 2);
    gsSPGeometryMode(G_SHADING_SMOOTH, 0);
    gsSP1Triangle(3, 4, 5);
    /* MODIFYVTX: recolor vertex 5 (loaded at slot 5), then redraw a smooth triangle using it */
    gsSPGeometryMode(0, G_SHADING_SMOOTH);
    vtx(&s_v[6], 240, 140, 0, 0, 0, 0, 0, 0, 255);
    vtx(&s_v[7], 240, 230, 0, 0, 0, 0, 0, 0, 255);
    vtx(&s_v[8], 300, 140, 0, 0, 0, 0, 0, 0, 255);
    gsSPVertex((uintptr_t)&s_v[6], 3, 10);
    gsSPModifyVertex(10, 0x10, 0x00FF00FF);
    gsSPModifyVertex(11, 0x10, 0x00FF00FF);
    gsSPModifyVertex(12, 0x10, 0x00FF00FF);
    gsSP1Triangle(10, 11, 12);
    frame_end();
    check("red corner", 23, 23, 255, 0, 0, 12);
    check("green corner", 22, 214, 0, 255, 0, 12);
    check("blue corner", 214, 22, 0, 0, 255, 12);
    check("middle", 80, 80, 102, 77, 77, 4);   /* weights 0.4, 0.3, 0.3 */
    check("outside", 200, 200, 0, 0, 0, 0);
    check("flat", 250, 40, 255, 255, 0, 2);
    check("modifyvtx", 250, 160, 0, 255, 0, 2);
}

static void case_zbuf(void) {
    int order;
    frame_begin(0, 0, 0);
    gsDPSetRenderMode(RM_ZB_OPA_SURF(1), RM_ZB_OPA_SURF(2));
    gsSPGeometryMode(0, G_ZBUFFER);
    cc_shade();
    for (order = 0; order < 2; order++) {
        int x = order * 160, near = order ? 0 : 3, far = order ? 3 : 0;
        Vtx *v = &s_v[order * 6];
        /* near (z = +400, red) and far (z = -400, green) quads overlapping in the middle */
        vtx(&v[near + 0], x + 10, 50, 400, 0, 0, 255, 0, 0, 255);
        vtx(&v[near + 1], x + 10, 150, 400, 0, 0, 255, 0, 0, 255);
        vtx(&v[near + 2], x + 100, 100, 400, 0, 0, 255, 0, 0, 255);
        vtx(&v[far + 0], x + 140, 50, -400, 0, 0, 0, 255, 0, 255);
        vtx(&v[far + 1], x + 50, 100, -400, 0, 0, 0, 255, 0, 255);
        vtx(&v[far + 2], x + 140, 150, -400, 0, 0, 0, 255, 0, 255);
        gsSPVertex((uintptr_t)v, 6, 0);
        gsSP1Triangle(0, 1, 2);
        gsSP1Triangle(3, 4, 5);
    }
    /* primitive depth: a rectangle at the far depth hides behind the near triangle */
    gsDPSetDepthSource(G_ZS_PRIM);
    gsDPSetPrimDepth(0x7000, 0);
    gsDPSetPrimColor(0, 0, 255, 255, 255, 255);
    cc_prim();
    gsDPFillRectangle(30, 95, 40, 105);
    frame_end();
    check("near-first overlap", 75, 100, 255, 0, 0, 2);
    check("near-last overlap", 160 + 75, 100, 255, 0, 0, 2);
    check("far only", 130, 100, 0, 255, 0, 2);
    check("prim depth hidden", 35, 100, 255, 0, 0, 2);
    check("near only", 20, 100, 255, 0, 0, 2);
}

/* ---- textures: every format, LOADBLOCK and LOADTILE */

static uint8_t s_tex8[64 * 64];
static uint16_t s_tex16[64 * 64];
static uint32_t s_tex32[64 * 64];
static uint16_t s_pal[256];

typedef struct {
    const char *name;
    int fmt, siz, tlut;
} Fmt;

static const Fmt s_fmts[] = {
    {"rgba16", G_IM_FMT_RGBA, G_IM_SIZ_16b, 0}, {"rgba32", G_IM_FMT_RGBA, G_IM_SIZ_32b, 0},
    {"ia16", G_IM_FMT_IA, G_IM_SIZ_16b, 0},     {"ia8", G_IM_FMT_IA, G_IM_SIZ_8b, 0},
    {"ia4", G_IM_FMT_IA, G_IM_SIZ_4b, 0},       {"i8", G_IM_FMT_I, G_IM_SIZ_8b, 0},
    {"i4", G_IM_FMT_I, G_IM_SIZ_4b, 0},         {"ci8", G_IM_FMT_CI, G_IM_SIZ_8b, 1},
    {"ci4", G_IM_FMT_CI, G_IM_SIZ_4b, 1},       {"ci8-ia", G_IM_FMT_CI, G_IM_SIZ_8b, 2},
};

/* texel (x, y) of a W-wide image in the source arrays; expected RGBA8 */
static void expect_texel(const Fmt *f, int W, int x, int y, int pal_bank, uint8_t o[4]) {
    int i = y * W + x, v;
    uint16_t c;
    switch (f->siz) {
    case G_IM_SIZ_4b:
        v = s_tex8[i >> 1];
        v = (i & 1) ? v & 15 : v >> 4;
        break;
    case G_IM_SIZ_8b:
        v = s_tex8[i];
        break;
    case G_IM_SIZ_16b:
        v = s_tex16[i];
        break;
    default:
        o[0] = s_tex32[i] >> 24;
        o[1] = s_tex32[i] >> 16;
        o[2] = s_tex32[i] >> 8;
        o[3] = s_tex32[i];
        return;
    }
    if (f->tlut) {
        c = s_pal[f->siz == G_IM_SIZ_4b ? (pal_bank << 4 | v) : v];
        if (f->tlut == 2) {
            o[0] = o[1] = o[2] = c >> 8;
            o[3] = c & 0xFF;
        } else {
            o[0] = (uint8_t)(((c >> 11) & 31) << 3 | ((c >> 11) & 31) >> 2);
            o[1] = (uint8_t)(((c >> 6) & 31) << 3 | ((c >> 6) & 31) >> 2);
            o[2] = (uint8_t)(((c >> 1) & 31) << 3 | ((c >> 1) & 31) >> 2);
            o[3] = (c & 1) ? 255 : 0;
        }
        return;
    }
    switch (f->fmt << 4 | f->siz) {
    case G_IM_FMT_RGBA << 4 | G_IM_SIZ_16b:
        c = (uint16_t)v;
        o[0] = (uint8_t)(((c >> 11) & 31) << 3 | ((c >> 11) & 31) >> 2);
        o[1] = (uint8_t)(((c >> 6) & 31) << 3 | ((c >> 6) & 31) >> 2);
        o[2] = (uint8_t)(((c >> 1) & 31) << 3 | ((c >> 1) & 31) >> 2);
        o[3] = (c & 1) ? 255 : 0;
        break;
    case G_IM_FMT_IA << 4 | G_IM_SIZ_16b:
        o[0] = o[1] = o[2] = (uint8_t)(v >> 8);
        o[3] = (uint8_t)v;
        break;
    case G_IM_FMT_IA << 4 | G_IM_SIZ_8b:
        o[0] = o[1] = o[2] = (uint8_t)((v >> 4) * 17);
        o[3] = (uint8_t)((v & 15) * 17);
        break;
    case G_IM_FMT_IA << 4 | G_IM_SIZ_4b:
        o[0] = o[1] = o[2] = (uint8_t)((v >> 1) * 255 / 7);
        o[3] = (v & 1) ? 255 : 0;
        break;
    case G_IM_FMT_I << 4 | G_IM_SIZ_8b:
        o[0] = o[1] = o[2] = o[3] = (uint8_t)v;
        break;
    default:   /* I4 */
        o[0] = o[1] = o[2] = o[3] = (uint8_t)(v * 17);
        break;
    }
}

static void fill_sources(void) {
    int i;
    for (i = 0; i < 64 * 64; i++) {
        s_tex8[i] = (uint8_t)(i * 37 + (i >> 6) * 11 + 5);
        s_tex16[i] = (uint16_t)(i * 2654435761u >> 7);
        s_tex32[i] = (uint32_t)(i * 2654435761u) ^ 0x5A3C0F00u;
    }
    for (i = 0; i < 256; i++)
        s_pal[i] = (uint16_t)(i * 40503u + 7);
}

static void draw_tex(int x, int y, int w, int h, int alpha) {
    if (alpha)
        cc_texalpha();
    else
        cc_decalrgba();
    gsSPTextureRectangle(x << 2, y << 2, (x + w) << 2, (y + h) << 2, 0, 0, 0, 1 << 10, 1 << 10);
}

static void check_tex(const Fmt *f, const char *how, int x0, int y0, int W, int tx0, int ty0, int w, int h,
                      int alpha, int bank) {
    int x, y, bad = 0;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            uint8_t e[4];
            const uint8_t *p = px(x0 + x, y0 + y);
            expect_texel(f, W, tx0 + x, ty0 + y, bank, e);
            if (alpha ? abs(p[0] - e[3]) > 1 : (abs(p[0] - e[0]) > 1 || abs(p[1] - e[1]) > 1 || abs(p[2] - e[2]) > 1)) {
                if (bad++ < 3)
                    printf("  FAIL %s %s %s %s texel (%d,%d): got %d,%d,%d want %d,%d,%d,%d\n", s_case, f->name,
                           how, alpha ? "alpha" : "rgb", tx0 + x, ty0 + y, p[0], p[1], p[2], e[0], e[1], e[2],
                           e[3]);
            }
        }
    if (bad)
        s_fail++;
}

static uintptr_t src_of(const Fmt *f) {
    return f->siz == G_IM_SIZ_32b ? (uintptr_t)s_tex32 : f->siz == G_IM_SIZ_16b ? (uintptr_t)s_tex16
                                                                                 : (uintptr_t)s_tex8;
}

static void case_textures(void) {
    int i;
    fill_sources();
    for (i = 0; i < (int)(sizeof(s_fmts) / sizeof(s_fmts[0])); i++) {
        const Fmt *f = &s_fmts[i];
        int W = 16, H = 8, y = 4 + i * 23;   /* 16 x 8 texels */
        frame_begin(0, 0, 0);
        gsDPSetTextureLUT(f->tlut == 2 ? G_TT_IA16 : f->tlut ? G_TT_RGBA16 : G_TT_NONE);
        if (f->tlut) {
            if (f->siz == G_IM_SIZ_4b)
                load_tlut_pal16(3, (uintptr_t)&s_pal[3 * 16]);
            else
                load_tlut_pal256((uintptr_t)s_pal);
        }
        /* LOADBLOCK of the whole 16 x 8 image */
        load_texture_block(src_of(f), f->fmt, f->siz, W, H, 3, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
        draw_tex(10, y, W, H, 0);
        draw_tex(30, y, W, H, 1);
        /* LOADTILE of a sub-rectangle (texels 2..13, 1..6) out of a 64-wide image */
        load_texture_tile(src_of(f), f->fmt, f->siz, 64, 2, 1, 13, 6, 3, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
        cc_decalrgba();
        gsSPTextureRectangle(60 << 2, y << 2, (60 + 12) << 2, (y + 6) << 2, 0, 2 << 5, 1 << 5, 1 << 10, 1 << 10);
        cc_texalpha();
        gsSPTextureRectangle(80 << 2, y << 2, (80 + 12) << 2, (y + 6) << 2, 0, 2 << 5, 1 << 5, 1 << 10, 1 << 10);
        frame_end();
        check_tex(f, "loadblock", 10, y, W, 0, 0, W, H, 0, 3);
        check_tex(f, "loadblock", 30, y, W, 0, 0, W, H, 1, 3);
        check_tex(f, "loadtile", 60, y, 64, 2, 1, 12, 6, 0, 3);
        check_tex(f, "loadtile", 80, y, 64, 2, 1, 12, 6, 1, 3);
    }
}

/* wrap, mirror and clamp of a 4x4 texture drawn over 16x16 pixels */
static void case_texwrap(void) {
    static uint16_t t4[16];
    int i, x, y;
    for (i = 0; i < 16; i++)
        t4[i] = rgba16((i & 3) * 80, (i >> 2) * 80, 255, 1);
    frame_begin(0, 0, 0);
    cc_decalrgba();
    load_texture_block((uintptr_t)t4, G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 4, 0, 0, G_TX_WRAP, G_TX_WRAP, 2, 2);
    gsSPTextureRectangle(10 << 2, 10 << 2, 26 << 2, 26 << 2, 0, 0, 0, 1 << 10, 1 << 10);
    load_texture_block((uintptr_t)t4, G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 4, 0, 0, G_TX_MIRROR, G_TX_MIRROR, 2, 2);
    gsSPTextureRectangle(40 << 2, 10 << 2, 56 << 2, 26 << 2, 0, 0, 0, 1 << 10, 1 << 10);
    load_texture_block((uintptr_t)t4, G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 4, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 2, 2);
    gsSPTextureRectangle(70 << 2, 10 << 2, 86 << 2, 26 << 2, 0, 0, 0, 1 << 10, 1 << 10);
    frame_end();
    for (y = 0; y < 16; y += 3)
        for (x = 0; x < 16; x += 3) {
            int ws = x & 3, wt = y & 3;
            int ms = (x & 4) ? 3 - (x & 3) : x & 3, mt = (y & 4) ? 3 - (y & 3) : y & 3;
            int cs = x > 3 ? 3 : x, ct = y > 3 ? 3 : y;
            check("wrap", 10 + x, 10 + y, x5(ws * 80), x5(wt * 80), 255, 1);
            check("mirror", 40 + x, 10 + y, x5(ms * 80), x5(mt * 80), 255, 1);
            check("clamp", 70 + x, 10 + y, x5(cs * 80), x5(ct * 80), 255, 1);
        }
}

/* texrect: copy mode (inclusive, 4x dsdx), 1-cycle 2x scale, flip in both, negative dsdx in copy mode */
static void case_texrect(void) {
    static uint16_t t[8 * 8];
    int i, x, y;
    for (i = 0; i < 64; i++)
        t[i] = rgba16((i & 7) * 32, (i >> 3) * 32, 128, (i & 7) != 7);   /* column 7 transparent */
    frame_begin(0, 0, 0);
    load_texture_block((uintptr_t)t, G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 8, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    gsDPSetCycleType(G_CYC_COPY);
    gsDPSetRenderMode(0, 0);
    gsSPTextureRectangle(10 << 2, 10 << 2, 17 << 2, 17 << 2, 0, 0, 0, 4 << 10, 1 << 10);
    gsSPTextureRectangleFlip(30 << 2, 10 << 2, 37 << 2, 17 << 2, 0, 0, 0, 4 << 10, 1 << 10);
    /* mirrored horizontally: start at the right column, step back */
    gsSPTextureRectangle(50 << 2, 10 << 2, 57 << 2, 17 << 2, 0, 7 << 5, 0, (uint16_t)(-4 << 10), 1 << 10);
    /* alpha compare in copy mode: the transparent column is skipped */
    gsDPSetAlphaCompare(G_AC_THRESHOLD);
    gsSPTextureRectangle(70 << 2, 10 << 2, 77 << 2, 17 << 2, 0, 0, 0, 4 << 10, 1 << 10);
    gsDPSetAlphaCompare(G_AC_NONE);
    gsDPSetCycleType(G_CYC_1CYCLE);
    gsDPSetRenderMode(RM_OPA_SURF(1), RM_OPA_SURF(2));
    cc_decalrgba();
    gsSPTextureRectangle(10 << 2, 40 << 2, 26 << 2, 56 << 2, 0, 0, 0, 1 << 9, 1 << 9);
    gsSPTextureRectangleFlip(40 << 2, 40 << 2, 48 << 2, 48 << 2, 0, 0, 0, 1 << 10, 1 << 10);
    frame_end();
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++) {
            int r = x5(x * 32), g = x5(y * 32), b = x5(128);
            check("copy", 10 + x, 10 + y, r, g, b, 1);
            check("copy flip", 30 + y, 10 + x, r, g, b, 1);
            check("copy -dsdx", 57 - x, 10 + y, r, g, b, 1);
            if (x == 7)
                check("copy alpha skip", 70 + x, 10 + y, 0, 0, 0, 0);
            else
                check("copy alpha", 70 + x, 10 + y, r, g, b, 1);
            check("1cyc 2x", 10 + 2 * x, 40 + 2 * y, r, g, b, 1);
            check("1cyc 2x b", 11 + 2 * x, 41 + 2 * y, r, g, b, 1);
            check("1cyc flip", 40 + y, 40 + x, r, g, b, 1);
        }
    check("copy inclusive edge", 18, 10, 0, 0, 0, 0);
    check("1cyc exclusive edge", 26, 40, 0, 0, 0, 0);
}

/* a lit mesh: directional + ambient, the light following the modelview; texgen from the lookat */
static void case_lit(void) {
    static Light amb = {{0, 40, 0}, 0, {0, 40, 0}, 0, {0, 0, 0}, 0};
    static Light dir = {{200, 0, 0}, 0, {200, 0, 0}, 0, {0, 0, 127}, 0};
    static Light lax = {{0, 0, 0}, 0, {0, 0, 0}, 0, {127, 0, 0}, 0};
    static Light lay = {{0, 0, 0}, 0, {0, 0, 0}, 0, {0, 127, 0}, 0};
    static uint16_t tex[32 * 32];
    static Mtx rot;
    int i;
    float f[4][4];
    for (i = 0; i < 32 * 32; i++)
        tex[i] = (i & 31) < 16 ? rgba16(255, 0, 0, 1) : rgba16(0, 255, 0, 1);
    /* normals in the color bytes: facing the light, at 60 degrees, perpendicular */
    vtx(&s_v[0], 10, 10, 0, 0, 0, 0, 0, 127, 255);
    vtx(&s_v[1], 10, 60, 0, 0, 0, 0, 0, 127, 255);
    vtx(&s_v[2], 60, 10, 0, 0, 0, 0, 0, 127, 255);
    vtx(&s_v[3], 80, 10, 0, 0, 0, 110, 0, 64, 255);
    vtx(&s_v[4], 80, 60, 0, 0, 0, 110, 0, 64, 255);
    vtx(&s_v[5], 130, 10, 0, 0, 0, 110, 0, 64, 255);
    vtx(&s_v[6], 150, 10, 0, 0, 0, 127, 0, 0, 255);
    vtx(&s_v[7], 150, 60, 0, 0, 0, 127, 0, 0, 255);
    vtx(&s_v[8], 200, 10, 0, 0, 0, 127, 0, 0, 255);
    frame_begin(0, 0, 0);
    cc_shade();
    gsSPGeometryMode(0, G_LIGHTING);
    gsSPNumLights(1);
    gsSPLight((uintptr_t)&dir, 1);
    gsSPLight((uintptr_t)&amb, 2);
    gsSPVertex((uintptr_t)s_v, 9, 0);
    gsSP1Triangle(0, 1, 2);
    gsSP1Triangle(3, 4, 5);
    gsSP1Triangle(6, 7, 8);
    /* modelview rotated 90 degrees about y: the +x normal now faces the eye-space light (z) */
    mtx_ident_f(f);
    f[0][0] = 0.0f;
    f[0][2] = 1.0f;
    f[2][0] = -1.0f;
    f[2][2] = 0.0f;
    f[3][0] = 0.0f;
    f[3][1] = 80.0f;
    mtx_f2l(f, &rot);
    gsSPMatrix((uintptr_t)&rot, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH);
    /* a triangle in the y-z plane with a +x normal: after the rotation it faces the eye at x 150.., y 90.. */
    vtx(&s_v[16], 0, 10, -150, 0, 0, 127, 0, 0, 255);
    vtx(&s_v[17], 0, 60, -150, 0, 0, 127, 0, 0, 255);
    vtx(&s_v[18], 0, 10, -200, 0, 0, 127, 0, 0, 255);
    gsSPVertex((uintptr_t)&s_v[16], 3, 0);
    gsSP1Triangle(0, 1, 2);
    gsSPPopMatrixN(1);
    /* texgen: s from the normal's x through the lookat; 32 texels wide: scale (32-1)*64 */
    gsSPLookAtX((uintptr_t)&lax);
    gsSPLookAtY((uintptr_t)&lay);
    gsSPGeometryMode(0, G_TEXTURE_GEN);
    gsSPTexture(31 * 64, 31 * 64, 0, 0, 1);
    load_texture_block((uintptr_t)tex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    cc_decalrgba();
    vtx(&s_v[10], 10, 140, 0, 0, 0, 64, 0, 110, 255);
    vtx(&s_v[11], 10, 190, 0, 0, 0, 64, 0, 110, 255);
    vtx(&s_v[12], 60, 140, 0, 0, 0, 64, 0, 110, 255);
    vtx(&s_v[13], 80, 140, 0, 0, 0, (uint8_t)-64, 0, 110, 255);
    vtx(&s_v[14], 80, 190, 0, 0, 0, (uint8_t)-64, 0, 110, 255);
    vtx(&s_v[15], 130, 140, 0, 0, 0, (uint8_t)-64, 0, 110, 255);
    gsSPVertex((uintptr_t)&s_v[10], 6, 0);
    gsSP2Triangles(0, 1, 2, 3, 4, 5);
    frame_end();
    check("facing", 20, 20, 200, 40, 0, 3);
    check("60 degrees", 90, 20, 200 * 64 * 127 / (127 * 127) + 0, 40, 0, 4);   /* nz/127 ~ cos 60 */
    check("perpendicular", 160, 20, 0, 40, 0, 2);
    check("rotated by modelview", 160, 100, 200, 40, 0, 3);
    check("texgen +x", 20, 150, 0, 255, 0, 2);
    check("texgen -x", 90, 150, 255, 0, 0, 2);
}

/* fog: 2-cycle, FOG_SHADE_A then OPA_SURF2, fog factor in shade alpha */
static void case_fog(void) {
    int i;
    static const int z[3] = {500, -500, -999};
    frame_begin(0, 0, 0);
    gsDPSetCycleType(G_CYC_2CYCLE);
    gsDPSetRenderMode(G_RM_FOG_SHADE_A, RM_OPA_SURF(2));
    gsDPSetCombineLERP(CC_0, CC_0, CC_0, CC_SHADE, AC_0, AC_0, AC_0, AC_SHADE, CC_0, CC_0, CC_0, CC_COMBINED,
                       AC_0, AC_0, AC_0, AC_COMBINED);
    gsDPSetFogColor(0, 0, 255, 255);
    gsSPFogPosition(500, 1000);   /* fog = 256 * (z/w): 0 at ndc 0, full at ndc 1 */
    gsSPGeometryMode(0, G_FOG);
    for (i = 0; i < 3; i++) {
        vtx(&s_v[i * 3 + 0], 10 + i * 60, 10, z[i], 0, 0, 255, 255, 255, 255);
        vtx(&s_v[i * 3 + 1], 10 + i * 60, 60, z[i], 0, 0, 255, 255, 255, 255);
        vtx(&s_v[i * 3 + 2], 60 + i * 60, 10, z[i], 0, 0, 255, 255, 255, 255);
    }
    gsSPVertex((uintptr_t)s_v, 9, 0);
    gsSP2Triangles(0, 1, 2, 3, 4, 5);
    gsSP1Triangle(6, 7, 8);
    frame_end();
    check("near: no fog", 20, 20, 255, 255, 255, 2);
    check("mid: half fog", 80, 20, 127, 127, 255, 3);
    check("far: fog color", 140, 20, 0, 0, 255, 2);
}

/* 2-cycle combiner: cycle 1 lerps two tiles by env alpha, cycle 2 lerps toward env by prim LOD fraction */
static void case_twocycle(void) {
    static uint16_t ta[4 * 4], tb[4 * 4];
    int i;
    for (i = 0; i < 16; i++) {
        ta[i] = rgba16(255, 0, 0, 1);
        tb[i] = rgba16(0, 255, 0, 1);
    }
    frame_begin(0, 0, 0);
    gsDPSetCycleType(G_CYC_2CYCLE);
    gsDPSetRenderMode(RM_OPA_SURF(1), RM_OPA_SURF(2));
    /* tile 0 at TMEM 0, tile 1 at TMEM 64 */
    load_texture_block((uintptr_t)ta, G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 4, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, (uintptr_t)tb);
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 64, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gsDPLoadSync();
    gsDPLoadBlock(G_TX_LOADTILE, 0, 0, 15, calc_dxt(4, 2));
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, 64, 1, 0, G_TX_CLAMP, 0, 0, G_TX_CLAMP, 0, 0);
    gsDPSetTileSize(1, 0, 0, 3 << 2, 3 << 2);
    gsDPSetEnvColor(0, 0, 255, 64);
    gsDPSetPrimColor(0, 128, 0, 0, 0, 255);
    gsDPSetCombineLERP(CC_TEXEL0, CC_TEXEL1, CC_ENV_ALPHA, CC_TEXEL1, AC_0, AC_0, AC_0, AC_1,
                       CC_ENVIRONMENT, CC_COMBINED, CC_PRIM_LOD_FRAC, CC_COMBINED, AC_0, AC_0, AC_0, AC_COMBINED);
    gsSPTextureRectangle(10 << 2, 10 << 2, 30 << 2, 30 << 2, 0, 0, 0, 1 << 8, 1 << 8);
    frame_end();
    {
        /* cycle 1: lerp(t1, t0, 64/255) = (64, 191, 0); cycle 2: lerp(c1, env, 128/255) */
        float a = 64 / 255.0f, l = 128 / 255.0f;
        float r1 = 255 * a, g1 = 255 * (1 - a), b1 = 0;
        check("2-cycle", 20, 20, (int)(r1 + (0 - r1) * l + 0.5f), (int)(g1 + (0 - g1) * l + 0.5f),
              (int)(b1 + (255 - b1) * l + 0.5f), 3);
    }
}

/* the game's dual tile: one CI8 load read by tile 0 as CI8 and by tile 1 as CI4 (shift 15, palette bank 1) */
static void case_dualtile(void) {
    int x, y;
    fill_sources();
    frame_begin(0, 0, 0);
    gsDPSetTextureLUT(G_TT_RGBA16);
    load_tlut_pal256((uintptr_t)s_pal);
    load_texture_block((uintptr_t)s_tex8, G_IM_FMT_CI, G_IM_SIZ_8b, 16, 8, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    gsDPSetTile(G_IM_FMT_CI, G_IM_SIZ_4b, 2, 0, 1, 1, G_TX_CLAMP, 0, 0, G_TX_CLAMP, 0, 15);
    gsDPSetTileSize(1, 0, 0, 31 << 2, 7 << 2);
    cc1(CC_0, CC_0, CC_0, CC_TEXEL1, AC_0, AC_0, AC_0, AC_1);
    gsSPTextureRectangle(10 << 2, 10 << 2, 26 << 2, 18 << 2, 0, 0, 0, 1 << 10, 1 << 10);
    frame_end();
    for (y = 0; y < 8; y++)
        for (x = 0; x < 16; x++) {
            /* tile 1 at s = 2x: byte x of the row, high nibble; palette 16 + nibble */
            int idx = 16 + (s_tex8[y * 16 + x] >> 4);
            uint16_t c = s_pal[idx];
            check("ci4 view of ci8", 10 + x, 10 + y, x5(((c >> 11) & 31) << 3), x5(((c >> 6) & 31) << 3),
                  x5(((c >> 1) & 31) << 3), 1);
        }
}

/* N64-form (big-endian) lists and every address kind (GFX.md "Addresses"):
 *   native list --G_DL 0x06000000 (segment 6 = a host pointer)--> BE list in a host buffer
 *     BE list: matrix, texture image and vertices through segment 6; then G_DL to K0 0x80100000
 *   BE list in RDRAM: matrix at K0, vertices at a physical address, a segment base set from this list (physical),
 *     vertices through that segment
 *   native list again: a texture at a K0 address (a game-built list pointing at a heap texture) */
static uint8_t s_asset[8192];

static void be16(uint8_t *p, uint16_t v) {
    p[0] = v >> 8;
    p[1] = v & 0xFF;
}
static void be32w(uint8_t *p, uint32_t v) {
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}
static void be_vtx(uint8_t *p, int x, int y, int s, int t, uint8_t r, uint8_t g, uint8_t b) {
    memset(p, 0, 16);
    be16(p, (uint16_t)x);
    be16(p + 2, (uint16_t)y);
    be16(p + 4, 0);
    be16(p + 8, (uint16_t)s);
    be16(p + 10, (uint16_t)t);
    p[12] = r;
    p[13] = g;
    p[14] = b;
    p[15] = 255;
}
static void be_mtx(uint8_t *p, float x, float y) {
    Mtx m;
    int i;
    mtx_trans(&m, x, y, 0);
    for (i = 0; i < 16; i++)
        be32w(p + 4 * i, m.m[i]);
}

static void case_belist(void) {
    uint8_t *ram = (uint8_t *)pc_n64_ptr(0x80100000u);   /* RDRAM 0x100000.. */
    int i;
    enum { DL = 0, MTX = 2048, VTX = 2304, TEX = 2560 };                     /* in s_asset */
    enum { R_DL = 0, R_MTX = 0x800, R_VTX = 0xC00, R_TEX = 0x1000, R_SEG7 = 0x2000 };   /* in RDRAM */
    memset(s_asset, 0, sizeof(s_asset));
    memset(ram, 0, 0x4000);
    be_mtx(s_asset + MTX, 100, 0);
    be_vtx(s_asset + VTX + 0, 10, 10, 0, 0, 255, 255, 255);
    be_vtx(s_asset + VTX + 16, 10, 42, 0, 32 << 5, 255, 255, 255);
    be_vtx(s_asset + VTX + 32, 42, 10, 32 << 5, 0, 255, 255, 255);
    be_vtx(s_asset + VTX + 48, 42, 42, 32 << 5, 32 << 5, 255, 255, 255);
    for (i = 0; i < 32 * 32; i++)   /* left half red, right half yellow, BE texels */
        be16(s_asset + TEX + 2 * i, (i & 31) < 16 ? rgba16(255, 0, 0, 1) : rgba16(255, 255, 0, 1));
    be_mtx(ram + R_MTX, 0, 100);
    be_vtx(ram + R_VTX + 0, 10, 10, 0, 0, 0, 0, 0);
    be_vtx(ram + R_VTX + 16, 10, 42, 0, 0, 0, 0, 0);
    be_vtx(ram + R_VTX + 32, 42, 10, 0, 0, 0, 0, 0);
    be_vtx(ram + R_SEG7 + 0, 60, 10, 0, 0, 0, 0, 0);
    be_vtx(ram + R_SEG7 + 16, 60, 42, 0, 0, 0, 0, 0);
    be_vtx(ram + R_SEG7 + 32, 92, 10, 0, 0, 0, 0, 0);
    for (i = 0; i < 8 * 8; i++)
        be16(ram + R_TEX + 2 * i, rgba16(0, 255, 255, 1));
    pc_mem_register_be(s_asset, sizeof(s_asset));
    pc_mem_register_be(ram, 0x4000);

    /* BE list in RDRAM, reached at K0 0x80100000 */
    g_be_mode = 1;
    bp = ram + R_DL;
    gsSPMatrix(0x80100000u + R_MTX, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);   /* K0 */
    gsDPSetPrimColor(0, 0, 0, 255, 0, 255);
    cc_prim();
    gsSPVertex(0x00100000u + R_VTX, 3, 0);                                         /* physical */
    gsSP1Triangle(0, 1, 2);
    gsSPSegment(7, 0x00100000u + R_SEG7);                                          /* base: physical */
    gsDPSetPrimColor(0, 0, 255, 0, 255, 255);
    gsSPVertex(0x07000000u, 3, 0);                                                 /* segmented */
    gsSP1Triangle(0, 1, 2);
    gsSPEndDisplayList();

    /* BE list in a host buffer, reached through segment 6 */
    bp = s_asset + DL;
    gsSPNoOp(0x010003);   /* an unrewritten LOD marker: a no-op */
    gsSPMatrix(0x06000000u + MTX, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH);
    gsSPTexture(0xFFFF, 0xFFFF, 0, 0, 1);
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, 0x06000000u + TEX);
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gsDPLoadSync();
    gsDPLoadBlock(G_TX_LOADTILE, 0, 0, 32 * 32 - 1, calc_dxt(32, 2));
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 0, 0, 0, G_TX_CLAMP, 5, 0, G_TX_CLAMP, 5, 0);
    gsDPSetTileSize(0, 0, 0, 31 << 2, 31 << 2);
    cc_decalrgba();
    gsSPVertex(0x06000000u + VTX, 4, 0);
    gsSPQuadrangle(0, 1, 3, 2);
    gsSPPopMatrixN(1);
    gsSPDisplayList(0x80100000u + R_DL);
    gsSPEndDisplayList();
    g_be_mode = 0;

    frame_begin(0, 0, 0);
    gsSPSegment(6, (uintptr_t)s_asset);   /* a host pointer */
    gsSPDisplayList(0x06000000u + DL);
    load_texture_block(0x80100000u + R_TEX, G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 8, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    cc_decalrgba();
    gsSPTextureRectangle(200 << 2, 50 << 2, 208 << 2, 58 << 2, 0, 0, 0, 1 << 10, 1 << 10);
    gsDPSetPrimColor(0, 0, 0, 0, 255, 255);
    cc_prim();
    gsDPFillRectangle(200, 10, 210, 20);
    frame_end();
    pc_mem_unregister_be(s_asset);
    pc_mem_unregister_be(ram);
    check("seg 6: BE quad left (red)", 115, 25, 255, 0, 0, 2);
    check("seg 6: BE quad right (yellow)", 135, 25, 255, 255, 0, 2);
    check("K0 list, physical vertices", 15, 115, 0, 255, 0, 2);
    check("segment set from a BE list", 65, 115, 255, 0, 255, 2);
    check("nothing at the origin", 25, 25, 0, 0, 0, 0);
    check("native list, K0 texture", 204, 54, 0, 255, 255, 2);
    check("native after BE", 205, 15, 0, 0, 255, 2);
}

static void case_matrix(void) {
    Mtx t1, t2, t3;
    mtx_trans(&t1, 50, 0, 0);
    mtx_trans(&t2, 0, 50, 0);
    mtx_trans(&t3, 0, 100, 0);
    vtx(&s_v[0], 10, 10, 0, 0, 0, 255, 255, 255, 255);
    vtx(&s_v[1], 10, 30, 0, 0, 0, 255, 255, 255, 255);
    vtx(&s_v[2], 30, 10, 0, 0, 0, 255, 255, 255, 255);
    frame_begin(0, 0, 0);
    cc_shade();
    gsSPMatrix((uintptr_t)&t1, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);   /* +50 x */
    gsSPMatrix((uintptr_t)&t2, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);   /* +50 y */
    gsSPMatrix((uintptr_t)&t1, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH); /* +50 x more */
    gsSPVertex((uintptr_t)s_v, 3, 0);
    gsSP1Triangle(0, 1, 2);                      /* at (100, 50) */
    gsSPPopMatrixN(1);
    gsSPVertex((uintptr_t)s_v, 3, 0);
    gsSP1Triangle(0, 1, 2);                      /* at (50, 0) */
    gsSPPopMatrixN(1);
    gsSPMatrix((uintptr_t)&t3, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
    gsSPVertex((uintptr_t)s_v, 3, 0);
    gsSP1Triangle(0, 1, 2);                      /* at (0, 100) */
    gsSPPopMatrixN(4);                           /* popping past the bottom stays at the bottom */
    gsSPVertex((uintptr_t)s_v, 3, 0);
    gsSP1Triangle(0, 1, 2);                      /* still (0, 100): pop keeps the base matrix */
    frame_end();
    check("push mul mul", 115, 65, 255, 255, 255, 0);
    check("after pop", 65, 15, 255, 255, 255, 0);
    check("after second pop", 15, 115, 255, 255, 255, 0);
    check("nothing at origin", 15, 15, 0, 0, 0, 0);
    check("nothing at (50,50)", 65, 65, 0, 0, 0, 0);
}

/* culling: screen-space winding, a mirrored modelview (as the game's), and G_CULLDL */
static void case_cull(void) {
    static Mtx mirror;
    float f[4][4];
    PcGfxWords *save;
    static Vtx box[2][8];
    int i;
    /* visually counterclockwise (front) at x 10.., clockwise (back) at x 60.. */
    vtx(&s_v[0], 10, 10, 0, 0, 0, 255, 0, 0, 255);
    vtx(&s_v[1], 10, 40, 0, 0, 0, 255, 0, 0, 255);
    vtx(&s_v[2], 40, 10, 0, 0, 0, 255, 0, 0, 255);
    vtx(&s_v[3], 60, 10, 0, 0, 0, 0, 255, 0, 255);
    vtx(&s_v[4], 90, 10, 0, 0, 0, 0, 255, 0, 255);
    vtx(&s_v[5], 60, 40, 0, 0, 0, 0, 255, 0, 255);
    frame_begin(0, 0, 0);
    cc_shade();
    gsSPVertex((uintptr_t)s_v, 6, 0);
    gsSPGeometryMode(0, G_CULL_BACK);
    gsSP2Triangles(0, 1, 2, 3, 4, 5);
    /* same in row 2 with G_CULL_FRONT */
    gsSPMatrix((uintptr_t)&s_ident, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    {
        static Mtx down;
        mtx_trans(&down, 0, 50, 0);
        gsSPMatrix((uintptr_t)&down, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    }
    gsSPVertex((uintptr_t)s_v, 6, 0);
    gsSPGeometryMode(G_CULL_BACK, G_CULL_FRONT);
    gsSP2Triangles(0, 1, 2, 3, 4, 5);
    /* row 3: x mirrored by the modelview (x -> 200 - x): windings swap, G_CULL_BACK keeps the green one */
    mtx_ident_f(f);
    f[0][0] = -1.0f;
    f[3][0] = 200.0f;
    f[3][1] = 100.0f;
    mtx_f2l(f, &mirror);
    gsSPMatrix((uintptr_t)&mirror, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gsSPVertex((uintptr_t)s_v, 6, 0);
    gsSPGeometryMode(G_CULL_FRONT, G_CULL_BACK);
    gsSP2Triangles(0, 1, 2, 3, 4, 5);
    gsSPGeometryMode(G_CULL_BOTH, 0);
    gsSPMatrix((uintptr_t)&s_ident, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    /* G_CULLDL: a sub-list whose bounding box is off screen ends early; one on screen draws */
    for (i = 0; i < 2; i++) {
        int bx = i == 0 ? -500 : 200, j;
        PcGfxWords *sub = s_sub + i * 64;
        for (j = 0; j < 8; j++)
            vtx(&box[i][j], bx + (j & 1) * 40, 150 + ((j >> 1) & 1) * 40, (j & 4) ? 10 : -10, 0, 0, 0, 0, 0, 255);
        save = gp;
        gp = sub;
        gsSPVertex((uintptr_t)box[i], 8, 0);
        gsSPCullDisplayList(0, 7);
        gsSPVertex((uintptr_t)&s_v[6 + i * 3], 3, 0);
        gsSP1Triangle(0, 1, 2);
        gsSPEndDisplayList();
        gp = save;
        vtx(&s_v[6 + i * 3 + 0], 200 + i * 50, 150, 0, 0, 0, 255, 255, 0, 255);
        vtx(&s_v[6 + i * 3 + 1], 200 + i * 50, 180, 0, 0, 0, 255, 255, 0, 255);
        vtx(&s_v[6 + i * 3 + 2], 230 + i * 50, 150, 0, 0, 0, 255, 255, 0, 255);
        gsSPDisplayList((uintptr_t)sub);
    }
    frame_end();
    check("cull back: front kept", 15, 15, 255, 0, 0, 0);
    check("cull back: back culled", 65, 15, 0, 0, 0, 0);
    check("cull front: front culled", 15, 65, 0, 0, 0, 0);
    check("cull front: back kept", 65, 65, 0, 255, 0, 0);
    check("mirrored: red now back", 185, 115, 0, 0, 0, 0);
    check("mirrored: green now front", 135, 115, 0, 255, 0, 0);
    check("culldl off screen", 205, 155, 0, 0, 0, 0);
    check("culldl on screen", 255, 155, 255, 255, 0, 0);
}

/* G_BRANCH_Z on a vertex's screen depth. It branches (no return), so each test runs in a called list, as the
 * game's model lists do: the target's ENDDL returns to the caller. */
static void case_branchz(void) {
    PcGfxWords *save;
    int i;
    static Vtx ref[2];
    vtx(&ref[0], 0, 0, 500, 0, 0, 0, 0, 0, 255);    /* near: screen z ~ 255 */
    vtx(&ref[1], 0, 0, -500, 0, 0, 0, 0, 0, 255);   /* far: screen z ~ 767 */
    frame_begin(0, 0, 0);
    cc_shade();
    for (i = 0; i < 2; i++) {
        PcGfxWords *target = s_sub + i * 128, *wrap = s_sub + i * 128 + 64;
        vtx(&s_v[i * 3 + 0], 10 + i * 100, 10, 0, 0, 0, 0, 255, 0, 255);
        vtx(&s_v[i * 3 + 1], 10 + i * 100, 40, 0, 0, 0, 0, 255, 0, 255);
        vtx(&s_v[i * 3 + 2], 40 + i * 100, 10, 0, 0, 0, 0, 255, 0, 255);
        vtx(&s_v[10 + i * 3 + 0], 10 + i * 100, 10, 0, 0, 0, 255, 0, 0, 255);
        vtx(&s_v[10 + i * 3 + 1], 10 + i * 100, 40, 0, 0, 0, 255, 0, 0, 255);
        vtx(&s_v[10 + i * 3 + 2], 40 + i * 100, 10, 0, 0, 0, 255, 0, 0, 255);
        save = gp;
        gp = target;   /* the branch target: green triangle */
        gsSPVertex((uintptr_t)&s_v[i * 3], 3, 0);
        gsSP1Triangle(0, 1, 2);
        gsSPEndDisplayList();
        gp = wrap;     /* branch when the vertex is nearer than mid-depth, else a red triangle */
        gsSPVertex((uintptr_t)&ref[i], 1, 8);
        gsSPBranchLessZraw((uintptr_t)target, 8, 511u << 16);
        gsSPVertex((uintptr_t)&s_v[10 + i * 3], 3, 0);
        gsSP1Triangle(0, 1, 2);
        gsSPEndDisplayList();
        gp = save;
        gsSPDisplayList((uintptr_t)wrap);
    }
    frame_end();
    check("near vertex: branch taken", 15, 15, 0, 255, 0, 0);
    check("far vertex: fall through", 115, 15, 255, 0, 0, 0);
}

/* NoN microcode: a floor running behind the camera clips at w > 0, nothing wraps above the horizon */
static void case_nearclip(void) {
    static Mtx persp;
    mtx_persp(&persp, 60.0f, 4.0f / 3.0f, 10.0f, 1000.0f);
    vtx(&s_v[0], -500, -50, 200, 0, 0, 0, 255, 255, 255);    /* behind the eye (z > 0 is behind) */
    vtx(&s_v[1], 500, -50, 200, 0, 0, 0, 255, 255, 255);
    vtx(&s_v[2], 0, -50, -900, 0, 0, 0, 255, 255, 255);
    vtx(&s_v[3], -1, -1, -5, 0, 0, 255, 0, 255, 255);       /* closer than the near plane: still drawn */
    vtx(&s_v[4], 1, -1, -5, 0, 0, 255, 0, 255, 255);
    vtx(&s_v[5], 0, 1, -5, 0, 0, 255, 0, 255, 255);
    frame_begin(0, 0, 0);
    gsSPMatrix((uintptr_t)&persp, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gsDPSetRenderMode(RM_ZB_OPA_SURF(1), RM_ZB_OPA_SURF(2));
    gsSPGeometryMode(0, G_ZBUFFER);
    cc_shade();
    gsSPVertex((uintptr_t)s_v, 3, 0);
    gsSP1Triangle(0, 1, 2);
    gsSPVertex((uintptr_t)&s_v[3], 3, 0);
    gsSP1Triangle(0, 1, 2);
    frame_end();
    check("floor below", 160, 235, 0, 255, 255, 0);
    check("sky above", 160, 5, 0, 0, 0, 0);
    check("sky corner", 5, 5, 0, 0, 0, 0);
    check("inside near plane", 160, 120, 255, 0, 255, 0);
}

/* translucency, alpha compare and texture edge */
static void case_xlu(void) {
    static uint16_t t[4 * 4];
    int i;
    for (i = 0; i < 16; i++)
        t[i] = (i & 1) ? rgba16(255, 255, 255, 1) : 0;   /* alternate transparent columns */
    frame_begin(255, 0, 0);
    gsDPSetRenderMode(RM_XLU_SURF(1), RM_XLU_SURF(2));
    gsDPSetPrimColor(0, 0, 0, 0, 255, 128);
    cc_prim();
    gsDPFillRectangle(10, 10, 30, 30);
    /* the game's fog-on rectangles: XLU_SURF in both cycles of 2-cycle mode, so memory blends twice:
     * in*a*a + mem*(1-a)*(1+a) */
    gsDPSetCycleType(G_CYC_2CYCLE);
    gsDPSetRenderMode(RM_XLU_SURF(1), RM_XLU_SURF(2));
    gsDPFillRectangle(10, 40, 30, 60);
    gsDPSetCycleType(G_CYC_1CYCLE);
    gsDPSetRenderMode(RM_OPA_SURF(1), RM_OPA_SURF(2));
    gsDPSetAlphaCompare(G_AC_THRESHOLD);
    gsDPSetBlendColor(0, 0, 0, 100);
    gsDPSetPrimColor(0, 0, 0, 255, 0, 99);    /* below threshold: dropped */
    gsDPFillRectangle(40, 10, 50, 20);
    gsDPSetPrimColor(0, 0, 0, 255, 0, 100);   /* at threshold: drawn */
    gsDPFillRectangle(60, 10, 70, 20);
    gsDPSetAlphaCompare(G_AC_NONE);
    gsDPSetRenderMode(RM_AA_ZB_TEX_EDGE(1), RM_AA_ZB_TEX_EDGE(2));
    load_texture_block((uintptr_t)t, G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 4, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    cc_decalrgba();
    gsSPTextureRectangle(80 << 2, 10 << 2, 84 << 2, 14 << 2, 0, 0, 0, 1 << 10, 1 << 10);
    frame_end();
    check("xlu 50%", 20, 20, 127, 0, 128, 3);
    check("xlu twice (2-cycle)", 20, 50, 191, 0, 64, 3);
    check("alpha compare below", 45, 15, 255, 0, 0, 0);
    check("alpha compare at", 65, 15, 0, 255, 0, 0);
    check("tex edge transparent", 80, 10, 255, 0, 0, 0);
    check("tex edge opaque", 81, 10, 255, 255, 255, 0);
}

/* ---- widescreen (GFX.md "Widescreen"). At 16:9 the 320 x 240 frame's target is 426 x 240: 53 pixels on each
 * side. A perspective view across the frame spans the target, at the same scale; 2D stays 4:3 in the middle;
 * full-width fills and rectangles span the target. */

#define WIDE_W 426
#define WIDE_OX 53

static Mtx s_frustum;   /* guPerspective at 90 degrees, aspect 1: x/w = x/-z, y/w = y/-z */

static void quad(Vtx *v, int x0, int y0, int x1, int y1, int z, uint8_t r, uint8_t g, uint8_t b) {
    vtx(&v[0], x0, y0, z, 0, 0, r, g, b, 255);
    vtx(&v[1], x0, y1, z, 0, 0, r, g, b, 255);
    vtx(&v[2], x1, y0, z, 0, 0, r, g, b, 255);
    vtx(&v[3], x1, y1, z, 0, 0, r, g, b, 255);
}

static uint16_t s_wtex[8 * 8], s_wbar[8 * 2];

static int bar_r(int c) { return x5(c * 36); }
static int bar_g(int c) { return x5(255 - c * 36); }

/* A perspective view's viewport, centre and half size in N64 pixels */
static void viewport(Vp *vp, int cx, int cy, int hw, int hh) {
    vp->vscale[0] = (int16_t)(hw * 4);
    vp->vscale[1] = (int16_t)(hh * 4);
    vp->vscale[2] = 511;
    vp->vscale[3] = 0;
    vp->vtrans[0] = (int16_t)(cx * 4);
    vp->vtrans[1] = (int16_t)(cy * 4);
    vp->vtrans[2] = 511;
    vp->vtrans[3] = 0;
}

/* Everything widescreen moves, in one frame on blue: a perspective view across the frame at rows 60-180, with
 * strips at x/w -1.2..-1 (red), 1..1.2 (green), 1.25..1.5 (yellow) and a square at -0.25..0.25 (white); copy-mode
 * texrects at x 10 and at the frame's left side (rows 10-17); an ortho triangle at the left side (rows 190-198); a
 * full-width 1-cycle fill (rows 200-209); a full-width texrect, 1024/15 pixels a texel (rows 220-229). */
static void wide_scene(void) {
    static Vtx v[20];
    int i;
    for (i = 0; i < 64; i++)
        s_wtex[i] = rgba16((i & 7) * 32, (i >> 3) * 32, 128, 1);
    for (i = 0; i < 16; i++)
        s_wbar[i] = rgba16(bar_r(i & 7), bar_g(i & 7), 0, 1);
    frame_begin(0, 0, 255);
    cc_shade();
    vtx(&v[16], 0, 190, 0, 0, 0, 255, 0, 255, 255);
    vtx(&v[17], 0, 199, 0, 0, 0, 255, 0, 255, 255);
    vtx(&v[18], 40, 190, 0, 0, 0, 255, 0, 255, 255);
    gsSPVertex((uintptr_t)&v[16], 3, 0);
    gsSP1Triangle(0, 1, 2);
    gsSPMatrix((uintptr_t)&s_frustum, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    quad(&v[0], -120, -50, -100, 50, -100, 255, 0, 0);
    quad(&v[4], 100, -50, 120, 50, -100, 0, 255, 0);
    quad(&v[8], 125, -50, 150, 50, -100, 255, 255, 0);
    quad(&v[12], -25, -50, 25, 50, -100, 255, 255, 255);
    gsSPVertex((uintptr_t)v, 16, 0);
    for (i = 0; i < 16; i += 4)
        gsSP2Triangles(i, i + 1, i + 2, i + 2, i + 1, i + 3);
    gsSPMatrix((uintptr_t)&s_ortho, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gsDPSetPrimColor(0, 0, 0, 255, 255, 255);
    cc_prim();
    gsDPFillRectangle(0, 200, FB_W, 210);
    load_texture_block((uintptr_t)s_wbar, G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 2, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    cc_decalrgba();
    gsSPTextureRectangle(0, 220 << 2, FB_W << 2, 230 << 2, 0, 0, 0, 15, 0);
    load_texture_block((uintptr_t)s_wtex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 8, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    gsDPSetCycleType(G_CYC_COPY);
    gsDPSetRenderMode(0, 0);
    gsSPTextureRectangle(10 << 2, 10 << 2, 17 << 2, 17 << 2, 0, 0, 0, 4 << 10, 1 << 10);
    gsSPTextureRectangle(0, 10 << 2, 7 << 2, 17 << 2, 0, 0, 0, 4 << 10, 1 << 10);
    frame_end();
}

/* Split screen: four quadrant views (x 0-160 and 160-320), each filled with its color and a white square at x/w
 * -0.25..0.25, then a copy-mode texrect at x 156-163 in the top-left view's scissor. */
static void wide_split(void) {
    static Vp vp[4];
    static Vtx v[4][8];
    static const uint8_t col[4][3] = {{255, 0, 0}, {0, 255, 0}, {255, 255, 0}, {255, 0, 255}};
    int q;
    frame_begin(0, 0, 255);
    cc_shade();
    gsSPMatrix((uintptr_t)&s_frustum, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    for (q = 0; q < 4; q++) {
        int x = (q & 1) * 160, y = (q >> 1) * 120;
        viewport(&vp[q], x + 80, y + 60, 80, 60);
        quad(&v[q][0], -300, -300, 300, 300, -100, col[q][0], col[q][1], col[q][2]);
        quad(&v[q][4], -25, -25, 25, 25, -100, 255, 255, 255);
        gsSPViewport((uintptr_t)&vp[q]);
        gsDPSetScissor(x, y, x + 160, y + 120);
        gsSPVertex((uintptr_t)v[q], 8, 0);
        gsSP2Triangles(0, 1, 2, 2, 1, 3);
        gsSP2Triangles(4, 5, 6, 6, 5, 7);
    }
    gsDPSetScissor(0, 0, 160, 120);
    load_texture_block((uintptr_t)s_wtex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 8, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    gsDPSetCycleType(G_CYC_COPY);
    gsDPSetRenderMode(0, 0);
    gsSPTextureRectangle(156 << 2, 2 << 2, 163 << 2, 9 << 2, 0, 0, 0, 4 << 10, 1 << 10);
    frame_end();
}

/* The game's own split-screen panes (docs/port/GFX.md "Widescreen"): 2 players, x 2-242 (rows 0-110); 4 players,
 * x 1-161 and 158-318 with the scissor split at 160 (rows 120-232). */
static void wide_panes(void) {
    static Vp vp[7];
    static Vtx v[7][8];
    static const uint8_t col[7][3] = {{0}, {0}, {0}, {0}, {0, 255, 255}, {255, 128, 0}, {128, 0, 255}};
    int q;
    frame_begin(0, 0, 255);
    cc_shade();
    gsSPMatrix((uintptr_t)&s_frustum, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    viewport(&vp[4], 122, 55, 120, 55);
    viewport(&vp[5], 81, 176, 80, 56);
    viewport(&vp[6], 238, 176, 80, 56);
    for (q = 4; q < 7; q++) {
        static const int sc[7][4] = {{0}, {0}, {0}, {0}, {2, 0, 242, 110}, {0, 120, 160, 232}, {160, 120, 320, 232}};
        quad(&v[q][0], -300, -300, 300, 300, -100, col[q][0], col[q][1], col[q][2]);
        gsSPViewport((uintptr_t)&vp[q]);
        gsDPSetScissor(sc[q][0], sc[q][1], sc[q][2], sc[q][3]);
        gsSPVertex((uintptr_t)v[q], 4, 0);
        gsSP2Triangles(0, 1, 2, 2, 1, 3);
    }
    frame_end();
}

static void case_widescreen(void) {
    static const struct { const char *text; float aspect; int ok; } parse[] = {
        {"off", 0.0f, 1}, {"auto", -1.0f, 1}, {"16:9", 16.0f / 9, 1}, {"16:10", 1.6f, 1}, {"21:9", 21.0f / 9, 1},
        {"2.39", 2.39f, 1}, {"4:3", 0.0f, 1}, {"5:4", 0.0f, 0}, {"9:16", 0.0f, 0}, {"16:9x", 0.0f, 0},
        {"wide", 0.0f, 0}, {"5", 0.0f, 0},
    };
    int i, x, y, ok, scale;
    float f[4][4], env_aspect = pc_gfx_parse_aspect(getenv("RUSH_GFX_WIDESCREEN"), &ok);
    uint8_t *off;
    size_t n;

    for (i = 0; i < (int)(sizeof(parse) / sizeof(parse[0])); i++) {
        float a = pc_gfx_parse_aspect(parse[i].text, &ok);
        if (ok != parse[i].ok || fabsf(a - parse[i].aspect) > 1e-5f) {
            printf("  FAIL widescreen parse '%s': %g ok %d\n", parse[i].text, a, ok);
            s_fail++;
        }
    }
    memset(f, 0, sizeof(f));
    f[0][0] = f[1][1] = 1.0f;
    f[2][2] = (10.0f + 1000.0f) / (10.0f - 1000.0f);
    f[2][3] = -1.0f;
    f[3][2] = 2.0f * 10.0f * 1000.0f / (10.0f - 1000.0f);
    mtx_f2l(f, &s_frustum);

    /* off: nothing past x/w = +-1 is drawn; the square is at 120..200 */
    pc_gfx_set_widescreen(0.0f);
    wide_scene();
    if (s_px_w != FB_W * (s_px_h / FB_H)) {
        printf("  FAIL widescreen off: the target is %dx%d\n", s_px_w, s_px_h);
        s_fail++;
        pc_gfx_set_widescreen(env_aspect);
        return;
    }
    check("off: square", 125, 120, 255, 255, 255, 0);
    check("off: left of the square", 115, 120, 0, 0, 255, 0);
    check("off: no strip at the left side", 1, 120, 0, 0, 255, 0);
    check("off: no strip at the right side", 318, 120, 0, 0, 255, 0);
    check("off: full-width texrect, texel 4 from 274", 280, 225, bar_r(4), bar_g(4), 0, 1);
    n = (size_t)s_px_w * s_px_h * 4;
    off = (uint8_t *)malloc(n);
    memcpy(off, s_px, n);

    pc_gfx_set_widescreen(16.0f / 9.0f);
    wide_scene();
    scale = s_px_h / FB_H;
    if (s_px_w != WIDE_W * scale || s_px_h != FB_H * scale) {
        printf("  FAIL widescreen 16:9: the target is %dx%d, not %dx%d\n", s_px_w, s_px_h, WIDE_W * scale,
               FB_H * scale);
        s_fail++;
        free(off);
        pc_gfx_set_widescreen(env_aspect);
        return;
    }
    check_at("clear: left side", 0, 5, 0, 0, 255, 0);
    check_at("clear: right side", WIDE_W - 1, 5, 0, 0, 255, 0);
    /* the view: the frame's middle (160) is the target's (213), 160 pixels per unit of x/w as at 4:3 */
    check_at("strip left of the 4:3 frustum", 37, 120, 255, 0, 0, 0);
    check_at("strip right of the 4:3 frustum", 389, 120, 0, 255, 0, 0);
    check_at("strip across the wide frustum's side, at the last column", WIDE_W - 1, 120, 255, 255, 0, 0);
    check_at("before that strip", 410, 120, 0, 0, 255, 0);
    check_at("left of the left strip", 15, 120, 0, 0, 255, 0);
    check_at("square: left edge inside", 175, 120, 255, 255, 255, 0);
    check_at("square: right edge inside", 251, 120, 255, 255, 255, 0);
    check_at("square: not wider on the left", 170, 120, 0, 0, 255, 0);
    check_at("square: not wider on the right", 256, 120, 0, 0, 255, 0);
    /* 2D in the middle, texel for texel */
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++) {
            check_at("texrect", WIDE_OX + 10 + x, 10 + y, x5(x * 32), x5(y * 32), x5(128), 1);
            check_at("texrect at the frame's left side", WIDE_OX + x, 10 + y, x5(x * 32), x5(y * 32), x5(128), 1);
        }
    check_at("texrect: the left bar is clear", WIDE_OX - 1, 12, 0, 0, 255, 0);
    check_at("texrect: not wider", WIDE_OX + 8, 12, 0, 0, 255, 0);
    check_at("ortho triangle at the frame's left side", WIDE_OX + 2, 191, 255, 0, 255, 0);
    check_at("ortho triangle: the left bar is clear", WIDE_OX - 2, 191, 0, 0, 255, 0);
    /* full width */
    check_at("full-width fill: left", 0, 205, 0, 255, 255, 0);
    check_at("full-width fill: right", WIDE_W - 1, 205, 0, 255, 255, 0);
    check_at("full-width texrect: first texel at the left", 0, 225, bar_r(0), bar_g(0), 0, 1);
    /* stretched by 426/320: 90.9 pixels a texel */
    check_at("full-width texrect: texel 0 up to 89", 88, 225, bar_r(0), bar_g(0), 0, 1);
    check_at("full-width texrect: texel 1 from 90", 91, 225, bar_r(1), bar_g(1), 0, 1);
    check_at("full-width texrect: texel 3 before 363", 360, 225, bar_r(3), bar_g(3), 0, 1);
    check_at("full-width texrect: texel 4 at the right", WIDE_W - 1, 225, bar_r(4), bar_g(4), 0, 1);

    /* split screen: the quadrant views at x 0-160 and 160-320 become 0-213 and 213-426 */
    wide_split();
    check_at("split: top-left at the left side", 0, 60, 255, 0, 0, 0);
    check_at("split: top-left to 212", 212, 60, 255, 0, 0, 0);
    check_at("split: top-right from 213", 213, 60, 0, 255, 0, 0);
    check_at("split: top-right at the right side", WIDE_W - 1, 60, 0, 255, 0, 0);
    check_at("split: bottom-left at the left side", 0, 180, 255, 255, 0, 0);
    check_at("split: bottom-left to 212", 212, 180, 255, 255, 0, 0);
    check_at("split: bottom-right from 213", 213, 180, 255, 0, 255, 0);
    check_at("split: bottom-right at the right side", WIDE_W - 1, 180, 255, 0, 255, 0);
    /* each square: 40 pixels as at 4:3, in the middle of its widened view (106.5, 319.5) */
    check_at("split: top-left square", 90, 60, 255, 255, 255, 0);
    check_at("split: top-left square, right", 124, 60, 255, 255, 255, 0);
    check_at("split: top-left square, not wider", 84, 60, 255, 0, 0, 0);
    check_at("split: top-left square, not wider right", 129, 60, 255, 0, 0, 0);
    check_at("split: top-right square", 303, 60, 255, 255, 255, 0);
    check_at("split: top-right square, not wider", 297, 60, 0, 255, 0, 0);
    check_at("split: bottom-right square", 336, 180, 255, 255, 255, 0);
    check_at("split: bottom-right square, not wider", 342, 180, 255, 0, 255, 0);
    /* 2D in a view keeps the N64's scissor, moved with the frame: cut at the frame's 160 */
    check_at("split: texrect in the top-left view", WIDE_OX + 156, 2, x5(0), x5(0), x5(128), 1);
    check_at("split: texrect up to the view's side", WIDE_OX + 159, 2, x5(3 * 32), x5(0), x5(128), 1);
    check_at("split: texrect cut at the view's side", WIDE_OX + 160, 2, 0, 255, 0, 0);
    /* the game's panes: their 1-2 pixel borders stay, the sides at the frame's widen */
    wide_panes();
    check_at("2 players: the border at the left", 1, 55, 0, 0, 255, 0);
    check_at("2 players: the pane from 2", 2, 55, 0, 255, 255, 0);
    check_at("2 players: the pane to 242 + 53", 241 + WIDE_OX, 55, 0, 255, 255, 0);
    check_at("2 players: the side panel stays in the frame", 243 + WIDE_OX, 55, 0, 0, 255, 0);
    check_at("4 players: left view at the left side", 0, 176, 255, 128, 0, 0);
    check_at("4 players: left view to 160 + 53", 159 + WIDE_OX, 176, 255, 128, 0, 0);
    check_at("4 players: right view from 160 + 53", 160 + WIDE_OX, 176, 128, 0, 255, 0);
    check_at("4 players: right view at the right side", WIDE_W - 1, 176, 128, 0, 255, 0);

    /* off again: pixel for pixel the first frame */
    pc_gfx_set_widescreen(0.0f);
    wide_scene();
    if ((size_t)s_px_w * s_px_h * 4 != n || memcmp(off, s_px, n) != 0) {
        printf("  FAIL widescreen off after on: the frame differs\n");
        s_fail++;
    }
    free(off);
    pc_gfx_set_widescreen(env_aspect);
}

/* the auto settings follow the window (here the headless size, pc_gfx_set_window_size): widescreen takes its aspect,
 * and the scale the largest whole multiple of the frame that it holds */
static void case_autosize(void) {
    static uint16_t t[8 * 8];
    const char *env_scale = getenv("RUSH_GFX_SCALE");
    int i, x, y, ok, scale = env_scale && !strcmp(env_scale, "auto") ? 0 : env_scale ? atoi(env_scale) : 1;
    float env_aspect = pc_gfx_parse_aspect(getenv("RUSH_GFX_WIDESCREEN"), &ok);
    static const struct { int win_w, win_h; float aspect; int scale, w, s; } steps[] = {
        {1280, 720, -1.0f, 0, 426, 3},    /* 16:9: 426 x 240, 3 x 3 fits 1280 x 720 */
        {1920, 1080, -1.0f, 0, 426, 4},   /* 4 x 426 = 1704 */
        {1024, 768, -1.0f, 0, 320, 3},    /* 4:3: widescreen off */
        {1280, 720, -1.0f, 2, 426, 2},    /* a fixed scale stays */
        {1280, 720, 21.0f / 9, 0, 560, 2},   /* a fixed aspect: 560 x 240, 2 x 560 fits 1280 */
        {640, 480, 0.0f, 0, 320, 2},
    };
    if (s_window) {   /* the window has its own size */
        printf("  (autosize: headless only)\n");
        return;
    }
    for (i = 0; i < 64; i++)
        t[i] = rgba16((i & 7) * 32, (i >> 3) * 32, 128, 1);
    for (i = 0; i < (int)(sizeof(steps) / sizeof(steps[0])); i++) {
        pc_gfx_set_window_size(steps[i].win_w, steps[i].win_h);
        pc_gfx_set_widescreen(steps[i].aspect);
        pc_gfx_set_scale(steps[i].scale);
        frame_begin(0, 0, 0);
        load_texture_block((uintptr_t)t, G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 8, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
        gsDPSetCycleType(G_CYC_COPY);
        gsDPSetRenderMode(0, 0);
        gsSPTextureRectangle(10 << 2, 10 << 2, 17 << 2, 17 << 2, 0, 0, 0, 4 << 10, 1 << 10);
        frame_end();
        if (s_px_w != steps[i].w * steps[i].s || s_px_h != FB_H * steps[i].s) {
            printf("  FAIL autosize %dx%d: the target is %dx%d, not %dx%d\n", steps[i].win_w, steps[i].win_h, s_px_w,
                   s_px_h, steps[i].w * steps[i].s, FB_H * steps[i].s);
            s_fail++;
            continue;
        }
        for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++)
                check("texrect", 10 + x, 10 + y, x5(x * 32), x5(y * 32), x5(128), 1);
    }
    pc_gfx_set_window_size(640, 480);
    pc_gfx_set_widescreen(env_aspect);
    pc_gfx_set_scale(scale);
}

/* the screenshot is a PNG of the presented frame */
static void case_screenshot(void) {
    char path[512];
    unsigned char hdr[24];
    FILE *f;
    frame_begin(0, 128, 0);
    frame_end();
    snprintf(path, sizeof(path), "%s/screenshot_test.png",
             s_png_dir ? s_png_dir : getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
    if (pc_gfx_save_screenshot(path) != 0 || (f = fopen(path, "rb")) == NULL) {
        printf("  FAIL screenshot: not written\n");
        s_fail++;
        return;
    }
    if (fread(hdr, 1, 24, f) != 24 || memcmp(hdr, "\x89PNG\r\n\x1a\n", 8) != 0 || memcmp(hdr + 12, "IHDR", 4) ||
        hdr[16] != 0 || hdr[17] != 0 || (hdr[18] << 8 | hdr[19]) != s_px_w || (hdr[22] << 8 | hdr[23]) != s_px_h) {
        printf("  FAIL screenshot: bad PNG header\n");
        s_fail++;
    }
    fclose(f);
}

/* ---------------------------------------------------------------- main */

typedef struct {
    const char *name;
    void (*fn)(void);
} Case;

static const Case s_cases[] = {
    {"fill", case_fill},         {"shade", case_shade},       {"zbuf", case_zbuf},
    {"textures", case_textures}, {"texwrap", case_texwrap},   {"texrect", case_texrect},
    {"lit", case_lit},           {"fog", case_fog},           {"twocycle", case_twocycle},
    {"dualtile", case_dualtile}, {"belist", case_belist},     {"matrix", case_matrix},
    {"cull", case_cull},         {"branchz", case_branchz},   {"nearclip", case_nearclip},
    {"xlu", case_xlu},           {"widescreen", case_widescreen}, {"autosize", case_autosize},
    {"screenshot", case_screenshot},
};

int main(int argc, char **argv) {
    const char *which = argc > 1 ? argv[1] : "all";
    int i, ran = 0, failed = 0, window = 0;
    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--png") == 0 && i + 1 < argc)
            s_png_dir = argv[++i];
        else if (strcmp(argv[i], "--window") == 0)
            window = 1;
    }
    s_rdram = (uint8_t *)calloc(1, PC_RDRAM_SIZE);
    mtx_ortho(&s_ortho, 0, FB_W, FB_H, 0, -1000, 1000);
    {
        float f[4][4];
        mtx_ident_f(f);
        mtx_f2l(f, &s_ident);
    }
    s_window = window;
    if (pc_gfx_init(!window, 640, 480) != 0) {
        printf("FAIL: pc_gfx_init\n");
        return 2;
    }
    for (i = 0; i < (int)(sizeof(s_cases) / sizeof(s_cases[0])); i++) {
        int before = s_fail;
        if (strcmp(which, "all") != 0 && strcmp(which, s_cases[i].name) != 0)
            continue;
        s_case = s_cases[i].name;
        s_cases[i].fn();
        if (pc_gfx_poll_events())
            printf("  (quit requested)\n");
        ran++;
        if (s_fail != before)
            failed++;
        printf("%s %s\n", s_fail != before ? "FAIL" : "ok  ", s_case);
    }
    pc_gfx_shutdown();
    free(s_px);
    free(s_rdram);
    if (ran == 0) {
        printf("no case named %s\n", which);
        return 2;
    }
    printf("%d/%d cases passed\n", ran - failed, ran);
    return failed ? 1 : 0;
}
