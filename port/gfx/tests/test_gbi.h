/* port/gfx/tests/test_gbi.h: test-local F3DEX2 command encoders, written from the SDK's documented formats
 * (independent of the renderer's decoder). Commands go to the current emitter: a native PcGfxWords list, or an
 * N64-form big-endian byte list. */
#ifndef TEST_GBI_H
#define TEST_GBI_H

#include <stdint.h>
#include <string.h>

#include "pc_port.h"

#define SHL(v, s, w) ((uint32_t)(((uint32_t)(v) & ((1u << (w)) - 1)) << (s)))

/* ---------------------------------------------------------------- emitter */

static PcGfxWords *gp;   /* native */
static uint8_t *bp;      /* N64 form */
static int g_be_mode;

static void cmd(uint32_t w0, uintptr_t w1) {
    if (g_be_mode) {
        uint32_t a = (uint32_t)w1;
        bp[0] = w0 >> 24; bp[1] = w0 >> 16; bp[2] = w0 >> 8; bp[3] = w0;
        bp[4] = a >> 24; bp[5] = a >> 16; bp[6] = a >> 8; bp[7] = a;
        bp += 8;
    } else {
        gp->w0 = w0;
        gp->w1 = w1;
        gp++;
    }
}

/* ---------------------------------------------------------------- constants */

enum {
    G_IM_FMT_RGBA, G_IM_FMT_YUV, G_IM_FMT_CI, G_IM_FMT_IA, G_IM_FMT_I,
};
enum { G_IM_SIZ_4b, G_IM_SIZ_8b, G_IM_SIZ_16b, G_IM_SIZ_32b };
#define G_TX_LOADTILE 7
#define G_TX_RENDERTILE 0
#define G_TX_NOMIRROR 0
#define G_TX_WRAP 0
#define G_TX_MIRROR 1
#define G_TX_CLAMP 2
#define G_TX_NOMASK 0
#define G_TX_NOLOD 0

#define G_ZBUFFER 0x00000001
#define G_SHADE 0x00000004
#define G_CULL_FRONT 0x00000200
#define G_CULL_BACK 0x00000400
#define G_CULL_BOTH 0x00000600
#define G_FOG 0x00010000
#define G_LIGHTING 0x00020000
#define G_TEXTURE_GEN 0x00040000
#define G_SHADING_SMOOTH 0x00200000

#define G_MTX_MODELVIEW 0x00
#define G_MTX_PROJECTION 0x04
#define G_MTX_MUL 0x00
#define G_MTX_LOAD 0x02
#define G_MTX_NOPUSH 0x00
#define G_MTX_PUSH 0x01

#define G_CYC_1CYCLE (0u << 20)
#define G_CYC_2CYCLE (1u << 20)
#define G_CYC_COPY (2u << 20)
#define G_CYC_FILL (3u << 20)
#define G_TT_NONE 0
#define G_TT_RGBA16 (2u << 14)
#define G_TT_IA16 (3u << 14)
#define G_TF_POINT 0
#define G_TL_TILE 0
#define G_TL_LOD (1u << 16)
#define G_TF_BILERP (2u << 12)
#define G_AC_NONE 0
#define G_AC_THRESHOLD 1
#define G_ZS_PIXEL 0
#define G_ZS_PRIM 4

/* render mode pieces */
#define AA_EN 0x8
#define Z_CMP 0x10
#define Z_UPD 0x20
#define IM_RD 0x40
#define CLR_ON_CVG 0x80
#define CVG_DST_CLAMP 0
#define CVG_DST_WRAP 0x100
#define CVG_DST_FULL 0x200
#define ZMODE_OPA 0
#define ZMODE_XLU 0x800
#define ZMODE_DEC 0xC00
#define CVG_X_ALPHA 0x1000
#define ALPHA_CVG_SEL 0x2000
#define FORCE_BL 0x4000
#define G_BL_CLR_IN 0
#define G_BL_CLR_MEM 1
#define G_BL_CLR_BL 2
#define G_BL_CLR_FOG 3
#define G_BL_1MA 0
#define G_BL_A_MEM 1
#define G_BL_A_IN 0
#define G_BL_A_FOG 1
#define G_BL_A_SHADE 2
#define G_BL_1 2
#define G_BL_0 3
#define GBL_c1(m1a, m1b, m2a, m2b) ((m1a) << 30 | (m1b) << 26 | (m2a) << 22 | (m2b) << 18)
#define GBL_c2(m1a, m1b, m2a, m2b) ((m1a) << 28 | (m1b) << 24 | (m2a) << 20 | (m2b) << 16)
#define RM_OPA_SURF(c) (CVG_DST_CLAMP | FORCE_BL | ZMODE_OPA | GBL_c##c(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1))
#define RM_ZB_OPA_SURF(c) (Z_CMP | Z_UPD | CVG_DST_FULL | ALPHA_CVG_SEL | ZMODE_OPA | \
                           GBL_c##c(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM))
