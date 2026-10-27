/* port/gfx/tests/interp_test.c: frame interpolation (gfx_interp.c, GFX.md "High frame rate"), headless.
 *
 *   gfx_interp_test <case>|all [--png DIR]
 *
 * Each case builds a scene as the game does (a perspective projection with the camera's turn multiplied into it,
 * a modelview per object carrying its place against the camera, models whose vertices are big-endian data
 * that stays put, matrices and lists in two alternating frame buffers) at two moments, B and C. B is swapped in at
 * retrace 10, C is drawn, and pc_gfx_present_tick shows the moment between them. The window then must match a
 * frame drawn directly with the scene as it should look at that moment (the reference), to within a few pixels on
 * edges. What the reference is follows the interpolator's rules: objects in both frames move part of the way,
 * new ones are where C has them, seen through the camera at that moment, a teleport is where C has it, 2D is C's,
 * a cut shows B until C is due. Exit status 0 = pass. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_hfr.h"
#include "pc_port.h"
#include "test_gbi.h"

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
} s_be[8];

void pc_mem_register_be(const void *start, size_t size) {
    int i;
    for (i = 0; i < 8; i++)
        if (s_be[i].p == NULL) {
            s_be[i].p = (const uint8_t *)start;
            s_be[i].n = size;
            return;
        }
}

void pc_mem_unregister_be(const void *start) {
    int i;
    for (i = 0; i < 8; i++)
        if (s_be[i].p == start)
            s_be[i].p = NULL;
}

int pc_mem_is_be(const void *p) {
    int i;
    for (i = 0; i < 8; i++)
        if (s_be[i].p != NULL && (const uint8_t *)p >= s_be[i].p && (const uint8_t *)p < s_be[i].p + s_be[i].n)
            return 1;
    return 0;
}

/* ---------------------------------------------------------------- matrices */

#define FB_W 320
#define FB_H 240
#define WIN_W 640
#define WIN_H 480

typedef struct {
    uint32_t m[16];
} Mtx;

typedef struct {
    int16_t vscale[4], vtrans[4];
} Vp;

static void mtx_f2l(const float f[4][4], Mtx *m) {
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 2; j++) {
            int32_t e1 = (int32_t)lrintf(f[i][j * 2] * 65536.0f), e2 = (int32_t)lrintf(f[i][j * 2 + 1] * 65536.0f);
            m->m[i * 2 + j] = ((uint32_t)e1 & 0xFFFF0000u) | (((uint32_t)e2 >> 16) & 0xFFFF);
            m->m[8 + i * 2 + j] = ((uint32_t)e1 << 16) | ((uint32_t)e2 & 0xFFFF);
        }
}

static void ident(float f[4][4]) {
    memset(f, 0, sizeof(float) * 16);
    f[0][0] = f[1][1] = f[2][2] = f[3][3] = 1.0f;
}

static void persp(float f[4][4], float fovy, float aspect, float n, float fa) {
    float c = 1.0f / tanf(fovy * 3.14159265f / 360.0f);
    memset(f, 0, sizeof(float) * 16);
    f[0][0] = c / aspect;
    f[1][1] = c;
    f[2][2] = (n + fa) / (n - fa);
    f[2][3] = -1.0f;
    f[3][2] = 2.0f * n * fa / (n - fa);
}

/* the camera's turn about y (row vectors: world to camera axes), as the game multiplies its lookat into the
 * projection */
static void look(float f[4][4], float yaw_deg) {
    float a = yaw_deg * 3.14159265f / 180.0f, c = cosf(a), s = sinf(a);
    ident(f);
    f[0][0] = c;
    f[0][2] = s;
    f[2][0] = -s;
    f[2][2] = c;
}

/* ---------------------------------------------------------------- scenes */

enum { M_RED, M_GREEN, M_BLUE, M_YELLOW, M_GROUND, M_TEX, M_S0, M_S1, M_S2, M_S3, NUM_MODELS };

typedef struct {
    int16_t ob[3];
    uint16_t flag;
    int16_t tc[2];
    uint8_t cn[4];
} Vtx;

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

typedef struct {
    int model;
    float p0[3], p1[3];       /* world position at B and at C */
    int in0, in1;             /* drawn at B, at C */
    int teleport;             /* a jump: shown where C has it */
    int children, child_model;   /* instances of one model under it (a car's wheels), child_gap apart along x */
    float child_gap;
    float rot0, rot1;         /* its turn about z at B and at C, degrees: a turn too large to blend (a wheel) */
} Obj;

#define MAX_VIEWS 4
#define MAX_OBJS 12

typedef struct {
    int views;                /* 1, 2 (top and bottom) or 4 (quarters) */
    float yaw0[MAX_VIEWS], yaw1[MAX_VIEWS];
    float eye0[MAX_VIEWS][3], eye1[MAX_VIEWS][3];
    Obj obj[MAX_VIEWS][MAX_OBJS];
    int nobj[MAX_VIEWS];
    int cut;                  /* the expected moment is B (a camera cut) */
    int hud, hud_x0, hud_x1;  /* a texture rectangle (2D) */
    int ortho;                /* an orthographic triangle that moves: 2D */
    float ortho_x0, ortho_x1;
    int dyn;                  /* an eye-relative quad of native vertices under the identity modelview */
    int dyn_fixed;            /* ...at one address every frame (else in the frame's own buffer) */
    float dyn_p[3];           /* its world position (it stays put; the camera moves) */
    int extras;               /* fog, the near plane, a texture, culling, depth */
} Scene;

/* Big-endian model data, as the game's WADs keep it: a 20 x 20 square per model, in the xy plane, a colour each.
 * The ground is a large square in the xz plane, through the eye plane; the textured one samples a checker. */
static uint8_t *s_models;          /* in the RDRAM window, registered big-endian */
static uint32_t s_model_vtx[NUM_MODELS];
static uint32_t s_tex_addr;

static void be16(uint8_t *p, uint16_t v) {
    p[0] = v >> 8;
    p[1] = (uint8_t)v;
}

