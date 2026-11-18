/* port/gfx/tests/test_gbi.h: test-local display-list encoders: each writes the command records
 * (port/include/pc_gfx_list.h) the SDK's macro of its name stands for, with the values the SDK's documented formats
 * give the command (coordinates in 10.2, texture coordinates in s10.5 ...). Records go to the current emitter, gp. */
#ifndef TEST_GBI_H
#define TEST_GBI_H

#include <stdint.h>
#include <string.h>

#include "pc_port.h"

/* ---------------------------------------------------------------- emitter */

static Gfx *gp;

/* The next record, zeroed, with its op. */
static Gfx *put(GfxOp op) {
    Gfx *g = gp++;
    memset(g, 0, sizeof(*g));
    g->op = (uint8_t)op;
    return g;
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
#define GBL_c1(m1a, m1b, m2a, m2b) ((unsigned)(m1a) << 30 | (m1b) << 26 | (m2a) << 22 | (m2b) << 18)   /* 3 << 30 */
#define GBL_c2(m1a, m1b, m2a, m2b) ((unsigned)(m1a) << 28 | (m1b) << 24 | (m2a) << 20 | (m2b) << 16)
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
    Gfx *g = put(GFX_OP_VERTICES);
    g->vertices.count = (uint8_t)n;
    g->vertices.first = (uint8_t)v0;
    g->vertices.vertices = (void *)v;
}
static void triangles(GfxOp op, int a, int b, int c, int d, int e, int f) {
    Gfx *g = put(op);
    g->triangles.v[0] = (uint8_t)a;
    g->triangles.v[1] = (uint8_t)b;
    g->triangles.v[2] = (uint8_t)c;
    g->triangles.v[3] = (uint8_t)d;
    g->triangles.v[4] = (uint8_t)e;
    g->triangles.v[5] = (uint8_t)f;
}
static void gsSP1Triangle(int a, int b, int c) {
    triangles(GFX_OP_TRIANGLE, a, b, c, 0, 0, 0);
}
static void gsSP2Triangles(int a, int b, int c, int d, int e, int f) {
    triangles(GFX_OP_TRIANGLES, a, b, c, d, e, f);
}
static void gsSPQuadrangle(int a, int b, int c, int d) {
    triangles(GFX_OP_TRIANGLES, a, b, c, a, c, d);
}
static void matrix(GfxOp op, uintptr_t m, int p) {
    Gfx *g = put(op);
    g->matrix.projection = (p & G_MTX_PROJECTION) != 0;
    g->matrix.load = (p & G_MTX_LOAD) != 0;
    g->matrix.push = (p & G_MTX_PUSH) != 0;
    g->matrix.matrix = (void *)m;
}
static void gsSPMatrix(uintptr_t m, int p) {
    matrix(GFX_OP_MATRIX, m, p);
}
static void gsSPForceMatrix(uintptr_t m) {   /* MP itself */
    matrix(GFX_OP_FORCE_MATRIX, m, 0);
}
static void gsSPInsertMatrix(int offset, uint32_t value) {   /* one word of MP */
    Gfx *g = put(GFX_OP_INSERT_MATRIX);
    g->insert_matrix.offset = (uint16_t)offset;
    g->insert_matrix.value = value;
}
static void gsSPPopMatrixN(int n) {
    put(GFX_OP_POP_MATRIX)->pop_matrix.count = (uint32_t)n;
}
static void gsSPViewport(uintptr_t v) {
    put(GFX_OP_VIEWPORT)->viewport.viewport = (void *)v;
}
static void light(uintptr_t l, int slot) {
    Gfx *g = put(GFX_OP_LIGHT);
    g->light.slot = (uint8_t)slot;
    g->light.light = (void *)l;
}
static void gsSPLight(uintptr_t l, int n) {   /* light n (from 1): slot n + 1, after the look-at's two */
    light(l, n + 1);
}
static void gsSPLookAtX(uintptr_t l) {
    light(l, 0);
}
static void gsSPLookAtY(uintptr_t l) {
    light(l, 1);
}
static void gsSPNumLights(int n) {
    put(GFX_OP_NUM_LIGHTS)->num_lights.count = (uint32_t)n;
}
static void gsSPFogPosition(int min, int max) {
    int fm = 128000 / (max - min), fo = (500 - min) * 256 / (max - min);
    Gfx *g = put(GFX_OP_FOG);
    g->fog.multiplier = (int16_t)fm;
    g->fog.offset = (int16_t)fo;
}
static void gsSPTexture(int s, int t, int level, int tile, int on) {
    Gfx *g = put(GFX_OP_TEXTURE);
    g->texture.scale_s = (uint16_t)s;
    g->texture.scale_t = (uint16_t)t;
    g->texture.level = (uint8_t)(level & 7);
    g->texture.tile = (uint8_t)(tile & 7);
    g->texture.on = (uint8_t)(on & 0x7F);
}
static void gsSPGeometryMode(uint32_t c, uint32_t s) {   /* clear c, then set s */
    Gfx *g = put(GFX_OP_GEOMETRY_MODE);
    g->geometry_mode.keep = ~c & 0xFFFFFF;
    g->geometry_mode.set = s;
}
static void gsSPDisplayList(uintptr_t dl) {
    put(GFX_OP_CALL_LIST)->list.list = (Gfx *)dl;
}
static void gsSPBranchList(uintptr_t dl) {
    put(GFX_OP_BRANCH_LIST)->list.list = (Gfx *)dl;
}
static void gsSPEndDisplayList(void) {
    put(GFX_OP_END_LIST);
}
static void gsSPCullDisplayList(int v0, int vn) {
    Gfx *g = put(GFX_OP_CULL_LIST);
    g->cull.first = (uint16_t)v0;
    g->cull.last = (uint16_t)vn;
}
static void gsSPBranchLessZraw(uintptr_t dl, int v, uint32_t zval) {
    Gfx *g = put(GFX_OP_BRANCH_Z);
    g->branch_z.vertex = (uint16_t)v;
    g->branch_z.z = (int32_t)zval;
    g->branch_z.list = (Gfx *)dl;
}
static void gsSPModifyVertex(int vtx, int where, uint32_t val) {
    Gfx *g = put(GFX_OP_MODIFY_VERTEX);
    g->modify_vertex.what = (uint8_t)where;
    g->modify_vertex.vertex = (uint16_t)vtx;
    g->modify_vertex.value = val;
}
static void gsSPSection(int bit, uintptr_t end) {   /* a level-of-detail marker: a no-op to the renderer */
    Gfx *g = put(GFX_OP_SECTION);
    g->section.bit = (uint16_t)bit;
    g->section.end = (Gfx *)end;
}
/* the other modes: word GFX_OP_OTHER_MODE_HIGH or _LOW, the field's bits sft..sft+len-1 in place in data */
static void gsSPSetOtherMode(GfxOp word, int sft, int len, uint32_t data) {
    Gfx *g = put(word);
    g->other_mode.shift = (uint8_t)sft;
    g->other_mode.length = (uint8_t)len;
    g->other_mode.bits = data;
}