#define RM_XLU_SURF(c) (IM_RD | CVG_DST_FULL | FORCE_BL | ZMODE_OPA | \
                        GBL_c##c(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA))
#define RM_AA_ZB_TEX_EDGE(c) (AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_CLAMP | CVG_X_ALPHA | ALPHA_CVG_SEL | \
                              ZMODE_OPA | GBL_c##c(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM))
#define G_RM_FOG_SHADE_A GBL_c1(G_BL_CLR_FOG, G_BL_A_SHADE, G_BL_CLR_IN, G_BL_1MA)

/* combiner inputs */
enum {
    CC_COMBINED = 0, CC_TEXEL0 = 1, CC_TEXEL1 = 2, CC_PRIMITIVE = 3, CC_SHADE = 4, CC_ENVIRONMENT = 5,
    CC_1 = 6, CC_TEXEL0_ALPHA = 8, CC_PRIM_ALPHA = 10, CC_SHADE_ALPHA = 11, CC_ENV_ALPHA = 12,
    CC_LOD_FRACTION = 13, CC_PRIM_LOD_FRAC = 14, CC_0 = 31,
};
enum { AC_COMBINED = 0, AC_TEXEL0 = 1, AC_TEXEL1 = 2, AC_PRIMITIVE = 3, AC_SHADE = 4, AC_ENVIRONMENT = 5,
       AC_1 = 6, AC_0 = 7, AC_LOD_FRACTION = 0 };

/* ---------------------------------------------------------------- RSP */