static void be_vtx(uint8_t *p, int x, int y, int z, int s, int t, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    be16(p, (uint16_t)x);
    be16(p + 2, (uint16_t)y);
    be16(p + 4, (uint16_t)z);
    be16(p + 6, 0);
    be16(p + 8, (uint16_t)s);
    be16(p + 10, (uint16_t)t);
    p[12] = r;
    p[13] = g;
    p[14] = b;
    p[15] = a;
}

static uint16_t rgba16(int r, int g, int b, int a) {
    return (uint16_t)((r >> 3) << 11 | (g >> 3) << 6 | (b >> 3) << 1 | (a ? 1 : 0));
}

static void make_models(void) {
    static const uint8_t col[NUM_MODELS][3] = {{255, 0, 0},     {0, 255, 0},   {0, 0, 255},   {255, 255, 0},
                                               {120, 120, 120}, {255, 255, 255}, {0, 90, 200}, {0, 120, 220},
                                               {0, 150, 240},   {0, 180, 255}};
    int m, i;
    s_models = (uint8_t *)pc_n64_ptr(0x80200000u);
    memset(s_models, 0, 0x2000);
    for (m = 0; m < NUM_MODELS; m++) {
        uint8_t *v = s_models + m * 64;
        s_model_vtx[m] = 0x80200000u + (uint32_t)m * 64;
        if (m == M_GROUND) {   /* 4000 x 4000 at y = -40, so it reaches behind the eye */
            be_vtx(v + 0, -2000, -40, -2000, 0, 0, 90, 140, 90, 255);
            be_vtx(v + 16, -2000, -40, 2000, 0, 0, 90, 140, 90, 255);
            be_vtx(v + 32, 2000, -40, -2000, 0, 0, 140, 90, 90, 255);
            be_vtx(v + 48, 2000, -40, 2000, 0, 0, 140, 90, 90, 255);
        } else {
            be_vtx(v + 0, -10, -10, 0, 0, 16 << 5, col[m][0], col[m][1], col[m][2], 255);
            be_vtx(v + 16, 10, -10, 0, 16 << 5, 16 << 5, col[m][0], col[m][1], col[m][2], 255);
            be_vtx(v + 32, -10, 10, 0, 0, 0, col[m][0], col[m][1], col[m][2], 255);
            be_vtx(v + 48, 10, 10, 0, 16 << 5, 0, col[m][0], col[m][1], col[m][2], 255);
        }
    }
    s_tex_addr = 0x80201000u;
    for (i = 0; i < 16 * 16; i++)   /* a 16 x 16 RGBA16 checker */
        be16(s_models + 0x1000 + 2 * i, ((i & 15) / 4 + i / 64) & 1 ? rgba16(255, 255, 255, 1) : rgba16(255, 0, 255, 1));
    pc_mem_register_be(s_models, 0x2000);
}

/* Two frame buffers, as the game alternates its frame records: matrices, viewports, lists and native vertices. */
static uint16_t s_fb[2][FB_W * FB_H], s_zb[FB_W * FB_H];
static PcGfxWords s_list[2][8192];
static Mtx s_mtx[2][128];
static Vp s_vp[2][MAX_VIEWS];
static uint8_t s_dynv[2][64], s_dynv_fixed[64];
static int s_nmtx;
static const char *s_case, *s_png_dir;
static int s_fail;

static Mtx *new_mtx(int buf, const float f[4][4]) {
    Mtx *m = &s_mtx[buf][s_nmtx++];
    mtx_f2l(f, m);
    return m;
}

/* What the scene is at fraction a of the way (the frames: a = 0 is B, 1 is C; mode 2: the expected picture). */
static float lerpf(float x, float y, float a) {
    return x + (y - x) * a;
}

static void view_rect(int views, int k, int r[4]) {   /* x0, y0, x1, y1 */
    r[0] = 0, r[1] = 0, r[2] = FB_W, r[3] = FB_H;
    if (views == 2) {
        r[1] = k * FB_H / 2;
        r[3] = r[1] + FB_H / 2;
    } else if (views == 4) {
        r[0] = (k & 1) * FB_W / 2;
        r[2] = r[0] + FB_W / 2;
        r[1] = (k >> 1) * FB_H / 2;
        r[3] = r[1] + FB_H / 2;
    }
}

static void quad_tris(void) {
    gsSP2Triangles(0, 1, 2, 2, 1, 3);
}

/* buf: the frame buffer to build in; a: the moment; expect: the expected picture rather than a frame of the
 * game (a frame is a = 0 or 1) */
