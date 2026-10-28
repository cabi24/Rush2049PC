/* port/gfx/tests/gfx_test.c: renders display lists headless and checks pixels.
 *
 *   gfx_test <case>|all [--png DIR] [--window [--no-window-readback]]
 *
 * --window initializes with a window (SDL2 builds; SDL_VIDEODRIVER=offscreen works without a display) and
 * presents to it; the checks still read the render targets, and the window's pixels are compared with the frame.
 * --no-window-readback leaves that comparison out: it reads the back buffer after the swap, which GL leaves
 * undefined (SDL's offscreen driver keeps it; WGL gives an earlier frame).
 * Each case builds its lists with test_gbi.h, runs them through pc_gfx_run_dl, presents, and compares sample
 * points against values worked out from the N64 semantics. Exit status 0 = pass.
 * RUSH_GFX_TEST_WINDOW=WxH: the headless window's size (640x480), which the native and auto scales follow. */
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
int pc_gfx_framebuffer_dims(const void *cimg, int *n64_w, int *n64_h, int *px_w, int *px_h);
int pcgfx_pixel_of(int i, int n, int px);
int pcgfx_row_of(int i, int n, int px);
float pc_gfx_parse_scale(const char *text, int *ok);
void pc_gfx_set_scale_value(float scale);

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
static int s_px_w, s_px_h, s_fail, s_window, s_window_readback = 1;
static int s_n64_w, s_n64_h;          /* the target in N64 pixels (s_px_w x s_px_h internal ones) */
static int s_win_w = 640, s_win_h = 480;   /* the headless window (RUSH_GFX_TEST_WINDOW) */
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
    {
        int pw, ph;
        if (pc_gfx_framebuffer_dims(s_fb, &s_n64_w, &s_n64_h, &pw, &ph) != 0)
            s_n64_w = s_n64_h = 1;
    }
    if (s_window && s_window_readback && s_px_w == FB_W) {   /* 640x480 window, 320x240 frame: at 2x */
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

/* Target pixel (x, y) in N64 pixels: the internal pixel at the middle of its block at the internal scale
 * (RUSH_GFX_SCALE; a fraction too: the pixel its centre is in). */
static const uint8_t *tpx(int x, int y) {
    int ix = (int)(((long long)2 * x + 1) * s_px_w / (2LL * s_n64_w));
    int iy = (int)(((long long)2 * y + 1) * s_px_h / (2LL * s_n64_h));
    return s_px + ((size_t)iy * s_px_w + ix) * 4;
}

/* The 4:3 frame's left side in the target: 0, or in widescreen (RUSH_GFX_WIDESCREEN) the pixels added on each side. */
static int frame_x0(void) {
    return (s_n64_w - FB_W) / 2;
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
    gsSPTextureRectangle(50 << 2, 10 << 2, 57 << 2, 17 << 2, 0, 7 << 5, 0, (uint16_t)-(4 << 10), 1 << 10);
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

/* Texture LOD (G_TL_LOD), as the game draws its world: four mip levels in tiles 0-3 (32, 16, 8 and 4 texels, each
 * shifted by its level), 2-cycle with (TEXEL1 - TEXEL0) x LOD_FRACTION + TEXEL0, point sampled. Screen-aligned
 * quads step r texels of the first level a pixel. Below 1 the first level; from 2^l the level l, toward l + 1 by
 * r / 2^l - 1; from the last level's on, the last level. The steps are in N64 pixels at every internal scale.
 * Without G_TL_LOD, TEXEL0 is tile 0 and the fraction 0. */
static void case_lod(void) {
    static uint16_t lv[4][32 * 32];
    static Vtx v[8][4];
    static const float r[8] = {0.5f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 16.0f, 4.0f};
    static const int col[4][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0}};
    int k, i, tmem = 0;
    for (k = 0; k < 4; k++) {   /* level 1: green and black columns, the others solid */
        int w = 32 >> k;
        for (i = 0; i < w * w; i++)
            lv[k][i] = k == 1 && (i & 1) ? rgba16(0, 0, 0, 1) : rgba16(col[k][0], col[k][1], col[k][2], 1);
    }
    frame_begin(0, 0, 0);
    gsDPSetCycleType(G_CYC_2CYCLE);
    gsDPSetRenderMode(RM_OPA_SURF(1), RM_OPA_SURF(2));
    for (k = 0; k < 4; k++) {
        int w = 32 >> k;
        gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, (uintptr_t)lv[k]);
        gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, tmem, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
        gsDPLoadSync();
        gsDPLoadBlock(G_TX_LOADTILE, 0, 0, w * w - 1, calc_dxt(w, 2));
        gsDPPipeSync();
        gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, (w * 2 + 7) >> 3, tmem, k, 0, G_TX_WRAP, 5 - k, k, G_TX_WRAP,
                    5 - k, k);
        gsDPSetTileSize(k, 0, 0, (w - 1) << 2, (w - 1) << 2);
        tmem += w * w * 2 / 8;
    }
    gsSPTexture(0xFFFF, 0xFFFF, 3, 0, 1);
    gsDPSetCombineLERP(CC_TEXEL1, CC_TEXEL0, CC_LOD_FRACTION, CC_TEXEL0, AC_TEXEL1, AC_TEXEL0, AC_LOD_FRACTION,
                       AC_TEXEL0, CC_0, CC_0, CC_0, CC_COMBINED, AC_0, AC_0, AC_0, AC_COMBINED);
    for (i = 0; i < 8; i++) {   /* 24 x 24 pixels at x 8 + 36 i, rows 20-44 */
        int x = 8 + 36 * i, st = (int)lrintf(24 * r[i] * 32);
        vtx(&v[i][0], x, 20, 0, 0, 0, 255, 255, 255, 255);
        vtx(&v[i][1], x, 44, 0, 0, st, 255, 255, 255, 255);
        vtx(&v[i][2], x + 24, 20, 0, st, 0, 255, 255, 255, 255);
        vtx(&v[i][3], x + 24, 44, 0, st, st, 255, 255, 255, 255);
        if (i == 7)
            gsDPSetTextureLOD(G_TL_TILE);
        else if (i == 0)
            gsDPSetTextureLOD(G_TL_LOD);
        gsSPVertex((uintptr_t)v[i], 4, 0);
        gsSP2Triangles(0, 1, 2, 2, 1, 3);
    }
    frame_end();
    check("magnified: the first level", 8 + 12, 32, 255, 0, 0, 2);
    check("2: level 1, even column", 8 + 36 + 12, 32, 0, 255, 0, 3);
    check("2: level 1, odd column", 8 + 36 + 13, 32, 0, 0, 0, 3);
    check("3: halfway to level 2, even", 8 + 72 + 12, 32, 0, 128, 128, 4);   /* level 1 column 1.5 (x + 0.5) */
    check("3: halfway to level 2, odd", 8 + 72 + 14, 32, 0, 0, 128, 4);
    check("4: level 2", 8 + 108 + 12, 32, 0, 0, 255, 3);
    check("6: halfway to level 3", 8 + 144 + 12, 32, 128, 128, 128, 4);
    check("8: the last level", 8 + 180 + 12, 32, 255, 255, 0, 3);
    check("16: the last level", 8 + 216 + 12, 32, 255, 255, 0, 3);
    check("no G_TL_LOD: tile 0", 8 + 252 + 12, 32, 255, 0, 0, 2);
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