static void gsSPVertex(uintptr_t v, int n, int v0) {
    cmd(SHL(0x01, 24, 8) | SHL(n, 12, 8) | SHL(v0 + n, 1, 7), v);
}
static uint32_t tri_w(int a, int b, int c) {
    return SHL(a * 2, 16, 8) | SHL(b * 2, 8, 8) | SHL(c * 2, 0, 8);
}
static void gsSP1Triangle(int a, int b, int c) {
    cmd(SHL(0x05, 24, 8) | tri_w(a, b, c), 0);
}
static void gsSP2Triangles(int a, int b, int c, int d, int e, int f) {
    cmd(SHL(0x06, 24, 8) | tri_w(a, b, c), tri_w(d, e, f));
}
static void gsSPQuadrangle(int a, int b, int c, int d) {
    cmd(SHL(0x07, 24, 8) | tri_w(a, b, c), tri_w(a, c, d));
}
static void gDma2p(int c, uintptr_t adrs, int len, int idx, int ofs) {
    cmd(SHL(c, 24, 8) | SHL((len - 1) / 8, 19, 5) | SHL(ofs / 8, 8, 8) | SHL(idx, 0, 8), adrs);
}
static void gsSPMatrix(uintptr_t m, int p) {
    gDma2p(0xDA, m, 64, p ^ G_MTX_PUSH, 0);
}
static void gsSPPopMatrixN(int n) {
    gDma2p(0xD8, (uintptr_t)(n * 64), 64, 2, 0);
}
static void gsSPViewport(uintptr_t v) {
    gDma2p(0xDC, v, 16, 8, 0);
}
static void gsSPLight(uintptr_t l, int n) {
    gDma2p(0xDC, l, 16, 10, n * 24 + 24);
}
static void gsSPLookAtX(uintptr_t l) {
    gDma2p(0xDC, l, 16, 10, 0);
}
static void gsSPLookAtY(uintptr_t l) {
    gDma2p(0xDC, l, 16, 10, 24);
}
static void gMoveWd(int index, int offset, uint32_t data) {
    cmd(SHL(0xDB, 24, 8) | SHL(index, 16, 8) | SHL(offset, 0, 16), data);
}
static void gsSPNumLights(int n) {
    gMoveWd(2, 0, (uint32_t)(n * 24));
}
static void gsSPSegment(int seg, uintptr_t base) {
    cmd(SHL(0xDB, 24, 8) | SHL(6, 16, 8) | SHL(seg * 4, 0, 16), base);
}
static void gsSPFogPosition(int min, int max) {
    int fm = 128000 / (max - min), fo = (500 - min) * 256 / (max - min);
    gMoveWd(8, 0, SHL(fm, 16, 16) | SHL(fo, 0, 16));
}
static void gsSPTexture(int s, int t, int level, int tile, int on) {
    cmd(SHL(0xD7, 24, 8) | SHL(0, 16, 8) | SHL(level, 11, 3) | SHL(tile, 8, 3) | SHL(on, 1, 7),
        SHL(s, 16, 16) | SHL(t, 0, 16));
}
static void gsSPGeometryMode(uint32_t c, uint32_t s) {
    cmd(SHL(0xD9, 24, 8) | SHL(~c, 0, 24), s);
}
static void gsSPDisplayList(uintptr_t dl) {
    cmd(SHL(0xDE, 24, 8) | SHL(0, 16, 8), dl);
}
static void gsSPBranchList(uintptr_t dl) {
    cmd(SHL(0xDE, 24, 8) | SHL(1, 16, 8), dl);
}
static void gsSPEndDisplayList(void) {
    cmd(SHL(0xDF, 24, 8), 0);
}
static void gsSPCullDisplayList(int v0, int vn) {
    cmd(SHL(0x03, 24, 8) | SHL(v0 * 2, 0, 16), SHL(vn * 2, 0, 16));
}
static void gsSPBranchLessZraw(uintptr_t dl, int v, uint32_t zval) {
    cmd(SHL(0xE1, 24, 8), dl);
    cmd(SHL(0x04, 24, 8) | SHL(v * 5, 12, 12) | SHL(v * 2, 0, 12), zval);
}
static void gsSPModifyVertex(int vtx, int where, uint32_t val) {
    cmd(SHL(0x02, 24, 8) | SHL(where, 16, 8) | SHL(vtx * 2, 0, 16), val);
}
static void gsSPNoOp(uint32_t low) {
    cmd(SHL(0xE0, 24, 8) | (low & 0xFFFFFF), 0x1234);
}
static void gsSPSetOtherMode(int c, int sft, int len, uint32_t data) {
    cmd(SHL(c, 24, 8) | SHL(32 - sft - len, 8, 8) | SHL(len - 1, 0, 8), data);
}

/* ---------------------------------------------------------------- RDP */