static void build(const Scene *sc, int buf, float a, int expect) {
    int k, i;
    float f[4][4], g[4][4];
    s_nmtx = 0;
    gp = s_list[buf];
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
    gsDPSetColorImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, FB_W, (uintptr_t)s_fb[buf]);
    gsDPSetFillColor((uint32_t)rgba16(0, 0, 40, 1) << 16 | rgba16(0, 0, 40, 1));
    gsDPFillRectangle(0, 0, FB_W - 1, FB_H - 1);
    gsDPPipeSync();
    gsDPSetCycleType(G_CYC_1CYCLE);
    gsDPSetRenderMode(RM_ZB_OPA_SURF(1), RM_ZB_OPA_SURF(2));
    gsSPGeometryMode(0xFFFFFFFF, G_SHADE | G_SHADING_SMOOTH | G_ZBUFFER);
    gsDPSetTextureLUT(G_TT_NONE);
    gsDPSetTextureFilter(G_TF_POINT);
    gsDPSetAlphaCompare(G_AC_NONE);
    gsDPSetDepthSource(G_ZS_PIXEL);
    cc_shade();
    for (k = 0; k < sc->views; k++) {
        int r[4];
        float yaw, eye[3];
        Vp *vp = &s_vp[buf][k];
        view_rect(sc->views, k, r);
        vp->vscale[0] = (int16_t)((r[2] - r[0]) * 2), vp->vscale[1] = (int16_t)((r[3] - r[1]) * 2);
        vp->vscale[2] = 511, vp->vscale[3] = 0;
        vp->vtrans[0] = (int16_t)((r[0] + r[2]) * 2), vp->vtrans[1] = (int16_t)((r[1] + r[3]) * 2);
        vp->vtrans[2] = 511, vp->vtrans[3] = 0;
        gsSPViewport((uintptr_t)vp);
        gsDPSetScissor(r[0], r[1], r[2], r[3]);
        for (i = 0; i < 3; i++)
            eye[i] = lerpf(sc->eye0[k][i], sc->eye1[k][i], a);
        persp(f, 60.0f, (float)(r[2] - r[0]) / (float)(r[3] - r[1]), 10.0f, 3000.0f);
        gsSPMatrix((uintptr_t)new_mtx(buf, f), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        if (expect && a > 0.0f && a < 1.0f) {   /* the blend of the two turns, as the interpolator blends them */
            float l0[4][4], l1[4][4];
            int p, q;
            look(l0, sc->yaw0[k]);
            look(l1, sc->yaw1[k]);
            for (p = 0; p < 4; p++)
                for (q = 0; q < 4; q++)
                    g[p][q] = lerpf(l0[p][q], l1[p][q], a);
        } else {
            yaw = lerpf(sc->yaw0[k], sc->yaw1[k], a);
            look(g, yaw);
        }
        gsSPMatrix((uintptr_t)new_mtx(buf, g), G_MTX_PROJECTION | G_MTX_MUL | G_MTX_NOPUSH);
        ident(f);
        gsSPMatrix((uintptr_t)new_mtx(buf, f), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        if (sc->extras && k == 0) {   /* the ground, through the eye plane, with fog in 2-cycle mode */
            ident(f);
            f[3][0] = -eye[0], f[3][1] = -eye[1], f[3][2] = -eye[2];
            gsSPMatrix((uintptr_t)new_mtx(buf, f), G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
            gsDPPipeSync();
            gsDPSetCycleType(G_CYC_2CYCLE);
            gsDPSetRenderMode(G_RM_FOG_SHADE_A, RM_ZB_OPA_SURF(2));
            gsDPSetCombineLERP(CC_0, CC_0, CC_0, CC_SHADE, AC_0, AC_0, AC_0, AC_SHADE, CC_0, CC_0, CC_0,
                               CC_COMBINED, AC_0, AC_0, AC_0, AC_COMBINED);
            gsDPSetFogColor(40, 40, 120, 255);
            gsSPFogPosition(900, 1000);
            gsSPGeometryMode(0, G_FOG);
            gsSPVertex(s_model_vtx[M_GROUND], 4, 0);
            quad_tris();
            gsSPGeometryMode(G_FOG, 0);
            gsDPPipeSync();
            gsDPSetCycleType(G_CYC_1CYCLE);
            gsDPSetRenderMode(RM_ZB_OPA_SURF(1), RM_ZB_OPA_SURF(2));
            cc_shade();
            gsSPPopMatrixN(1);
        }
        for (i = 0; i < sc->nobj[k]; i++) {
            const Obj *o = &sc->obj[k][i];
            float pos[3];
            int j, shown;
            if (!expect || a <= 0.0f || a >= 1.0f) {
                shown = a < 0.5f ? o->in0 : o->in1;
                for (j = 0; j < 3; j++)
                    pos[j] = a < 0.5f ? o->p0[j] : o->p1[j];
                if (expect == 0 && a > 0.0f && a < 1.0f)   /* a frame of the game at a moment between */
                    for (j = 0; j < 3; j++)
                        pos[j] = lerpf(o->p0[j], o->p1[j], a);
            } else {   /* the frame between is drawn from the nearer frame's: B's up to halfway, then C's */
                int from_c = a > 0.5f;
                shown = from_c ? o->in1 : o->in0;
                for (j = 0; j < 3; j++)
                    pos[j] = (o->in0 && o->in1 && !o->teleport) ? lerpf(o->p0[j], o->p1[j], a)
                                                                 : from_c ? o->p1[j] : o->p0[j];
            }
            if (!shown)
                continue;
            ident(f);
            if (o->rot0 != 0.0f || o->rot1 != 0.0f) {   /* the nearer frame's turn (a frame: its own) */
                float r = (a > 0.5f ? o->rot1 : o->rot0) * 3.14159265f / 180.0f;
                f[0][0] = cosf(r), f[0][1] = sinf(r), f[1][0] = -sinf(r), f[1][1] = cosf(r);
            }
            f[3][0] = pos[0] - eye[0], f[3][1] = pos[1] - eye[1], f[3][2] = pos[2] - eye[2];
            gsSPMatrix((uintptr_t)new_mtx(buf, f), G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
            if (o->model == M_TEX) {
                gsDPPipeSync();
                cc_decalrgba();
                gsSPTexture(0xFFFF, 0xFFFF, 0, 0, 1);
                gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, s_tex_addr);
                gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
                gsDPLoadSync();
                gsDPLoadBlock(G_TX_LOADTILE, 0, 0, 16 * 16 - 1, calc_dxt(16, 2));
                gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 0, 0, 0, G_TX_CLAMP, 4, 0, G_TX_CLAMP, 4, 0);
                gsDPSetTileSize(0, 0, 0, 15 << 2, 15 << 2);
                gsSPGeometryMode(0, G_CULL_BACK);
            }
            gsSPVertex(s_model_vtx[o->model], 4, 0);
            quad_tris();
            if (o->model == M_TEX) {
                gsSPTexture(0xFFFF, 0xFFFF, 0, 0, 0);
                gsSPGeometryMode(G_CULL_BACK, 0);
                gsDPPipeSync();
                cc_shade();
            }
            for (j = 0; j < o->children; j++) {   /* pushed on the parent's, as the scene walk does */
                ident(f);
                f[3][0] = ((float)j - 1.5f) * o->child_gap, f[3][1] = -14.0f;
                gsSPMatrix((uintptr_t)new_mtx(buf, f), G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
                gsSPVertex(s_model_vtx[o->child_model], 4, 0);
                quad_tris();
                gsSPPopMatrixN(1);
            }
            gsSPPopMatrixN(1);
        }
        if (sc->dyn && k == 0) {   /* eye-relative native vertices, as the game's skid marks are */
            uint8_t *v = sc->dyn_fixed ? s_dynv_fixed : s_dynv[buf];
            Vtx *q = (Vtx *)(void *)v;
            int x = (int)lrintf(sc->dyn_p[0] - eye[0]), y = (int)lrintf(sc->dyn_p[1] - eye[1]),
                z = (int)lrintf(sc->dyn_p[2] - eye[2]);
            vtx(&q[0], x - 8, y - 8, z, 0, 0, 255, 0, 255, 255);
            vtx(&q[1], x + 8, y - 8, z, 0, 0, 255, 0, 255, 255);
            vtx(&q[2], x - 8, y + 8, z, 0, 0, 255, 0, 255, 255);
            vtx(&q[3], x + 8, y + 8, z, 0, 0, 255, 0, 255, 255);
            gsSPVertex((uintptr_t)v, 4, 0);
            quad_tris();
        }
    }
    if (sc->ortho) {   /* an orthographic triangle, as a 2D overlay drawn with triangles: C's */
        float x = expect && a > 0.0f && a < 1.0f ? (a > 0.5f ? sc->ortho_x1 : sc->ortho_x0)
                                                  : lerpf(sc->ortho_x0, sc->ortho_x1, a);
        static Vtx tri[2][3];
        Vtx *t = tri[buf];
        gsSPViewport((uintptr_t)&s_vp[buf][0]);
        gsDPSetScissor(0, 0, FB_W, FB_H);
        f[0][0] = 2.0f / FB_W, f[1][1] = -2.0f / FB_H, f[2][2] = -1.0f / 1000.0f;
        f[0][1] = f[0][2] = f[0][3] = f[1][0] = f[1][2] = f[1][3] = f[2][0] = f[2][1] = f[2][3] = 0.0f;
        f[3][0] = -1.0f, f[3][1] = 1.0f, f[3][2] = 0.0f, f[3][3] = 1.0f;
        gsSPMatrix((uintptr_t)new_mtx(buf, f), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        ident(f);
        gsSPMatrix((uintptr_t)new_mtx(buf, f), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        gsSPGeometryMode(G_ZBUFFER, 0);
        vtx(&t[0], (int)x, 200, 0, 0, 0, 255, 255, 0, 255);
        vtx(&t[1], (int)x + 20, 200, 0, 0, 0, 255, 255, 0, 255);
        vtx(&t[2], (int)x, 220, 0, 0, 0, 255, 255, 0, 255);
        gsSPVertex((uintptr_t)t, 3, 0);
        gsSP1Triangle(0, 1, 2);
    }
    if (sc->hud) {   /* a texture rectangle: C's */
        int x = expect && a > 0.0f && a < 1.0f ? (a > 0.5f ? sc->hud_x1 : sc->hud_x0)
                                                : (int)lrintf(lerpf((float)sc->hud_x0, (float)sc->hud_x1, a));
        gsDPPipeSync();
        gsDPSetCycleType(G_CYC_COPY);
        gsDPSetRenderMode(0, 0);
        gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, s_tex_addr);
        gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
        gsDPLoadSync();
        gsDPLoadBlock(G_TX_LOADTILE, 0, 0, 16 * 16 - 1, calc_dxt(16, 2));
        gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 0, 0, 0, G_TX_CLAMP, 4, 0, G_TX_CLAMP, 4, 0);
        gsDPSetTileSize(0, 0, 0, 15 << 2, 15 << 2);
        gsSPTextureRectangle(x << 2, 10 << 2, (x + 15) << 2, 25 << 2, 0, 0, 0, 4 << 10, 1 << 10);
    }
    gsDPFullSync();
    gsSPEndDisplayList();
}

/* ---------------------------------------------------------------- running and comparing */

static uint8_t *read_window(void) {
    int w, h;
    uint8_t *px;
    if (pc_gfx_read_window(NULL, &w, &h) != 0 || w != WIN_W || h != WIN_H) {
        printf("  FAIL %s: no %dx%d window image (%dx%d)\n", s_case, WIN_W, WIN_H, w, h);
        s_fail++;
        return (uint8_t *)calloc(WIN_W * WIN_H, 4);
    }
    px = (uint8_t *)malloc((size_t)w * h * 4);
    pc_gfx_read_window(px, &w, &h);
    return px;
}

static void save(const uint8_t *px, const char *what) {
    char path[512];
    if (s_png_dir == NULL)
        return;
    snprintf(path, sizeof(path), "%s/interp-%s-%s.png", s_png_dir, s_case, what);
    pcgfx_png_write(path, px, WIN_W, WIN_H);
}

/* pixels whose colour differs by more than tol in a channel */
static int count_diff(const uint8_t *a, const uint8_t *b, int tol) {
    int i, n = 0;
    for (i = 0; i < WIN_W * WIN_H; i++) {
        const uint8_t *p = a + 4 * i, *q = b + 4 * i;
        if (abs(p[0] - q[0]) > tol || abs(p[1] - q[1]) > tol || abs(p[2] - q[2]) > tol)
            n++;
    }
    return n;
}

static void expect_match(const char *what, const uint8_t *got, const uint8_t *want, int max_px) {
    int n = count_diff(got, want, 8);
    if (n > max_px) {
        printf("  FAIL %s %s: %d pixels differ from the reference (at most %d)\n", s_case, what, n, max_px);
        s_fail++;
        save(got, what);
        save(want, "reference");
    }
}

/* The centroid, in window pixels, of the pixels near colour (r, g, b); 0 if none. In window pixels, so that the
 * checks hold in every variant (scale, widescreen): they compare places, not N64 coordinates. */
static int centroid(const uint8_t *px, int r, int g, int b, float *cx, float *cy) {
    double sx = 0, sy = 0;
    int x, y, n = 0;
    for (y = 0; y < WIN_H; y++)
        for (x = 0; x < WIN_W; x++) {
            const uint8_t *p = px + ((size_t)y * WIN_W + x) * 4;
            if (abs(p[0] - r) < 24 && abs(p[1] - g) < 24 && abs(p[2] - b) < 24)
                sx += x, sy += y, n++;
        }
    if (n == 0)
        return 0;
    *cx = (float)(sx / n);
    *cy = (float)(sy / n);
    return n;
}

static void expect_near(const char *what, float got, float want, float tol) {
    if (fabsf(got - want) > tol) {
        printf("  FAIL %s %s: %.2f, want %.2f (+-%.2f)\n", s_case, what, got, want, tol);
        s_fail++;
    }
}

/* Draws B (buffer 0, swapped in at retrace 10) and C (buffer 1, drawn, not swapped in), at the rate given. */
static void pair(const Scene *sc, int rate) {
    pc_gfx_set_frame_rate(0);
    pc_gfx_set_frame_rate(rate);   /* from nothing recorded */
    build(sc, 0, 0.0f, 0);
    pc_gfx_retrace(10);
    pc_gfx_run_dl(s_list[0]);
    pc_gfx_present(s_fb[0]);
    build(sc, 1, 1.0f, 0);
    pc_gfx_run_dl(s_list[1]);
}

/* The picture at moment a drawn directly, the frame rate off: the window shows it at the present. */
static uint8_t *reference(const Scene *sc, float a, int expect) {
    pc_gfx_set_frame_rate(0);
    build(sc, 0, a, expect);
    pc_gfx_run_dl(s_list[0]);
    pc_gfx_present(s_fb[0]);
    return read_window();
}

static uint8_t *tick(double vt) {
    pc_gfx_present_tick(vt, 0);
    return read_window();
}

/* Interpolated at a against the scene's expected picture there. */
static void check_moment(const Scene *sc, float a, int max_px) {
    uint8_t *got, *want;
    char what[32];
    pair(sc, 120);
    got = tick(10.0 + 2.0 * a);
    want = reference(sc, sc->cut ? 0.0f : a, 1);
    snprintf(what, sizeof(what), "at %.2f", a);
    expect_match(what, got, want, max_px);
    free(got);
    free(want);
}

static void scene_init(Scene *sc, int views) {
    int k;
    memset(sc, 0, sizeof(*sc));
    sc->views = views;
    for (k = 0; k < MAX_VIEWS; k++) {
        sc->eye0[k][2] = sc->eye1[k][2] = 0.0f;
    }
}

static Obj *add_obj(Scene *sc, int view, int model, float x0, float y0, float z0, float x1, float y1, float z1) {
    Obj *o = &sc->obj[view][sc->nobj[view]++];
    memset(o, 0, sizeof(*o));
    o->model = model;
    o->p0[0] = x0, o->p0[1] = y0, o->p0[2] = z0;
    o->p1[0] = x1, o->p1[1] = y1, o->p1[2] = z1;
    o->in0 = o->in1 = 1;
    return o;
}

/* Objects that stay put in the world, each a model of its own, so the camera's move can be measured from them (as
 * the track's pieces). */
static void add_static(Scene *sc, int view) {
    add_obj(sc, view, M_S0, -70, 30, -300, -70, 30, -300);
    add_obj(sc, view, M_S1, 70, 30, -300, 70, 30, -300);
    add_obj(sc, view, M_S2, -50, -30, -350, -50, -30, -350);
    add_obj(sc, view, M_S3, 50, -30, -350, 50, -30, -350);
}

/* ---------------------------------------------------------------- cases */


/* Identical frames: the frame drawn between must be the game's frame to the pixel (the replay is exact), with
 * fog, the near plane, a texture, culling, depth, 2D and a split view. */
static void case_exact(void) {
    Scene sc;
    uint8_t *got, *want;
    int ground;
    for (ground = 0; ground < 2; ground++) {
        /* With a ground through the eye plane, the frame between is clipped there exactly (gfx_rsp.c's robust
         * clipping) where the game's frame lerps w down to the plane in float: a few pixels on the clipped edges
         * may differ. Without it, the frame between is the game's frame to the pixel. */
        int allowed = ground ? 32 : 0, n;
        scene_init(&sc, 2);
        sc.extras = ground;
        add_static(&sc, 0);
        add_obj(&sc, 0, M_TEX, 0, 0, -150, 0, 0, -150);
        add_obj(&sc, 1, M_RED, 10, 0, -120, 10, 0, -120);
        sc.hud = 1, sc.hud_x0 = sc.hud_x1 = 40;
        sc.ortho = 1, sc.ortho_x0 = sc.ortho_x1 = 200;
        sc.yaw0[0] = sc.yaw1[0] = 10.0f;
        pair(&sc, 120);
        got = tick(11.0);
        want = reference(&sc, 1.0f, 0);
        n = count_diff(got, want, 0);
        if (n > allowed) {
            printf("  FAIL %s: the frame drawn between two identical ones is not the game's (%d pixels, at most %d)\n",
                   s_case, n, allowed);
            s_fail++;
            save(got, "between");
            save(want, "frame");
        }
        {
            PcGfxHfrStats st;
            pcgfx_hfr_stats(&st);
            if (st.xf_lerp == 0 || st.xf_snap == 0 || st.drawn == 0) {
                printf("  FAIL %s: transforms %u blended %u as drawn, %u frames drawn\n", s_case, st.xf_lerp,
                       st.xf_snap, st.drawn);
                s_fail++;
            }
        }
        free(got);
        free(want);
    }
}

/* An object moving across a still camera: a quarter, half and three quarters of the way. */
static void case_move(void) {
    Scene sc;
    uint8_t *b, *c, *m;
    float xb, yb, xc, yc, xm, ym;
    scene_init(&sc, 1);
    add_static(&sc, 0);
    add_obj(&sc, 0, M_RED, -40, 0, -200, 40, 10, -200);
    check_moment(&sc, 0.25f, 40);
    check_moment(&sc, 0.5f, 40);
    check_moment(&sc, 0.75f, 40);
    /* and where it is, measured: the middle of its two places on screen (same depth: exactly) */
    pair(&sc, 120);
    b = tick(10.0);
    m = tick(11.0);
    c = reference(&sc, 1.0f, 0);
    if (!centroid(b, 255, 0, 0, &xb, &yb) || !centroid(c, 255, 0, 0, &xc, &yc) ||
        !centroid(m, 255, 0, 0, &xm, &ym)) {
        printf("  FAIL %s: the red square is missing\n", s_case);
        s_fail++;
    } else {
        expect_near("x halfway", xm, 0.5f * (xb + xc), 1.2f);
        expect_near("y halfway", ym, 0.5f * (yb + yc), 1.2f);
        if (fabsf(xc - xb) < 20.0f) {
            printf("  FAIL %s: it hardly moved (%.1f)\n", s_case, xc - xb);
            s_fail++;
        }
    }
    free(b);
    free(c);
    free(m);
}

/* The camera turns (in the projection, as the game turns it) and moves (in every modelview). */
static void case_camera(void) {
    Scene sc;
    uint8_t *m, *t;
    float xm, ym, xt, yt;
    scene_init(&sc, 1);
    add_static(&sc, 0);
    add_obj(&sc, 0, M_RED, 0, 0, -250, 0, 0, -250);
    sc.yaw0[0] = -6.0f, sc.yaw1[0] = 6.0f;
    sc.eye0[0][0] = -15.0f, sc.eye1[0][0] = 15.0f;
    sc.eye0[0][2] = 10.0f, sc.eye1[0][2] = -10.0f;
    check_moment(&sc, 0.5f, 60);
    check_moment(&sc, 0.25f, 60);
    /* against the true turn (the interpolator blends the matrices): within a pixel */
    pair(&sc, 120);
    m = tick(11.0);
    t = reference(&sc, 0.5f, 0);
    if (!centroid(m, 255, 0, 0, &xm, &ym) || !centroid(t, 255, 0, 0, &xt, &yt)) {
        printf("  FAIL %s: the red square is missing\n", s_case);
        s_fail++;
    } else {
        expect_near("x against the true turn", xm, xt, 2.0f);
        expect_near("y against the true turn", ym, yt, 2.0f);
    }
    free(m);
    free(t);
}

/* A camera cut (a quarter turn): no frame between; B until C is due, then C. */
static void case_cut(void) {
    Scene sc;
    uint8_t *b, *m, *late, *c;
    PcGfxHfrStats s0, s1;
    scene_init(&sc, 1);
    add_static(&sc, 0);
    add_obj(&sc, 0, M_RED, 0, 0, -200, 0, 0, -200);
    sc.yaw0[0] = 0.0f, sc.yaw1[0] = 90.0f;
    add_obj(&sc, 0, M_GREEN, 200, 0, 0, 200, 0, 0);   /* to the side: in view after the turn */
    pair(&sc, 120);
    pcgfx_hfr_stats(&s0);
    b = tick(10.0);
    m = tick(11.0);
    pcgfx_hfr_stats(&s1);
    expect_match("between: B", m, b, 0);
    if (s1.drawn != s0.drawn) {
        printf("  FAIL %s: %u frames drawn across a cut\n", s_case, s1.drawn - s0.drawn);
        s_fail++;
    }
    late = tick(12.0);   /* due, not swapped in yet: C as the game drew it */
    c = reference(&sc, 1.0f, 0);
    expect_match("when C is due", late, c, 0);
    free(b);
    free(m);
    free(late);
    free(c);
}

/* A teleport (a car put back on the track) among moving and still objects: it is where B has it up to halfway,
 * then where C has it, not part of the way. */
static void case_teleport(void) {
    Scene sc;
    Obj *o;
    scene_init(&sc, 1);
    add_static(&sc, 0);
    add_obj(&sc, 0, M_RED, -20, 0, -200, 0, 0, -200);
    o = add_obj(&sc, 0, M_GREEN, -60, 0, -150, 60, 0, -450);
    o->teleport = 1;
    check_moment(&sc, 0.5f, 40);
    check_moment(&sc, 0.75f, 40);
    /* the same with the camera moving: the teleport is seen through the camera at that moment */
    sc.eye0[0][0] = -10.0f, sc.eye1[0][0] = 10.0f;
    check_moment(&sc, 0.25f, 60);
    check_moment(&sc, 0.75f, 60);
}

/* Split screen: 2 and 4 views, each with its own camera and objects moving their own ways. */
static void case_split(void) {
    Scene sc;
    int views, k;
    for (views = 2; views <= 4; views += 2) {
        scene_init(&sc, views);
        for (k = 0; k < views; k++) {
            float d = (k & 1) ? -1.0f : 1.0f;
            add_static(&sc, k);
            add_obj(&sc, k, M_RED, -30 * d, 0, -200, 30 * d, 0, -200);
            add_obj(&sc, k, M_GREEN, 0, -20 * d, -180, 0, 20 * d, -180);
            sc.yaw0[k] = 3.0f * k, sc.yaw1[k] = -3.0f * k;
            sc.eye0[k][1] = -5.0f * k, sc.eye1[k][1] = 5.0f * k;
        }
        check_moment(&sc, 0.5f, 80);
    }
}

/* 2D: a texture rectangle and an orthographic triangle that move are drawn as B has them up to halfway, then as C
 * has them, texel for texel, never part of the way; the 3D with them moves part of the way. */
static void case_hud(void) {
    Scene sc;
    int k;
    scene_init(&sc, 1);
    add_static(&sc, 0);
    add_obj(&sc, 0, M_RED, -40, 0, -200, 40, 0, -200);
    sc.hud = 1, sc.hud_x0 = 20, sc.hud_x1 = 60;
    sc.ortho = 1, sc.ortho_x0 = 150, sc.ortho_x1 = 250;
    check_moment(&sc, 0.25f, 40);
    check_moment(&sc, 0.5f, 40);
    check_moment(&sc, 0.75f, 40);
    for (k = 0; k < 2; k++) {   /* the rectangle where a frame has it */
        uint8_t *m, *f;
        float x, y, xf, yf;
        pair(&sc, 120);
        m = tick(k ? 11.5 : 10.5);
        f = reference(&sc, k ? 1.0f : 0.0f, 0);
        if (!centroid(m, 255, 0, 255, &x, &y) || !centroid(f, 255, 0, 255, &xf, &yf))
            printf("  FAIL %s: no rectangle\n", s_case), s_fail++;
        else
            expect_near(k ? "rectangle x at 0.75: C's" : "rectangle x at 0.25: B's", x, xf, 0.01f);
        free(m);
        free(f);
    }
}

/* An object only C has appears past halfway, where C has it (through the moving camera); one only B has stays
 * where B has it until halfway. */
static void case_appear(void) {
    Scene sc;
    Obj *o;
    scene_init(&sc, 1);
    add_static(&sc, 0);
    add_obj(&sc, 0, M_RED, -20, 0, -200, 20, 0, -200);
    o = add_obj(&sc, 0, M_GREEN, 0, 0, 0, 30, 30, -220);
    o->in0 = 0;
    o = add_obj(&sc, 0, M_YELLOW, -40, -30, -220, 0, 0, 0);
    o->in1 = 0;
    sc.eye0[0][0] = -12.0f, sc.eye1[0][0] = 12.0f;
    check_moment(&sc, 0.25f, 60);
    check_moment(&sc, 0.5f, 60);
    check_moment(&sc, 0.75f, 60);
}

/* Instances of one model (the same vertex data): the first gone in C, the others moved; each pairs with its own,
 * not with the next one along. */
static void case_instances(void) {
    Scene sc;
    Obj *o;
    scene_init(&sc, 1);
    add_static(&sc, 0);
    o = add_obj(&sc, 0, M_RED, -70, 0, -220, 0, 0, 0);
    o->in1 = 0;
    add_obj(&sc, 0, M_RED, -25, 0, -220, -18, 0, -220);
    add_obj(&sc, 0, M_RED, 20, 0, -220, 27, 0, -220);
    add_obj(&sc, 0, M_RED, 65, 0, -220, 72, 0, -220);
    sc.eye0[0][1] = -6.0f, sc.eye1[0][1] = 6.0f;
    check_moment(&sc, 0.5f, 60);
    check_moment(&sc, 0.75f, 60);
}

/* A wheel at speed: it turns 60 degrees a frame, too far to blend. Its place is blended, its turn is the nearer
 * frame's (blending the matrices would shrink it). */
static void case_spin(void) {
    Scene sc;
    Obj *o;
    scene_init(&sc, 1);
    add_static(&sc, 0);
    o = add_obj(&sc, 0, M_RED, -30, 0, -160, 30, 0, -160);
    o->rot0 = 0.0f, o->rot1 = 60.0f;
    check_moment(&sc, 0.25f, 60);
    check_moment(&sc, 0.75f, 60);
}

/* A car's wheels: four instances of one model under a parent the camera follows, as far apart as the camera moves
 * in a frame. Against the world they all moved by the camera's move, so nearness can't tell them apart; they pair
 * in their order under the matched parent. */
static void case_children(void) {
    Scene sc;
    Obj *o;
    scene_init(&sc, 1);
    add_static(&sc, 0);
    o = add_obj(&sc, 0, M_YELLOW, -12, 0, -160, 12, 0, -160);
    o->children = 4, o->child_model = M_RED, o->child_gap = 24.0f;
    sc.eye0[0][0] = -12.0f, sc.eye1[0][0] = 12.0f;   /* the camera follows it */
    check_moment(&sc, 0.25f, 60);
    check_moment(&sc, 0.5f, 60);
    check_moment(&sc, 0.75f, 60);
}

/* Eye-relative native vertices under the identity modelview (the game's skid marks): they stay put in the
 * world as the camera moves, in the frame's own buffer and at one address. */
static void case_eyerel(void) {
    Scene sc;
    int fixed;
    for (fixed = 0; fixed < 2; fixed++) {
        scene_init(&sc, 1);
        add_static(&sc, 0);
        add_obj(&sc, 0, M_RED, -30, 0, -200, -30, 0, -200);
        sc.dyn = 1, sc.dyn_fixed = fixed;
        sc.dyn_p[0] = 30, sc.dyn_p[1] = 0, sc.dyn_p[2] = -180;
        sc.eye0[0][0] = -16.0f, sc.eye1[0][0] = 16.0f;
        check_moment(&sc, 0.5f, 60);
    }
}

/* When frames are shown: the presented one at its retrace, frames between, the next one when due and not swapped
 * in (the game is late), then it at its retrace; a period of 3; the screenshot is always the game's frame; a redraw
 * shows the frame between again; off, the ticks show nothing and the present shows the frame. */
static void case_timing(void) {
    Scene sc;
    uint8_t *b, *m, *m2, *early, *c, *shot0, *shot1, *red, *w0, *w1;
    PcGfxHfrStats s0, s1;
    int w, h;
    scene_init(&sc, 1);
    add_static(&sc, 0);
    add_obj(&sc, 0, M_RED, -40, 0, -200, 40, 0, -200);
    pair(&sc, 120);
    pcgfx_hfr_stats(&s0);
    b = tick(10.0);
    pcgfx_gl_read_presented(NULL, &w, &h);
    shot0 = (uint8_t *)malloc((size_t)w * h * 4);
    shot1 = (uint8_t *)malloc((size_t)w * h * 4);
    pcgfx_gl_read_presented(shot0, &w, &h);
    m = tick(11.0);
    pcgfx_gl_read_presented(shot1, &w, &h);
    if (memcmp(shot0, shot1, (size_t)w * h * 4) != 0)
        printf("  FAIL %s: the presented image (screenshots) changed with a frame drawn between\n", s_case), s_fail++;
    pc_gfx_redraw();
    red = read_window();
    expect_match("redraw", red, m, 0);
    m2 = tick(11.5);
    early = tick(12.5);
    c = reference(&sc, 1.0f, 0);
    expect_match("late: the next frame as drawn", early, c, 0);
    pcgfx_hfr_stats(&s1);
    if (s1.ticks - s0.ticks != 4 || s1.held - s0.held != 1 || s1.drawn - s0.drawn != 2 || s1.early - s0.early != 1)
        printf("  FAIL %s: shown %u, held %u, drawn %u, early %u (want 4, 1, 2, 1)\n", s_case, s1.ticks - s0.ticks,
               s1.held - s0.held, s1.drawn - s0.drawn, s1.early - s0.early), s_fail++;
    /* a period of 3: C swapped in at 13 after B at 10; D drawn; its frames between are a third of the way */
    pair(&sc, 120);
    pc_gfx_retrace(13);
    pc_gfx_present(s_fb[1]);
    build(&sc, 0, 0.0f, 0);   /* D: the scene at B's again */
    pc_gfx_run_dl(s_list[0]);
    free(m2);
    m2 = tick(14.5);   /* half of 3 */
    {
        float xc, yc, xd, yd, xm, ym;
        uint8_t *cc = tick(13.0), *dd;
        dd = reference(&sc, 0.0f, 0);
        if (!centroid(cc, 255, 0, 0, &xc, &yc) || !centroid(dd, 255, 0, 0, &xd, &yd) ||
            !centroid(m2, 255, 0, 0, &xm, &ym))
            printf("  FAIL %s: the red square is missing\n", s_case), s_fail++;
        else
            expect_near("x halfway over 3 retraces", xm, 0.5f * (xc + xd), 1.2f);
        free(cc);
        free(dd);
    }
    /* off: a tick shows nothing, the present shows the frame */
    pc_gfx_set_frame_rate(0);
    build(&sc, 0, 0.0f, 0);
    pc_gfx_run_dl(s_list[0]);
    pc_gfx_present(s_fb[0]);
    w0 = read_window();
    build(&sc, 1, 1.0f, 0);
    pc_gfx_run_dl(s_list[1]);
    pc_gfx_present_tick(10.5, 0);
    w1 = read_window();
    expect_match("off: a tick changes nothing", w1, w0, 0);
    free(w1);
    pc_gfx_present(s_fb[1]);
    w1 = read_window();
    if (count_diff(w0, w1, 8) == 0)
        printf("  FAIL %s: off, the present didn't show the next frame\n", s_case), s_fail++;
    free(b);
    free(m);
    free(m2);
    free(red);
    free(early);
    free(c);
    free(shot0);
    free(shot1);
    free(w0);
    free(w1);
}

/* The setting's text. */
static void case_parse(void) {
    static const struct {
        const char *text;
        int ok, value;
    } t[] = {{"original", 1, 0}, {"off", 1, 0}, {"0", 1, 0}, {"display", 1, -1}, {"60", 1, 60}, {"120", 1, 120},
             {"144", 1, 144}, {"fast", 0, 0}, {"-5", 0, 0}, {"60hz", 0, 0}, {"1001", 0, 0}};
    unsigned i;
    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        int ok, v = pc_gfx_parse_frame_rate(t[i].text, &ok);
        if (ok != t[i].ok || (ok && v != t[i].value))
            printf("  FAIL %s: '%s' gave %d (ok %d)\n", s_case, t[i].text, v, ok), s_fail++;
    }
    pc_gfx_set_frame_rate(-1);
    if (pc_gfx_frame_rate() != 60)   /* headless: no display, 60 */
        printf("  FAIL %s: display without one is %d, not 60\n", s_case, pc_gfx_frame_rate()), s_fail++;
    pc_gfx_set_frame_rate(144);
    if (pc_gfx_frame_rate() != 144)
        printf("  FAIL %s: 144 is %d\n", s_case, pc_gfx_frame_rate()), s_fail++;
    pc_gfx_set_frame_rate(0);
    if (pc_gfx_frame_rate() != 0)
        printf("  FAIL %s: off is %d\n", s_case, pc_gfx_frame_rate()), s_fail++;
}

static const struct {
    const char *name;
    void (*fn)(void);
} s_cases[] = {
    {"exact", case_exact},   {"move", case_move},           {"camera", case_camera}, {"cut", case_cut},
    {"teleport", case_teleport}, {"split", case_split},     {"hud", case_hud},       {"appear", case_appear},
    {"instances", case_instances}, {"children", case_children}, {"spin", case_spin}, {"eyerel", case_eyerel},
    {"timing", case_timing}, {"parse", case_parse},
};

int main(int argc, char **argv) {
    const char *which = argc > 1 ? argv[1] : "all";
    int i, ran = 0, failed = 0;
    for (i = 2; i < argc; i++)
        if (strcmp(argv[i], "--png") == 0 && i + 1 < argc)
            s_png_dir = argv[++i];
    s_rdram = (uint8_t *)calloc(1, PC_RDRAM_SIZE);
    if (pc_gfx_init(1, WIN_W, WIN_H) != 0) {
        printf("FAIL: pc_gfx_init\n");
        return 2;
    }
    make_models();
    for (i = 0; i < (int)(sizeof(s_cases) / sizeof(s_cases[0])); i++) {
        int before = s_fail;
        if (strcmp(which, "all") != 0 && strcmp(which, s_cases[i].name) != 0)
            continue;
        s_case = s_cases[i].name;
        s_cases[i].fn();
        ran++;
        if (s_fail != before)
            failed++;
        printf("%s %s\n", s_fail != before ? "FAIL" : "ok  ", s_case);
    }
    pc_gfx_set_frame_rate(0);
    pc_gfx_shutdown();
    free(s_rdram);
    if (ran == 0) {
        printf("no case named %s\n", which);
        return 2;
    }
    printf("%d/%d cases passed\n", ran - failed, ran);
    return failed ? 1 : 0;
}