/* ---------------------------------------------------------------- RDP */

static void gsDPPipeSync(void) { put(GFX_OP_NOOP); }
static void gsDPLoadSync(void) { put(GFX_OP_NOOP); }
static void gsDPTileSync(void) { put(GFX_OP_NOOP); }
static void gsDPFullSync(void) { put(GFX_OP_NOOP); }
static void gsDPSetCycleType(uint32_t t) { gsSPSetOtherMode(GFX_OP_OTHER_MODE_HIGH, 20, 2, t); }
static void gsDPSetTextureLUT(uint32_t t) { gsSPSetOtherMode(GFX_OP_OTHER_MODE_HIGH, 14, 2, t); }
static void gsDPSetTextureFilter(uint32_t t) { gsSPSetOtherMode(GFX_OP_OTHER_MODE_HIGH, 12, 2, t); }
static void gsDPSetTexturePersp(uint32_t t) { gsSPSetOtherMode(GFX_OP_OTHER_MODE_HIGH, 19, 1, t); }
static void gsDPSetTextureLOD(uint32_t t) { gsSPSetOtherMode(GFX_OP_OTHER_MODE_HIGH, 16, 1, t); }
static void gsDPSetAlphaCompare(uint32_t t) { gsSPSetOtherMode(GFX_OP_OTHER_MODE_LOW, 0, 2, t); }
static void gsDPSetDepthSource(uint32_t t) { gsSPSetOtherMode(GFX_OP_OTHER_MODE_LOW, 2, 1, t); }
static void gsDPSetRenderMode(uint32_t c0, uint32_t c1) { gsSPSetOtherMode(GFX_OP_OTHER_MODE_LOW, 3, 29, c0 | c1); }
static void gsDPSetCombineLERP(int a0, int b0, int c0, int d0, int Aa0, int Ab0, int Ac0, int Ad0,
                               int a1, int b1, int c1, int d1, int Aa1, int Ab1, int Ac1, int Ad1) {
    Gfx *g = put(GFX_OP_COMBINE);   /* each selector cut to its field's width, as the RDP's */
    g->combine.cycle[0].rgb_a = (unsigned)a0;
    g->combine.cycle[0].rgb_b = (unsigned)b0;
    g->combine.cycle[0].rgb_c = (unsigned)c0;
    g->combine.cycle[0].rgb_d = (unsigned)d0;
    g->combine.cycle[0].alpha_a = (unsigned)Aa0;
    g->combine.cycle[0].alpha_b = (unsigned)Ab0;
    g->combine.cycle[0].alpha_c = (unsigned)Ac0;
    g->combine.cycle[0].alpha_d = (unsigned)Ad0;
    g->combine.cycle[1].rgb_a = (unsigned)a1;
    g->combine.cycle[1].rgb_b = (unsigned)b1;
    g->combine.cycle[1].rgb_c = (unsigned)c1;
    g->combine.cycle[1].rgb_d = (unsigned)d1;
    g->combine.cycle[1].alpha_a = (unsigned)Aa1;
    g->combine.cycle[1].alpha_b = (unsigned)Ab1;
    g->combine.cycle[1].alpha_c = (unsigned)Ac1;
    g->combine.cycle[1].alpha_d = (unsigned)Ad1;
}
/* one mode for both cycles */
static void cc1(int a, int b, int c, int d, int Aa, int Ab, int Ac, int Ad) {
    gsDPSetCombineLERP(a, b, c, d, Aa, Ab, Ac, Ad, a, b, c, d, Aa, Ab, Ac, Ad);
}
static void cc_shade(void) { cc1(CC_0, CC_0, CC_0, CC_SHADE, AC_0, AC_0, AC_0, AC_SHADE); }
static void cc_prim(void) { cc1(CC_0, CC_0, CC_0, CC_PRIMITIVE, AC_0, AC_0, AC_0, AC_PRIMITIVE); }
static void cc_decalrgba(void) { cc1(CC_0, CC_0, CC_0, CC_TEXEL0, AC_0, AC_0, AC_0, AC_TEXEL0); }
static void cc_texalpha(void) { cc1(CC_1, CC_0, CC_TEXEL0_ALPHA, CC_0, AC_0, AC_0, AC_0, AC_1); }