static void gsDPPipeSync(void) { cmd(SHL(0xE7, 24, 8), 0); }
static void gsDPLoadSync(void) { cmd(SHL(0xE6, 24, 8), 0); }
static void gsDPTileSync(void) { cmd(SHL(0xE8, 24, 8), 0); }
static void gsDPFullSync(void) { cmd(SHL(0xE9, 24, 8), 0); }
static void gsDPSetCycleType(uint32_t t) { gsSPSetOtherMode(0xE3, 20, 2, t); }
static void gsDPSetTextureLUT(uint32_t t) { gsSPSetOtherMode(0xE3, 14, 2, t); }
static void gsDPSetTextureFilter(uint32_t t) { gsSPSetOtherMode(0xE3, 12, 2, t); }
static void gsDPSetTexturePersp(uint32_t t) { gsSPSetOtherMode(0xE3, 19, 1, t); }
static void gsDPSetTextureLOD(uint32_t t) { gsSPSetOtherMode(0xE3, 16, 1, t); }
static void gsDPSetAlphaCompare(uint32_t t) { gsSPSetOtherMode(0xE2, 0, 2, t); }
static void gsDPSetDepthSource(uint32_t t) { gsSPSetOtherMode(0xE2, 2, 1, t); }
static void gsDPSetRenderMode(uint32_t c0, uint32_t c1) { gsSPSetOtherMode(0xE2, 3, 29, c0 | c1); }
static void gsDPSetCombineLERP(int a0, int b0, int c0, int d0, int Aa0, int Ab0, int Ac0, int Ad0,
                               int a1, int b1, int c1, int d1, int Aa1, int Ab1, int Ac1, int Ad1) {
    uint32_t w0 = SHL(0xFC, 24, 8) | SHL(a0, 20, 4) | SHL(c0, 15, 5) | SHL(Aa0, 12, 3) | SHL(Ac0, 9, 3) |
                  SHL(a1, 5, 4) | SHL(c1, 0, 5);
    uint32_t w1 = SHL(b0, 28, 4) | SHL(d0, 15, 3) | SHL(Ab0, 12, 3) | SHL(Ad0, 9, 3) | SHL(b1, 24, 4) |
                  SHL(Aa1, 21, 3) | SHL(Ac1, 18, 3) | SHL(d1, 6, 3) | SHL(Ab1, 3, 3) | SHL(Ad1, 0, 3);
    cmd(w0, w1);
}
/* one mode for both cycles */
static void cc1(int a, int b, int c, int d, int Aa, int Ab, int Ac, int Ad) {
    gsDPSetCombineLERP(a, b, c, d, Aa, Ab, Ac, Ad, a, b, c, d, Aa, Ab, Ac, Ad);
}
static void cc_shade(void) { cc1(CC_0, CC_0, CC_0, CC_SHADE, AC_0, AC_0, AC_0, AC_SHADE); }
static void cc_prim(void) { cc1(CC_0, CC_0, CC_0, CC_PRIMITIVE, AC_0, AC_0, AC_0, AC_PRIMITIVE); }
static void cc_decalrgba(void) { cc1(CC_0, CC_0, CC_0, CC_TEXEL0, AC_0, AC_0, AC_0, AC_TEXEL0); }
static void cc_texalpha(void) { cc1(CC_1, CC_0, CC_TEXEL0_ALPHA, CC_0, AC_0, AC_0, AC_0, AC_1); }