/* A road under the camera and a wall beside it, both from behind the eye to far ahead, back faces culled: the game's
 * track at every frame. Clipped at the eye plane with a float lerp, the new vertices' w (lerped from tens of
 * thousands down to 1e-4) came out at or below zero for some of these, which turned the polygon around and culled
 * the whole road (the retail game showed the sky under the car for a frame). Even when drawn, the road's depth near
 * the camera came out far too deep: GL interpolated it from the vertex at the eye plane, at depth 0 but projected
 * millions of pixels out, so what lay under or behind the road and was drawn after it showed through. Clipped to the
 * guard band as the RSP does, each is drawn to the screen's bottom and right edges, and a floor 20 units under the
 * road stays hidden. */
static void case_eyeplane(void) {
    /* (behind, ahead): the road's near vertices' distance behind the eye and its far vertex's ahead. The first five
     * gave the float lerp a w below zero with and without fused multiply-adds (aarch64 builds fuse, x86-64 ones
     * don't), the next four with them only (exactly 0 without); the rest are ordinary. */
    static const int cfg[][2] = {{124, 8190}, {160, 8106}, {214, 8107}, {235, 8175}, {268, 8058}, {3, 8000},
                                 {40, 8001},  {90, 8011},  {235, 8005}, {1, 20000},  {100, 25000}, {300, 30000}};
    static Mtx persp;
    static Vtx v[12][9];
    int k;
    mtx_persp(&persp, 60.0f, 4.0f / 3.0f, 10.0f, 10000.0f);
    for (k = 0; k < 12; k++) {
        int behind = cfg[k][0], far = -cfg[k][1], half = 3000 + k * 251;
        Vtx *f = v[k];
        vtx(&f[0], -half, -40, behind, 0, 0, 0, 255, 255, 255);   /* road: counterclockwise from above */
        vtx(&f[1], half, -40, behind, 0, 0, 0, 255, 255, 255);
        vtx(&f[2], 0, -40, far, 0, 0, 0, 255, 255, 255);
        vtx(&f[3], 30, -40, behind, 0, 0, 255, 0, 255, 255);      /* wall at x 30, facing the camera */
        vtx(&f[4], 30, -40, far, 0, 0, 255, 0, 255, 255);
        vtx(&f[5], 30, 400, behind + 7, 0, 0, 255, 0, 255, 255);
        /* a red floor 20 units under the road, drawn after it: the road hides it everywhere */
        vtx(&f[6], -half, -60, behind + 40, 0, 0, 255, 0, 0, 255);
        vtx(&f[7], half, -60, behind + 40, 0, 0, 255, 0, 0, 255);
        vtx(&f[8], 0, -60, -2000, 0, 0, 255, 0, 0, 255);
        frame_begin(0, 0, 0);
        gsSPMatrix((uintptr_t)&persp, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        gsDPSetRenderMode(RM_ZB_OPA_SURF(1), RM_ZB_OPA_SURF(2));
        gsSPGeometryMode(0, G_ZBUFFER | G_CULL_BACK);
        cc_shade();
        gsSPVertex((uintptr_t)f, 9, 0);
        gsSP2Triangles(0, 1, 2, 3, 5, 4);
        gsSP1Triangle(6, 7, 8);
        frame_end();
        {
            int x, y, red = 0;
            for (y = 130; y < 240; y += 3)
                for (x = 0; x < 300; x += 3)
                    red += px(x, y)[0] > 128 && px(x, y)[1] < 64 && px(x, y)[2] < 64;
            if (red) {
                printf("  FAIL %s: the floor under the road shows through it at %d samples (placement %d)\n", s_case,
                       red, k);
                s_fail++;
            }
        }
        check("road under the camera", 160, 236, 0, 255, 255, 0);
        check("road, bottom left", 4, 236, 0, 255, 255, 0);
        check("road, horizon", 160, 125, 0, 255, 255, 0);
        check("sky", 160, 5, 0, 0, 0, 0);
        check("wall at the right edge", 318, 60, 255, 0, 255, 0);
        check("wall, low", 318, 200, 255, 0, 255, 0);
    }
    /* A chase camera's angles: the camera 30-120 units over a road of two big triangles around it (one corner well
     * behind the eye), pitched 4-16 degrees down and turned to 8 headings; the road fills the bottom of the screen
     * and the floor under it stays hidden. */
    for (k = 0; k < 24; k++) {
        static Mtx view;
        static Vtx road[8];
        float f[4][4], yaw = (float)(k % 8) * 45.0f + 7.0f, pitch = 4.0f + (float)(k / 8) * 6.0f;
        float cy = cosf(yaw * 3.14159265f / 180), sy = sinf(yaw * 3.14159265f / 180);
        float cp = cosf(pitch * 3.14159265f / 180), sp = sinf(pitch * 3.14159265f / 180);
        int h = 30 + (k / 8) * 45, x, y, bad = 0;
        /* row vectors: turn about y by yaw, then tilt about x so that the view looks down by pitch */
        mtx_ident_f(f);
        f[0][0] = cy, f[0][1] = sy * sp, f[0][2] = -sy * cp;
        f[1][0] = 0, f[1][1] = cp, f[1][2] = sp;
        f[2][0] = sy, f[2][1] = -cy * sp, f[2][2] = cy * cp;
        f[3][1] = -(float)h * cp, f[3][2] = -(float)h * sp;
        mtx_f2l(f, &view);
        vtx(&road[0], -20000, 0, -20000, 0, 0, 0, 255, 255, 255);
        vtx(&road[1], -20000, 0, 20000, 0, 0, 0, 255, 255, 255);
        vtx(&road[2], 20000, 0, -20000, 0, 0, 0, 255, 255, 255);
        vtx(&road[3], 20000, 0, 20000, 0, 0, 0, 255, 255, 255);
        vtx(&road[4], -20000, -20, -20000, 0, 0, 255, 0, 0, 255);
        vtx(&road[5], -20000, -20, 20000, 0, 0, 255, 0, 0, 255);
        vtx(&road[6], 20000, -20, -20000, 0, 0, 255, 0, 0, 255);
        vtx(&road[7], 20000, -20, 20000, 0, 0, 255, 0, 0, 255);
        frame_begin(0, 0, 0);
        gsSPMatrix((uintptr_t)&persp, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        gsSPMatrix((uintptr_t)&view, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        gsDPSetRenderMode(RM_ZB_OPA_SURF(1), RM_ZB_OPA_SURF(2));
        gsSPGeometryMode(0, G_ZBUFFER | G_CULL_BACK);
        cc_shade();
        gsSPVertex((uintptr_t)road, 8, 0);
        gsSP2Triangles(0, 1, 2, 2, 1, 3);
        gsSP2Triangles(4, 5, 6, 6, 5, 7);
        frame_end();
        for (y = 200; y < 240; y += 4)
            for (x = 0; x < 320; x += 8)
                bad += !(px(x, y)[0] < 64 && px(x, y)[1] > 192 && px(x, y)[2] > 192);
        if (bad) {
            printf("  FAIL %s: chase camera %d (heading %.0f, pitch %.0f, height %d): %d samples of the bottom rows "
                   "aren't the road\n", s_case, k, yaw, pitch, h, bad);
            s_fail++;
        }
    }
}

/* What the clipping hands on to GL, on the CPU (tests/clip_check.c). */
int gfx_clip_check(void);

static void case_clip(void) {
    s_fail += gfx_clip_check();
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
    int i, x, y, ok;
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
    if (s_n64_w != FB_W) {
        printf("  FAIL widescreen off: the target is %dx%d\n", s_n64_w, s_n64_h);
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
    if (s_n64_w != WIDE_W || s_n64_h != FB_H) {
        printf("  FAIL widescreen 16:9: the target is %dx%d N64 pixels, not %dx%d\n", s_n64_w, s_n64_h, WIDE_W, FB_H);
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

/* ---- the race HUD at the screen's edges (GFX.md "The HUD at the screen's edges"): a race frame's 2D, made up for the
 * test. On blue: a perspective strip across the target (rows 212-220); left, a bezel fill (6-54, rows 5-23) with a
 * copy-mode digit on it (14-22, rows 8-16) and an orthographic triangle (8-28, rows 40-50); right, a run of four
 * digits a pixel apart (270-305, rows 10-18) and a radar fill (240-300, rows 100-140) cut by its scissor (250-290);
 * in the middle a banner (90-230, rows 60-80) with a digit at each end (95, 217) and a run of digits 3 pixels apart
 * across the band's left side (60-156, rows 90-98); a full-width fill (rows 200-210); then a pause-like panel
 * (40-280, rows 145-195, drawn after a radar fill 10-42 under its left side) with a digit in it (50, rows 160-168),
 * a second panel (20-300, rows 205-240) and then another digit on the first panel (270, rows 185-193). */
static int s_race;
static int test_race(void) { return s_race; }
static uint16_t s_hdig[8 * 8];

static void hud_digit(int x, int y) {
    gsSPTextureRectangle(x << 2, y << 2, (x + 7) << 2, (y + 7) << 2, 0, 0, 0, 4 << 10, 1 << 10);
}

static void hud_fill(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
    gsDPSetCycleType(G_CYC_1CYCLE);
    gsDPSetRenderMode(RM_OPA_SURF(1), RM_OPA_SURF(2));
    gsDPSetPrimColor(0, 0, r, g, b, 255);
    cc_prim();
    gsDPFillRectangle(x0, y0, x1, y1);
}

static void hud_copy(void) {
    load_texture_block((uintptr_t)s_hdig, G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 8, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    gsDPSetCycleType(G_CYC_COPY);
    gsDPSetRenderMode(0, 0);
}

static void hud_scene(void) {
    static Vtx v[8];
    int i;
    for (i = 0; i < 64; i++)
        s_hdig[i] = rgba16(255, 255, 255, 1);
    frame_begin(0, 0, 255);
    cc_shade();
    gsSPMatrix((uintptr_t)&s_frustum, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    quad(&v[0], -200, -84, 200, -76, -100, 255, 128, 0);
    gsSPVertex((uintptr_t)v, 4, 0);
    gsSP2Triangles(0, 1, 2, 2, 1, 3);
    gsSPMatrix((uintptr_t)&s_ortho, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    hud_fill(6, 5, 54, 23, 255, 0, 0);
    hud_copy();
    hud_digit(14, 8);
    gsDPSetCycleType(G_CYC_1CYCLE);
    gsDPSetRenderMode(RM_OPA_SURF(1), RM_OPA_SURF(2));
    cc_shade();
    vtx(&v[4], 8, 40, 0, 0, 0, 255, 0, 255, 255);
    vtx(&v[5], 8, 50, 0, 0, 0, 255, 0, 255, 255);
    vtx(&v[6], 28, 40, 0, 0, 0, 255, 0, 255, 255);
    gsSPVertex((uintptr_t)&v[4], 3, 0);
    gsSP1Triangle(0, 1, 2);
    hud_copy();
    for (i = 0; i < 4; i++)
        hud_digit(270 + 9 * i, 10);
    gsDPSetScissor(250, 100, 290, 140);
    hud_fill(240, 100, 300, 140, 0, 255, 0);
    gsDPSetScissor(0, 0, FB_W, FB_H);
    hud_fill(90, 60, 230, 80, 255, 255, 0);
    hud_copy();
    hud_digit(95, 64);
    hud_digit(217, 64);
    for (i = 0; i < 9; i++)
        hud_digit(60 + 11 * i, 90);
    hud_fill(0, 200, FB_W, 210, 0, 255, 255);
    hud_fill(10, 150, 42, 190, 128, 128, 128);
    hud_fill(40, 145, 280, 195, 64, 0, 64);
    hud_copy();
    hud_digit(50, 160);
    hud_fill(20, 205, 300, 240, 0, 64, 64);
    hud_copy();
    hud_digit(270, 185);
    frame_end();
}

static void case_hud(void) {
    static const struct { const char *name; float aspect; int ox; } wide[] = {{"16:9", 16.0f / 9, 53},
                                                                              {"21:9", 21.0f / 9, 120}};
    int ok, k, i, w = 255;
    float env_aspect = pc_gfx_parse_aspect(getenv("RUSH_GFX_WIDESCREEN"), &ok);
    int env_hud = pc_gfx_hud();
    uint8_t *ref;
    size_t n;
    float f[4][4];

    if (pc_gfx_parse_hud("edges", &ok) != PC_GFX_HUD_EDGES || !ok || pc_gfx_parse_hud("centered", &ok) !=
        PC_GFX_HUD_CENTERED || !ok || (pc_gfx_parse_hud("left", &ok), ok)) {
        printf("  FAIL hud: parsing edges, centered, left\n");
        s_fail++;
    }
    memset(f, 0, sizeof(f));
    f[0][0] = f[1][1] = 1.0f;
    f[2][2] = (10.0f + 1000.0f) / (10.0f - 1000.0f);
    f[2][3] = -1.0f;
    f[3][2] = 2.0f * 10.0f * 1000.0f / (10.0f - 1000.0f);
    mtx_f2l(f, &s_frustum);
    pc_gfx_set_hud_race(test_race);

    /* 4:3: edges in a race draws what centered draws */
    pc_gfx_set_widescreen(0.0f);
    s_race = 0;
    pc_gfx_set_hud(PC_GFX_HUD_CENTERED);
    hud_scene();
    n = (size_t)s_px_w * s_px_h * 4;
    ref = (uint8_t *)malloc(n);
    memcpy(ref, s_px, n);
    s_race = 1;
    pc_gfx_set_hud(PC_GFX_HUD_EDGES);
    hud_scene();
    if ((size_t)s_px_w * s_px_h * 4 != n || memcmp(ref, s_px, n) != 0) {
        printf("  FAIL hud 4:3: edges in a race differs from centered\n");
        s_fail++;
    }
    free(ref);

    for (k = 0; k < 2; k++) {
        int ox = wide[k].ox, W = FB_W + 2 * ox;
        pc_gfx_set_widescreen(wide[k].aspect);
        /* centered (and edges outside a race): the 2D at the 4:3 frame's place, the same frame either way */
        s_race = 1;
        pc_gfx_set_hud(PC_GFX_HUD_CENTERED);
        hud_scene();
        if (s_n64_w != W) {
            printf("  FAIL hud %s: the target is %d wide, not %d\n", wide[k].name, s_n64_w, W);
            s_fail++;
            break;
        }
        check_at("centered: the bezel with the frame", ox + 8, 20, 255, 0, 0, 0);
        check_at("centered: the digit run with the frame", ox + 271, 12, w, w, w, 0);
        check_at("centered: nothing at the right edge", W - 10, 12, 0, 0, 255, 0);
        if (pc_gfx_hud_dx(30) != 0 || pc_gfx_hud_dx(290) != 0) {
            printf("  FAIL hud %s: centered moves a point\n", wide[k].name);
            s_fail++;
        }
        n = (size_t)s_px_w * s_px_h * 4;
        ref = (uint8_t *)malloc(n);
        memcpy(ref, s_px, n);
        s_race = 0;
        pc_gfx_set_hud(PC_GFX_HUD_EDGES);
        hud_scene();
        if ((size_t)s_px_w * s_px_h * 4 != n || memcmp(ref, s_px, n) != 0) {
            printf("  FAIL hud %s: edges outside a race differs from centered\n", wide[k].name);
            s_fail++;
        }
        free(ref);

        /* edges in a race */
        s_race = 1;
        hud_scene();
        if (pc_gfx_hud_dx(30) != -ox || pc_gfx_hud_dx(290) != ox || pc_gfx_hud_dx(160) != 0 ||
            pc_gfx_hud_dx(110) != 0 || pc_gfx_hud_dx(210) != 0) {
            printf("  FAIL hud %s: a point's move %d %d %d %d %d\n", wide[k].name, pc_gfx_hud_dx(30),
                   pc_gfx_hud_dx(290), pc_gfx_hud_dx(160), pc_gfx_hud_dx(110), pc_gfx_hud_dx(210));
            s_fail++;
        }
        /* 3D as before: the strip across the target */
        check_at("3D strip at the left edge", 0, 216, 255, 128, 0, 0);
        check_at("3D strip at the right edge", W - 1, 216, 255, 128, 0, 0);
        /* left: the bezel, its digit and the triangle keep their distance to the left edge */
        check_at("bezel at the left edge", 6, 20, 255, 0, 0, 0);
        check_at("bezel's right side", 53, 20, 255, 0, 0, 0);
        check_at("bezel: nothing after it", 55, 20, 0, 0, 255, 0);
        check_at("bezel: not at the 4:3 place", ox + 50, 20, 0, 0, 255, 0);
        for (i = 0; i < 8; i++)
            check_at("digit on the bezel, texel for texel", 14 + i, 8 + i, w, w, w, 0);
        check_at("digit on the bezel: the bezel beside it", 13, 12, 255, 0, 0, 0);
        check_at("orthographic triangle at the left", 10, 42, 255, 0, 255, 0);
        check_at("orthographic triangle: not at the 4:3 place", ox + 12, 42, 0, 0, 255, 0);
        /* right: the run of digits, together */
        for (i = 0; i < 4; i++) {
            check_at("digit run at the right", 2 * ox + 270 + 9 * i, 12, w, w, w, 0);
            check_at("digit run: the gap between", 2 * ox + 278 + 9 * i, 12, 0, 0, 255, 0);
        }
        check_at("digit run: not at the 4:3 place", ox + 271, 12, 0, 0, 255, 0);
        /* the radar's scissor moved with it */
        check_at("radar inside its scissor", 2 * ox + 251, 120, 0, 255, 0, 0);
        check_at("radar to its scissor's right side", 2 * ox + 289, 120, 0, 255, 0, 0);
        check_at("radar cut at its scissor's left side", 2 * ox + 248, 120, 0, 0, 255, 0);
        check_at("radar cut at its scissor's right side", 2 * ox + 292, 120, 0, 0, 255, 0);
        /* the middle stays: the banner and its end digits; the run across the band's side */
        check_at("banner stays", ox + 92, 62, 255, 255, 0, 0);
        check_at("banner's left digit stays", ox + 96, 66, w, w, w, 0);
        check_at("banner's right digit stays", ox + 218, 66, w, w, w, 0);
        check_at("run across the band: first digit stays", ox + 61, 92, w, w, w, 0);
        check_at("run across the band: last digit stays", ox + 149, 92, w, w, w, 0);
        check_at("run across the band: not moved left", 61, 92, 0, 0, 255, 0);
        /* the full-width fill spans the target */
        check_at("full-width fill: left edge", 0, 205, 0, 255, 255, 0);
        check_at("full-width fill: right edge", W - 1, 205, 0, 255, 255, 0);
        /* the panel stays and keeps its digit; the radar under its side moves left */
        check_at("panel stays", ox + 45, 150, 64, 0, 64, 0);
        check_at("panel's digit stays", ox + 51, 162, w, w, w, 0);
        check_at("radar under the panel moves left", 12, 170, 128, 128, 128, 0);
        check_at("radar under the panel: its right side", 41, 170, 128, 128, 128, 0);
        check_at("a digit drawn after the second panel stays on the first", ox + 271, 187, w, w, w, 0);
        check_at("that digit: not moved right", 2 * ox + 271, 187, 0, 0, 255, 0);
        check_at("the second panel stays", ox + 25, 230, 0, 64, 64, 0);
    }
    pc_gfx_set_hud_race(NULL);
    pc_gfx_set_hud(env_hud);
    pc_gfx_set_widescreen(env_aspect);
}

/* the auto settings follow the window (here the headless size, pc_gfx_set_window_size): widescreen takes its aspect,
 * the auto scale the largest whole multiple of the frame that it holds, and native the window's own pixels (any
 * fraction; the frame here shows all 240 rows) */
static void case_autosize(void) {
    static uint16_t t[8 * 8];
    int i, x, y, ok;
    float env_aspect = pc_gfx_parse_aspect(getenv("RUSH_GFX_WIDESCREEN"), &ok);
    float env_scale = getenv("RUSH_GFX_SCALE") ? pc_gfx_parse_scale(getenv("RUSH_GFX_SCALE"), &ok) : 1.0f;
    static const struct { int win_w, win_h; float aspect; int scale, w, px_w, px_h; } steps[] = {
        {1280, 720, -1.0f, 0, 426, 1278, 720},    /* 16:9: 426 x 240, 3 x 3 fits 1280 x 720 */
        {1920, 1080, -1.0f, 0, 426, 1704, 960},   /* 4 x 426 = 1704 */
        {1024, 768, -1.0f, 0, 320, 960, 720},     /* 4:3: widescreen off */
        {1280, 720, -1.0f, 2, 426, 852, 480},     /* a fixed scale stays */
        {1280, 720, 21.0f / 9, 0, 560, 1120, 480},   /* a fixed aspect: 560 x 240, 2 x 560 fits 1280 */
        {640, 480, 0.0f, 0, 320, 640, 480},
        {2880, 1620, -1.0f, -1, 426, 2876, 1620},  /* native: 6.75, the rows' fit (2875.5 rounds up) */
        {2880, 1620, 0.0f, -1, 320, 2160, 1620},   /* native 4:3 */
        {1366, 768, -1.0f, -1, 426, 1363, 768},    /* native 3.2 */
        {1440, 810, -1.0f, -1, 426, 1438, 810},    /* native 3.375 (a 200% desktop without DPI awareness) */
        {2880, 1620, -1.0f, 1, 426, 426, 240},     /* N64 pixels */
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
        if (s_n64_w != steps[i].w || s_px_w != steps[i].px_w || s_px_h != steps[i].px_h) {
            printf("  FAIL autosize %dx%d scale %d: the target is %dx%d (%d across), not %dx%d (%d)\n", steps[i].win_w,
                   steps[i].win_h, steps[i].scale, s_px_w, s_px_h, s_n64_w, steps[i].px_w, steps[i].px_h, steps[i].w);
            s_fail++;
            continue;
        }
        for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++)
                check("texrect", 10 + x, 10 + y, x5(x * 32), x5(y * 32), x5(128), 1);
    }
    pc_gfx_set_window_size(s_win_w, s_win_h);
    pc_gfx_set_widescreen(env_aspect);
    pc_gfx_set_scale_value(env_scale);
}

/* 2D at the internal scale, fractional ones above all: tiles of copy-mode and point-sampled texture rectangles side
 * by side (a background, menu art), glyph-sized rectangles a pixel apart (the game's fonts), magnified rectangles,
 * fill and 1-cycle bars a pixel wide or tall, a wide fill to the frame's bottom. Every internal pixel of the 4:3
 * frame must have its N64 pixel's color (the one its centre is in): no gap (the clear showing between neighbours),
 * no bleed (a neighbouring texel or rectangle), no seam. */
static void seams_expect(int x, int y, int c[3]) {
    int u = -1, v = 0;
    c[0] = 255, c[1] = 0, c[2] = 255;   /* the clear */
    if (y < 64)
        u = x % 16, v = y % 16;
    else if (y < 112 && x >= 3 && x < 3 + 13 * 24)
        u = (x - 3) % 13, v = (y - 64) % 16;
    else if (y >= 120 && y < 129 && x >= 1 && (x - 1) % 6 < 5 && (x - 1) / 6 < 53)
        u = (x - 1) % 6, v = y - 120;
    else if (y >= 136 && y < 168)
        u = (x % 32) / 2, v = (y - 136) / 2;
    else if (y >= 172 && y < 180 && x >= 8 && x < 312)
        c[0] = (x & 1) ? 255 : 0, c[1] = (x & 1) ? 0 : 255, c[2] = 0;
    else if (y >= 182 && y < 190 && x >= 8 && x < 312)
        c[0] = c[1] = (y & 1) ? 255 : 0, c[2] = 255;
    else if (y >= 200 && x >= 8 && x < 312)
        c[0] = c[1] = 255, c[2] = 0;
    if (u >= 0)
        c[0] = x5(u * 16), c[1] = x5(v * 16), c[2] = x5(128);
}

static void case_seams(void) {
    static uint16_t t[16 * 16];
    int i, k, x, y, bad = 0, ox, shown = 0;
    for (i = 0; i < 256; i++)
        t[i] = rgba16((i & 15) * 16, (i >> 4) * 16, 128, 1);
    frame_begin(255, 0, 255);
    load_texture_block((uintptr_t)t, G_IM_FMT_RGBA, G_IM_SIZ_16b, 16, 16, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    gsDPSetCycleType(G_CYC_COPY);
    gsDPSetRenderMode(0, 0);
    for (y = 0; y < 64; y += 16)
        for (x = 0; x < FB_W; x += 16)   /* copy mode: the lower right is inclusive */
            gsSPTextureRectangle(x << 2, y << 2, (x + 15) << 2, (y + 15) << 2, 0, 0, 0, 4 << 10, 1 << 10);
    gsDPSetCycleType(G_CYC_1CYCLE);
    gsDPSetRenderMode(RM_OPA_SURF(1), RM_OPA_SURF(2));
    cc_decalrgba();
    for (y = 64; y < 112; y += 16)
        for (k = 0; k < 24; k++)
            gsSPTextureRectangle((3 + 13 * k) << 2, y << 2, (16 + 13 * k) << 2, (y + 16) << 2, 0, 0, 0, 1 << 10,
                                 1 << 10);
    for (k = 0; k < 53; k++)
        gsSPTextureRectangle((1 + 6 * k) << 2, 120 << 2, (6 + 6 * k) << 2, 129 << 2, 0, 0, 0, 1 << 10, 1 << 10);
    for (x = 0; x < FB_W; x += 32)
        gsSPTextureRectangle(x << 2, 136 << 2, (x + 32) << 2, 168 << 2, 0, 0, 0, 1 << 9, 1 << 9);
    gsDPPipeSync();
    gsDPSetCycleType(G_CYC_FILL);
    for (x = 8; x < 312; x++) {
        uint16_t c = (x & 1) ? rgba16(255, 0, 0, 1) : rgba16(0, 255, 0, 1);
        gsDPSetFillColor((uint32_t)c << 16 | c);
        gsDPFillRectangle(x, 172, x, 179);
    }
    gsDPPipeSync();
    gsDPSetCycleType(G_CYC_1CYCLE);
    cc_prim();
    for (y = 182; y < 190; y++) {
        gsDPSetPrimColor(0, 0, (y & 1) ? 255 : 0, (y & 1) ? 255 : 0, 255, 255);
        gsDPFillRectangle(8, y, 312, y + 1);
    }
    gsDPSetPrimColor(0, 0, 255, 255, 0, 255);
    gsDPFillRectangle(8, 200, 312, FB_H);
    frame_end();
    ox = frame_x0();
    for (y = 0; y < s_px_h; y++)
        for (x = 0; x < s_px_w; x++) {
            int nx = pcgfx_pixel_of(x, s_n64_w, s_px_w) - ox, ny = pcgfx_row_of(y, s_n64_h, s_px_h), c[3];
            const uint8_t *p = s_px + ((size_t)y * s_px_w + x) * 4;
            if (nx < 0 || nx >= FB_W || ny >= FB_H)
                continue;
            seams_expect(nx, ny, c);
            if (abs(p[0] - c[0]) > 1 || abs(p[1] - c[1]) > 1 || abs(p[2] - c[2]) > 1) {
                if (shown++ < 8)
                    printf("  FAIL seams: internal pixel (%d,%d), N64 (%d,%d): %d,%d,%d, not %d,%d,%d\n", x, y, nx,
                           ny, p[0], p[1], p[2], c[0], c[1], c[2]);
                bad++;
            }
        }
    if (bad) {
        printf("  FAIL seams: %d of %d internal pixels (%dx%d for %dx%d N64 pixels)\n", bad, s_px_w * s_px_h, s_px_w,
               s_px_h, s_n64_w, s_n64_h);
        s_fail++;
    }
}

/* the screenshot is a PNG of the presented frame */
static void case_screenshot(void) {
    char path[512];
    unsigned char hdr[24];
    FILE *f;
    frame_begin(0, 128, 0);
    frame_end();
#ifdef _WIN32
    snprintf(path, sizeof(path), "%s/screenshot_test.png",
             s_png_dir ? s_png_dir : getenv("TEMP") ? getenv("TEMP") : ".");
#else
    snprintf(path, sizeof(path), "%s/screenshot_test.png",
             s_png_dir ? s_png_dir : getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
#endif
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
    {"lod", case_lod},           {"dualtile", case_dualtile}, {"belist", case_belist},
    {"matrix", case_matrix},     {"cull", case_cull},         {"branchz", case_branchz},
    {"nearclip", case_nearclip}, {"eyeplane", case_eyeplane}, {"clip", case_clip},
    {"xlu", case_xlu},           {"widescreen", case_widescreen}, {"autosize", case_autosize},
    {"seams", case_seams},       {"screenshot", case_screenshot}, {"hud", case_hud},
};

int main(int argc, char **argv) {
    const char *which = argc > 1 ? argv[1] : "all";
    int i, ran = 0, failed = 0, window = 0;
    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--png") == 0 && i + 1 < argc)
            s_png_dir = argv[++i];
        else if (strcmp(argv[i], "--window") == 0)
            window = 1;
        else if (strcmp(argv[i], "--no-window-readback") == 0)
            s_window_readback = 0;
    }
    s_rdram = (uint8_t *)calloc(1, PC_RDRAM_SIZE);
    mtx_ortho(&s_ortho, 0, FB_W, FB_H, 0, -1000, 1000);
    {
        float f[4][4];
        mtx_ident_f(f);
        mtx_f2l(f, &s_ident);
    }
    s_window = window;
    if (getenv("RUSH_GFX_TEST_WINDOW") != NULL &&
        sscanf(getenv("RUSH_GFX_TEST_WINDOW"), "%dx%d", &s_win_w, &s_win_h) != 2)
        s_win_w = 640, s_win_h = 480;
    if (pc_gfx_init(!window, s_win_w, s_win_h) != 0) {
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