static void setcolor(GfxOp op, int m, int l, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    Gfx *c = put(op);
    c->color.min_level = (uint8_t)(m & 0x1F);
    c->color.lod_fraction = (uint8_t)l;
    c->color.r = r;
    c->color.g = g;
    c->color.b = b;
    c->color.a = a;
}
static void gsDPSetPrimColor(int m, int l, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    setcolor(GFX_OP_PRIM_COLOR, m, l, r, g, b, a);
}
static void gsDPSetEnvColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    setcolor(GFX_OP_ENV_COLOR, 0, 0, r, g, b, a);
}
static void gsDPSetFogColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    setcolor(GFX_OP_FOG_COLOR, 0, 0, r, g, b, a);
}
static void gsDPSetBlendColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    setcolor(GFX_OP_BLEND_COLOR, 0, 0, r, g, b, a);
}
static void gsDPSetFillColor(uint32_t c) { put(GFX_OP_FILL_COLOR)->fill_color.pattern = c; }
static void gsDPSetPrimDepth(int z, int dz) {
    Gfx *g = put(GFX_OP_PRIM_DEPTH);
    g->prim_depth.z = (uint16_t)z;
    g->prim_depth.dz = (uint16_t)dz;
}
static void setimg(GfxOp op, int fmt, int siz, int width, uintptr_t img) {
    Gfx *g = put(op);
    g->image.format = (uint8_t)fmt;
    g->image.size = (uint8_t)siz;
    g->image.width = (uint16_t)(((width - 1) & 0xFFF) + 1);
    g->image.image = (void *)img;
}
static void gsDPSetColorImage(int fmt, int siz, int w, uintptr_t img) { setimg(GFX_OP_COLOR_IMAGE, fmt, siz, w, img); }
static void gsDPSetTextureImage(int fmt, int siz, int w, uintptr_t img) {
    setimg(GFX_OP_TEXTURE_IMAGE, fmt, siz, w, img);
}
static void gsDPSetDepthImage(uintptr_t img) { setimg(GFX_OP_DEPTH_IMAGE, 0, 0, 1, img); }
static void gsDPSetScissor(int ulx, int uly, int lrx, int lry) {   /* pixels, to 10.2 */
    Gfx *g = put(GFX_OP_SCISSOR);
    g->scissor.ulx = (uint16_t)((ulx * 4) & 0xFFF);
    g->scissor.uly = (uint16_t)((uly * 4) & 0xFFF);
    g->scissor.lrx = (uint16_t)((lrx * 4) & 0xFFF);
    g->scissor.lry = (uint16_t)((lry * 4) & 0xFFF);
}
static void gsDPFillRectangle(int ulx, int uly, int lrx, int lry) {   /* pixels, to 10.2 */
    Gfx *g = put(GFX_OP_FILL_RECTANGLE);
    g->rectangle.ulx = (uint16_t)((ulx & 0x3FF) << 2);
    g->rectangle.uly = (uint16_t)((uly & 0x3FF) << 2);
    g->rectangle.lrx = (uint16_t)((lrx & 0x3FF) << 2);
    g->rectangle.lry = (uint16_t)((lry & 0x3FF) << 2);
}
static void gsDPSetTile(int fmt, int siz, int line, int tmem, int tile, int palette, int cmt, int maskt,
                        int shiftt, int cms, int masks, int shifts) {
    Gfx *g = put(GFX_OP_SET_TILE);
    g->set_tile.format = (uint8_t)(fmt & 7);
    g->set_tile.size = (uint8_t)(siz & 3);
    g->set_tile.line = (uint16_t)(line & 0x1FF);
    g->set_tile.tmem = (uint16_t)(tmem & 0x1FF);
    g->set_tile.tile = (uint8_t)(tile & 7);
    g->set_tile.palette = (uint8_t)(palette & 0xF);
    g->set_tile.cmt = (uint8_t)(cmt & 3);
    g->set_tile.maskt = (uint8_t)(maskt & 0xF);
    g->set_tile.shiftt = (uint8_t)(shiftt & 0xF);
    g->set_tile.cms = (uint8_t)(cms & 3);
    g->set_tile.masks = (uint8_t)(masks & 0xF);
    g->set_tile.shifts = (uint8_t)(shifts & 0xF);
}
static void tile_rect(GfxOp op, int t, int uls, int ult, int lrs, int lrt) {   /* 10.2 */
    Gfx *g = put(op);
    g->tile_rect.tile = (uint8_t)(t & 7);
    g->tile_rect.uls = (uint16_t)(uls & 0xFFF);
    g->tile_rect.ult = (uint16_t)(ult & 0xFFF);
    g->tile_rect.lrs = (uint16_t)(lrs & 0xFFF);
    g->tile_rect.lrt = (uint16_t)(lrt & 0xFFF);
}
static void gsDPSetTileSize(int t, int uls, int ult, int lrs, int lrt) {
    tile_rect(GFX_OP_TILE_SIZE, t, uls, ult, lrs, lrt);
}
static void gsDPLoadTile(int t, int uls, int ult, int lrs, int lrt) {
    tile_rect(GFX_OP_LOAD_TILE, t, uls, ult, lrs, lrt);
}
static void gsDPLoadBlock(int t, int uls, int ult, int lrs, int dxt) {
    Gfx *g = put(GFX_OP_LOAD_BLOCK);
    g->load_block.tile = (uint8_t)(t & 7);
    g->load_block.uls = (uint16_t)(uls & 0xFFF);
    g->load_block.ult = (uint16_t)(ult & 0xFFF);
    g->load_block.lrs = (uint16_t)(lrs & 0xFFF);
    g->load_block.dxt = (uint16_t)(dxt & 0xFFF);
}
static void gsDPLoadTLUTCmd(int t, int count) {   /* entries 0..count */
    Gfx *g = put(GFX_OP_LOAD_TLUT);
    g->load_tlut.tile = (uint8_t)(t & 7);
    g->load_tlut.last = (uint16_t)(count & 0x3FF);
}
/* xl..xh in 10.2; s, t s10.5; dsdx, dtdy s5.10: the rectangle, then its texture coordinates */
static void texrect(int flip, int xl, int yl, int xh, int yh, int tile, int s, int t, int dsdx, int dtdy) {
    Gfx *g = put(GFX_OP_TEXTURE_RECTANGLE);
    g->rectangle.tile = (uint8_t)(tile & 7);
    g->rectangle.flip = (uint8_t)flip;
    g->rectangle.ulx = (uint16_t)(xl & 0xFFF);
    g->rectangle.uly = (uint16_t)(yl & 0xFFF);
    g->rectangle.lrx = (uint16_t)(xh & 0xFFF);
    g->rectangle.lry = (uint16_t)(yh & 0xFFF);
    g = put(GFX_OP_RECTANGLE_TEXTURE);
    g->rectangle_texture.s = (int16_t)s;
    g->rectangle_texture.t = (int16_t)t;
    g->rectangle_texture.dsdx = (int16_t)dsdx;
    g->rectangle_texture.dtdy = (int16_t)dtdy;
}
static void gsSPTextureRectangle(int xl, int yl, int xh, int yh, int tile, int s, int t, int dsdx, int dtdy) {
    texrect(0, xl, yl, xh, yh, tile, s, t, dsdx, dtdy);
}
static void gsSPTextureRectangleFlip(int xl, int yl, int xh, int yh, int tile, int s, int t, int dsdx, int dtdy) {
    texrect(1, xl, yl, xh, yh, tile, s, t, dsdx, dtdy);
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