static void setcolor(int c, uint32_t w0low, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    cmd(SHL(c, 24, 8) | w0low, (uint32_t)r << 24 | (uint32_t)g << 16 | (uint32_t)b << 8 | a);
}
static void gsDPSetPrimColor(int m, int l, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    setcolor(0xFA, SHL(m, 8, 8) | SHL(l, 0, 8), r, g, b, a);
}
static void gsDPSetEnvColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { setcolor(0xFB, 0, r, g, b, a); }
static void gsDPSetFogColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { setcolor(0xF8, 0, r, g, b, a); }
static void gsDPSetBlendColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { setcolor(0xF9, 0, r, g, b, a); }
static void gsDPSetFillColor(uint32_t c) { cmd(SHL(0xF7, 24, 8), c); }
static void gsDPSetPrimDepth(int z, int dz) { cmd(SHL(0xEE, 24, 8), SHL(z, 16, 16) | SHL(dz, 0, 16)); }
static void setimg(int c, int fmt, int siz, int width, uintptr_t img) {
    cmd(SHL(c, 24, 8) | SHL(fmt, 21, 3) | SHL(siz, 19, 2) | SHL(width - 1, 0, 12), img);
}
static void gsDPSetColorImage(int fmt, int siz, int w, uintptr_t img) { setimg(0xFF, fmt, siz, w, img); }
static void gsDPSetTextureImage(int fmt, int siz, int w, uintptr_t img) { setimg(0xFD, fmt, siz, w, img); }
static void gsDPSetDepthImage(uintptr_t img) { cmd(SHL(0xFE, 24, 8), img); }
static void gsDPSetScissor(int ulx, int uly, int lrx, int lry) {
    cmd(SHL(0xED, 24, 8) | SHL(ulx * 4, 12, 12) | SHL(uly * 4, 0, 12), SHL(lrx * 4, 12, 12) | SHL(lry * 4, 0, 12));
}
static void gsDPFillRectangle(int ulx, int uly, int lrx, int lry) {
    cmd(SHL(0xF6, 24, 8) | SHL(lrx, 14, 10) | SHL(lry, 2, 10), SHL(ulx, 14, 10) | SHL(uly, 2, 10));
}
static void gsDPSetTile(int fmt, int siz, int line, int tmem, int tile, int palette, int cmt, int maskt,
                        int shiftt, int cms, int masks, int shifts) {
    cmd(SHL(0xF5, 24, 8) | SHL(fmt, 21, 3) | SHL(siz, 19, 2) | SHL(line, 9, 9) | SHL(tmem, 0, 9),
        SHL(tile, 24, 3) | SHL(palette, 20, 4) | SHL(cmt, 18, 2) | SHL(maskt, 14, 4) | SHL(shiftt, 10, 4) |
        SHL(cms, 8, 2) | SHL(masks, 4, 4) | SHL(shifts, 0, 4));
}
static void gsDPSetTileSize(int t, int uls, int ult, int lrs, int lrt) {
    cmd(SHL(0xF2, 24, 8) | SHL(uls, 12, 12) | SHL(ult, 0, 12), SHL(t, 24, 3) | SHL(lrs, 12, 12) | SHL(lrt, 0, 12));
}
static void gsDPLoadTile(int t, int uls, int ult, int lrs, int lrt) {
    cmd(SHL(0xF4, 24, 8) | SHL(uls, 12, 12) | SHL(ult, 0, 12), SHL(t, 24, 3) | SHL(lrs, 12, 12) | SHL(lrt, 0, 12));
}
static void gsDPLoadBlock(int t, int uls, int ult, int lrs, int dxt) {
    cmd(SHL(0xF3, 24, 8) | SHL(uls, 12, 12) | SHL(ult, 0, 12), SHL(t, 24, 3) | SHL(lrs, 12, 12) | SHL(dxt, 0, 12));
}
static void gsDPLoadTLUTCmd(int t, int count) {
    cmd(SHL(0xF0, 24, 8), SHL(t, 24, 3) | SHL(count, 14, 10));
}
/* xl..xh in 10.2; s, t s10.5; dsdx, dtdy s5.10 */
static void texrect(int op, int xl, int yl, int xh, int yh, int tile, int s, int t, int dsdx, int dtdy) {
    cmd(SHL(op, 24, 8) | SHL(xh, 12, 12) | SHL(yh, 0, 12), SHL(tile, 24, 3) | SHL(xl, 12, 12) | SHL(yl, 0, 12));
    cmd(SHL(0xE1, 24, 8), SHL(s, 16, 16) | SHL(t, 0, 16));
    cmd(SHL(0xF1, 24, 8), SHL(dsdx, 16, 16) | SHL(dtdy, 0, 16));
}
static void gsSPTextureRectangle(int xl, int yl, int xh, int yh, int tile, int s, int t, int dsdx, int dtdy) {
    texrect(0xE4, xl, yl, xh, yh, tile, s, t, dsdx, dtdy);
}
static void gsSPTextureRectangleFlip(int xl, int yl, int xh, int yh, int tile, int s, int t, int dsdx, int dtdy) {
    texrect(0xE5, xl, yl, xh, yh, tile, s, t, dsdx, dtdy);
}

/* ---------------------------------------------------------------- SDK load sequences */

static int txl2words(int txls, int bytes) {
    int w = txls * bytes / 8;
    return w < 1 ? 1 : w;
}
static int calc_dxt(int width, int bytes) {
    return ((1 << 11) + txl2words(width, bytes) - 1) / txl2words(width, bytes);
}
static int calc_dxt_4b(int width) {
    int w = width / 16 < 1 ? 1 : width / 16;
    return ((1 << 11) + w - 1) / w;
}

/* gDPLoadTextureBlock / _4b into tile 0 at TMEM `tmem` (in 64-bit words) */
static void load_texture_block(uintptr_t timg, int fmt, int siz, int w, int h, int pal, int tmem, int cms,
                               int cmt, int masks, int maskt) {
    static const int load_siz[4] = {G_IM_SIZ_16b, G_IM_SIZ_16b, G_IM_SIZ_16b, G_IM_SIZ_32b};
    static const int incr[4] = {3, 1, 0, 0}, shift[4] = {2, 1, 0, 0}, bytes[4] = {0, 1, 2, 4};
    int line = siz == G_IM_SIZ_4b ? ((w >> 1) + 7) >> 3 : siz == G_IM_SIZ_8b ? (w + 7) >> 3 : (w * 2 + 7) >> 3;
    int dxt = siz == G_IM_SIZ_4b ? calc_dxt_4b(w) : calc_dxt(w, bytes[siz]);
    gsDPSetTextureImage(fmt, load_siz[siz], 1, timg);
    gsDPSetTile(fmt, load_siz[siz], 0, tmem, G_TX_LOADTILE, 0, cmt, maskt, 0, cms, masks, 0);
    gsDPLoadSync();
    gsDPLoadBlock(G_TX_LOADTILE, 0, 0, ((w * h + incr[siz]) >> shift[siz]) - 1, dxt);
    gsDPPipeSync();
    gsDPSetTile(fmt, siz, line, tmem, G_TX_RENDERTILE, pal, cmt, maskt, 0, cms, masks, 0);
    gsDPSetTileSize(G_TX_RENDERTILE, 0, 0, (w - 1) << 2, (h - 1) << 2);
}

/* gDPLoadTextureTile / _4b: the sub-rectangle (uls, ult)-(lrs, lrt) of a width x height image */
static void load_texture_tile(uintptr_t timg, int fmt, int siz, int width, int uls, int ult, int lrs, int lrt,
                              int pal, int cms, int cmt, int masks, int maskt) {
    if (siz == G_IM_SIZ_4b) {
        int line = ((((lrs - uls + 1) >> 1) + 7) >> 3);
        gsDPSetTextureImage(fmt, G_IM_SIZ_8b, width >> 1, timg);
        gsDPSetTile(fmt, G_IM_SIZ_8b, line, 0, G_TX_LOADTILE, 0, cmt, maskt, 0, cms, masks, 0);
        gsDPLoadSync();
        gsDPLoadTile(G_TX_LOADTILE, uls << 1, ult << 2, lrs << 1, lrt << 2);
        gsDPPipeSync();
        gsDPSetTile(fmt, G_IM_SIZ_4b, line, 0, G_TX_RENDERTILE, pal, cmt, maskt, 0, cms, masks, 0);
    } else {
        static const int tile_bytes[4] = {0, 1, 2, 2};
        int line = ((lrs - uls + 1) * tile_bytes[siz] + 7) >> 3;
        gsDPSetTextureImage(fmt, siz, width, timg);
        gsDPSetTile(fmt, siz, line, 0, G_TX_LOADTILE, 0, cmt, maskt, 0, cms, masks, 0);
        gsDPLoadSync();
        gsDPLoadTile(G_TX_LOADTILE, uls << 2, ult << 2, lrs << 2, lrt << 2);
        gsDPPipeSync();
        gsDPSetTile(fmt, siz, line, 0, G_TX_RENDERTILE, pal, cmt, maskt, 0, cms, masks, 0);
    }
    gsDPSetTileSize(G_TX_RENDERTILE, uls << 2, ult << 2, lrs << 2, lrt << 2);
}

static void load_tlut_pal256(uintptr_t pal) {
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, pal);
    gsDPTileSync();
    gsDPSetTile(0, 0, 0, 256, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gsDPLoadSync();
    gsDPLoadTLUTCmd(G_TX_LOADTILE, 255);
    gsDPPipeSync();
}

static void load_tlut_pal16(int bank, uintptr_t pal) {
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, pal);
    gsDPTileSync();
    gsDPSetTile(0, 0, 0, 256 + (bank & 15) * 16, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gsDPLoadSync();
    gsDPLoadTLUTCmd(G_TX_LOADTILE, 15);
    gsDPPipeSync();
}

#endif
