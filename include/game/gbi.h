/* include/game/gbi.h: the display lists the game builds (the frame's, the blitter's, the track map's): the commands'
 * records (Gfx, port/include/pc_gfx_list.h, which the renderer in port/gfx reads), the macros that write them, and the
 * modes, formats and tiles their fields take. (pc_prelude.h brings Gfx's declaration.)
 *
 * The macros keep libultra's names and arguments (gSPVertex, gDPSetTile, gDPLoadTextureBlock ...), as the game calls
 * them: each writes the record of the command it names at pkt, which the call steps on (gSPVertex(gfx++, ...)); a
 * load sequence (gDPLoadTextureBlock) writes one record a step. A value is cut to the width its field has on the RDP
 * (GBI_FIELD), as the N64's commands held it, so a value past it draws as it did there. The modes' values are the
 * RDP's (the other modes' fields in place in their word, the combiner's selectors), which the renderer takes as the
 * N64 did (docs/port/GFX.md "The command records"). */
#ifndef GAME_GBI_H
#define GAME_GBI_H

/* ---- the values the records take ---- */

/* gSPMatrix's parameters: projection or modelview, load or multiply, push the modelview first or not. */
#define G_MTX_MODELVIEW 0x00
#define G_MTX_PROJECTION 0x04
#define G_MTX_MUL 0x00
#define G_MTX_LOAD 0x02
#define G_MTX_NOPUSH 0x00
#define G_MTX_PUSH 0x01

/* The geometry mode's bits (gSPSetGeometryMode). */
#define G_ZBUFFER 0x00000001
#define G_FOG 0x00010000
#define G_LIGHTING 0x00020000
#define G_TEXTURE_GEN 0x00040000
#define G_ON 1

/* The other modes: each field's shift in its word (the high word: G_MDSFT_RGBDITHER to G_MDSFT_CYCLETYPE; the low
 * word: G_MDSFT_ALPHACOMPARE to G_MDSFT_RENDERMODE), then the fields' values in place. */
#define G_MDSFT_RGBDITHER 6
#define G_MDSFT_TEXTFILT 12
#define G_MDSFT_TEXTLUT 14
#define G_MDSFT_TEXTPERSP 19
#define G_MDSFT_CYCLETYPE 20
#define G_MDSFT_ALPHACOMPARE 0
#define G_MDSFT_ZSRCSEL 2
#define G_MDSFT_RENDERMODE 3
#define G_CD_MAGICSQ (0 << G_MDSFT_RGBDITHER)
#define G_CD_BAYER (1 << G_MDSFT_RGBDITHER)
#define G_CD_NOISE (2 << G_MDSFT_RGBDITHER)
#define G_CD_DISABLE (3 << G_MDSFT_RGBDITHER)
#define G_TF_POINT (0 << G_MDSFT_TEXTFILT)
#define G_TF_AVERAGE (3 << G_MDSFT_TEXTFILT)
#define G_TF_BILERP (2 << G_MDSFT_TEXTFILT)
#define G_TT_NONE (0 << G_MDSFT_TEXTLUT)
#define G_TT_RGBA16 (2 << G_MDSFT_TEXTLUT)
#define G_TP_NONE (0 << G_MDSFT_TEXTPERSP)
#define G_CYC_1CYCLE (0 << G_MDSFT_CYCLETYPE)
#define G_CYC_2CYCLE (1 << G_MDSFT_CYCLETYPE)
#define G_CYC_COPY (2 << G_MDSFT_CYCLETYPE)
#define G_CYC_FILL (3 << G_MDSFT_CYCLETYPE)
#define G_AC_NONE (0 << G_MDSFT_ALPHACOMPARE)
#define G_AC_THRESHOLD (1 << G_MDSFT_ALPHACOMPARE)
#define G_ZS_PIXEL (0 << G_MDSFT_ZSRCSEL)
#define G_ZS_PRIM (1 << G_MDSFT_ZSRCSEL)

/* The colour combiner's inputs (a cycle's colour is (a - b) * c + d, its alpha likewise): the selectors' numbers. An
 * input a slot doesn't have reads as 0 there (the RDP's field is narrower: 0 in a and b is 15, in d 7). */
#define G_CCMUX_COMBINED 0
#define G_CCMUX_TEXEL0 1
#define G_CCMUX_TEXEL1 2
#define G_CCMUX_PRIMITIVE 3
#define G_CCMUX_SHADE 4
#define G_CCMUX_ENVIRONMENT 5
#define G_CCMUX_1 6
#define G_CCMUX_0 31
#define G_ACMUX_COMBINED 0
#define G_ACMUX_TEXEL0 1
#define G_ACMUX_TEXEL1 2
#define G_ACMUX_PRIMITIVE 3
#define G_ACMUX_SHADE 4
#define G_ACMUX_ENVIRONMENT 5
#define G_ACMUX_1 6
#define G_ACMUX_0 7

/* The combiner modes the game uses, each one cycle's (a, b, c, d) for the colour, then for the alpha. */
#define G_CC_PRIMITIVE 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE
#define G_CC_SHADE 0, 0, 0, SHADE, 0, 0, 0, SHADE
#define G_CC_DECALRGBA 0, 0, 0, TEXEL0, 0, 0, 0, TEXEL0
#define G_CC_MODULATERGBA TEXEL0, 0, SHADE, 0, TEXEL0, 0, SHADE, 0
#define G_CC_MODULATERGBA_PRIM TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0
#define G_CC_MODULATEIDECALA_PRIM TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, TEXEL0
#define G_CC_PASS2 0, 0, 0, COMBINED, 0, 0, 0, COMBINED
/* The game's own: the texel's colour, its alpha from texel 0 toward texel 1 by the environment's alpha (the
 * blitter's two-tile mode); the texel by the shade, its alpha by the primitive's; the shade with the primitive's
 * alpha; the shade, opaque. */
#define G_CC_DECALRGB_LERPA_ENV 0, 0, 0, TEXEL0, TEXEL1, TEXEL0, ENVIRONMENT, TEXEL0
#define G_CC_MODULATERGB_PRIMA TEXEL0, 0, SHADE, 0, TEXEL0, 0, PRIMITIVE, 0
#define G_CC_SHADE_PRIMA 0, 0, 0, SHADE, 0, 0, 0, PRIMITIVE
#define G_CC_SHADE_OPAQUE 0, 0, 0, SHADE, 0, 0, 0, 1

/* gDPSetScissor's mode. */
#define G_SC_NON_INTERLACE 0

/* The image formats and texel sizes (gDPSetTextureImage, gDPSetTile), and what loading each size takes: its bytes, the
 * size a block load uses for it, the shift and increment that turn its texels into 16-bit ones. */
#define G_IM_FMT_RGBA 0
#define G_IM_FMT_YUV 1
#define G_IM_FMT_CI 2
#define G_IM_FMT_IA 3
#define G_IM_FMT_I 4
#define G_IM_SIZ_4b 0
#define G_IM_SIZ_8b 1
#define G_IM_SIZ_16b 2
#define G_IM_SIZ_32b 3
#define G_IM_SIZ_8b_BYTES 1
#define G_IM_SIZ_8b_TILE_BYTES G_IM_SIZ_8b_BYTES
#define G_IM_SIZ_8b_LINE_BYTES G_IM_SIZ_8b_BYTES
#define G_IM_SIZ_16b_BYTES 2
#define G_IM_SIZ_16b_TILE_BYTES G_IM_SIZ_16b_BYTES
#define G_IM_SIZ_16b_LINE_BYTES G_IM_SIZ_16b_BYTES
#define G_IM_SIZ_32b_BYTES 4
#define G_IM_SIZ_32b_TILE_BYTES 2
#define G_IM_SIZ_32b_LINE_BYTES 2
#define G_IM_SIZ_4b_LOAD_BLOCK G_IM_SIZ_16b
#define G_IM_SIZ_8b_LOAD_BLOCK G_IM_SIZ_16b
#define G_IM_SIZ_16b_LOAD_BLOCK G_IM_SIZ_16b
#define G_IM_SIZ_32b_LOAD_BLOCK G_IM_SIZ_32b
#define G_IM_SIZ_4b_SHIFT 2
#define G_IM_SIZ_8b_SHIFT 1
#define G_IM_SIZ_16b_SHIFT 0
#define G_IM_SIZ_32b_SHIFT 0
#define G_IM_SIZ_4b_INCR 3
#define G_IM_SIZ_8b_INCR 1
#define G_IM_SIZ_16b_INCR 0
#define G_IM_SIZ_32b_INCR 0
#define G_TEXTURE_IMAGE_FRAC 2

/* The tiles and their clamp and mirror bits (gDPSetTile). */
#define G_TX_RENDERTILE 0
#define G_TX_LOADTILE 7
#define G_TX_NOMIRROR 0
#define G_TX_WRAP 0
#define G_TX_MIRROR 0x1
#define G_TX_CLAMP 0x2
#define G_TX_NOMASK 0
#define G_TX_NOLOD 0
#define G_TX_DXT_FRAC 11
#define G_TX_LDBLK_MAX_TXL 2047

/* ---- the records ---- */

/* v cut to the width of its field on the RDP (bits), as the N64's command held it. */
#define GBI_FIELD(v, bits) ((u32)(v) & ((1u << (bits)) - 1))

/* A record with nothing to do: the RDP's pipeline syncs, which have nothing to wait for on the PC. */
static inline Gfx gbi_noop(void) {
    return (Gfx){ .bytes = { GFX_OP_NOOP } };
}

/* The lists. */
static inline Gfx gbi_list(GfxOp op, Gfx *list) {
    return (Gfx){ .list = { .op = (u8)op, .list = list } };
}
static inline Gfx gbi_end(void) {
    return (Gfx){ .bytes = { GFX_OP_END_LIST } };
}
static inline Gfx gbi_cull(u32 first, u32 last) {
    return (Gfx){ .cull = { .op = GFX_OP_CULL_LIST, .first = (u16)first, .last = (u16)last } };
}

/* The geometry. */
static inline Gfx gbi_vertices(void *vertices, u32 count, u32 first) {
    return (Gfx){ .vertices = { .op = GFX_OP_VERTICES, .count = (u8)count, .first = (u8)first, .vertices = vertices } };
}
static inline Gfx gbi_triangle(u32 a, u32 b, u32 c) {
    return (Gfx){ .triangles = { .op = GFX_OP_TRIANGLE, .v = { (u8)a, (u8)b, (u8)c } } };
}
static inline Gfx gbi_triangles(u32 a, u32 b, u32 c, u32 d, u32 e, u32 f) {
    return (Gfx){ .triangles = { .op = GFX_OP_TRIANGLES, .v = { (u8)a, (u8)b, (u8)c, (u8)d, (u8)e, (u8)f } } };
}
static inline Gfx gbi_texture(u32 scale_s, u32 scale_t, u32 level, u32 tile, u32 on) {
    return (Gfx){ .texture = { .op = GFX_OP_TEXTURE, .level = (u8)level, .tile = (u8)tile, .on = (u8)on,
                               .scale_s = (u16)scale_s, .scale_t = (u16)scale_t } };
}
static inline Gfx gbi_geometry_mode(u32 keep, u32 set) {
    return (Gfx){ .geometry_mode = { .op = GFX_OP_GEOMETRY_MODE, .keep = keep, .set = set } };
}
static inline Gfx gbi_matrix(void *matrix, u32 params) {
    return (Gfx){ .matrix = { .op = GFX_OP_MATRIX, .projection = (params & G_MTX_PROJECTION) != 0,
                              .load = (params & G_MTX_LOAD) != 0, .push = (params & G_MTX_PUSH) != 0,
                              .matrix = matrix } };
}
static inline Gfx gbi_pop_matrix(u32 count) {
    return (Gfx){ .pop_matrix = { .op = GFX_OP_POP_MATRIX, .count = count } };
}
static inline Gfx gbi_viewport(void *viewport) {
    return (Gfx){ .viewport = { .op = GFX_OP_VIEWPORT, .viewport = viewport } };
}
static inline Gfx gbi_light(void *light, u32 slot) {
    return (Gfx){ .light = { .op = GFX_OP_LIGHT, .slot = (u8)slot, .light = light } };
}
static inline Gfx gbi_num_lights(u32 count) {
    return (Gfx){ .num_lights = { .op = GFX_OP_NUM_LIGHTS, .count = count } };
}
static inline Gfx gbi_fog(u32 multiplier, u32 offset) {
    return (Gfx){ .fog = { .op = GFX_OP_FOG, .multiplier = (s16)multiplier, .offset = (s16)offset } };
}
static inline Gfx gbi_persp_normalize(u32 scale) {
    return (Gfx){ .persp_normalize = { .op = GFX_OP_PERSP_NORMALIZE, .scale = (u16)scale } };
}

/* The RDP's state. */
static inline Gfx gbi_other_mode(GfxOp op, u32 shift, u32 length, u32 bits) {
    return (Gfx){ .other_mode = { .op = (u8)op, .shift = (u8)shift, .length = (u8)length, .bits = bits } };
}
static inline Gfx gbi_combine(u32 a0, u32 b0, u32 c0, u32 d0, u32 aa0, u32 ab0, u32 ac0, u32 ad0,
                              u32 a1, u32 b1, u32 c1, u32 d1, u32 aa1, u32 ab1, u32 ac1, u32 ad1) {
    Gfx g = { .combine = { .op = GFX_OP_COMBINE } };
    g.combine.cycle[0] = (GfxCombineCycle){ a0, b0, c0, d0, aa0, ab0, ac0, ad0 };
    g.combine.cycle[1] = (GfxCombineCycle){ a1, b1, c1, d1, aa1, ab1, ac1, ad1 };
    return g;
}
static inline Gfx gbi_fill_color(u32 pattern) {
    return (Gfx){ .fill_color = { .op = GFX_OP_FILL_COLOR, .pattern = pattern } };
}
static inline Gfx gbi_color(GfxOp op, u32 min_level, u32 lod_fraction, u32 r, u32 g, u32 b, u32 a) {
    return (Gfx){ .color = { .op = (u8)op, .min_level = (u8)min_level, .lod_fraction = (u8)lod_fraction,
                             .r = (u8)r, .g = (u8)g, .b = (u8)b, .a = (u8)a } };
}
static inline Gfx gbi_prim_depth(u32 z, u32 dz) {
    return (Gfx){ .prim_depth = { .op = GFX_OP_PRIM_DEPTH, .z = (u16)z, .dz = (u16)dz } };
}
static inline Gfx gbi_scissor(u32 mode, u32 ulx, u32 uly, u32 lrx, u32 lry) {
    return (Gfx){ .scissor = { .op = GFX_OP_SCISSOR, .mode = (u8)mode, .ulx = (u16)ulx, .uly = (u16)uly,
                               .lrx = (u16)lrx, .lry = (u16)lry } };
}
static inline Gfx gbi_image(GfxOp op, u32 format, u32 size, u32 width, void *image) {
    return (Gfx){ .image = { .op = (u8)op, .format = (u8)format, .size = (u8)size, .width = (u16)width,
                             .image = image } };
}

/* Textures. */
static inline Gfx gbi_set_tile(u32 format, u32 size, u32 line, u32 tmem, u32 tile, u32 palette, u32 cmt, u32 maskt,
                               u32 shiftt, u32 cms, u32 masks, u32 shifts) {
    return (Gfx){ .set_tile = { .op = GFX_OP_SET_TILE, .tile = (u8)tile, .format = (u8)format, .size = (u8)size,
                                .line = (u16)line, .tmem = (u16)tmem, .palette = (u8)palette, .cmt = (u8)cmt,
                                .maskt = (u8)maskt, .shiftt = (u8)shiftt, .cms = (u8)cms, .masks = (u8)masks,
                                .shifts = (u8)shifts } };
}
static inline Gfx gbi_tile_rect(GfxOp op, u32 tile, u32 uls, u32 ult, u32 lrs, u32 lrt) {
    return (Gfx){ .tile_rect = { .op = (u8)op, .tile = (u8)tile, .uls = (u16)uls, .ult = (u16)ult, .lrs = (u16)lrs,
                                 .lrt = (u16)lrt } };
}
static inline Gfx gbi_load_block(u32 tile, u32 uls, u32 ult, u32 lrs, u32 dxt) {
    return (Gfx){ .load_block = { .op = GFX_OP_LOAD_BLOCK, .tile = (u8)tile, .uls = (u16)uls, .ult = (u16)ult,
                                  .lrs = (u16)lrs, .dxt = (u16)dxt } };
}
static inline Gfx gbi_load_tlut(u32 tile, u32 first, u32 row, u32 last) {
    return (Gfx){ .load_tlut = { .op = GFX_OP_LOAD_TLUT, .tile = (u8)tile, .first = (u16)first, .row = (u16)row,
                                 .last = (u16)last } };
}

/* Rectangles. */
static inline Gfx gbi_rectangle(GfxOp op, u32 tile, u32 flip, u32 ulx, u32 uly, u32 lrx, u32 lry) {
    return (Gfx){ .rectangle = { .op = (u8)op, .tile = (u8)tile, .flip = (u8)flip, .ulx = (u16)ulx, .uly = (u16)uly,
                                 .lrx = (u16)lrx, .lry = (u16)lry } };
}
static inline Gfx gbi_rectangle_texture(u32 s, u32 t, u32 dsdx, u32 dtdy) {
    return (Gfx){ .rectangle_texture = { .op = GFX_OP_RECTANGLE_TEXTURE, .s = (s16)s, .t = (s16)t, .dsdx = (s16)dsdx,
                                         .dtdy = (s16)dtdy } };
}

/* ---- the macros the game calls: each writes its record at pkt, which the caller steps on ---- */

#define GBI_PUT(pkt, record) \
{ \
    Gfx *_g = (pkt); \
    *_g = (record); \
}

/* The lists. */
#define gSPDisplayList(pkt, dl) GBI_PUT(pkt, gbi_list(GFX_OP_CALL_LIST, (Gfx *)(dl)))
#define gSPBranchList(pkt, dl) GBI_PUT(pkt, gbi_list(GFX_OP_BRANCH_LIST, (Gfx *)(dl)))
#define gSPEndDisplayList(pkt) GBI_PUT(pkt, gbi_end())
#define gSPCullDisplayList(pkt, vstart, vend) GBI_PUT(pkt, gbi_cull(GBI_FIELD((vstart) * 2, 16) >> 1, \
                                                                    GBI_FIELD((vend) * 2, 16) >> 1))
#define gDPPipeSync(pkt) GBI_PUT(pkt, gbi_noop())
#define gDPLoadSync(pkt) GBI_PUT(pkt, gbi_noop())
#define gDPTileSync(pkt) GBI_PUT(pkt, gbi_noop())
#define gDPFullSync(pkt) GBI_PUT(pkt, gbi_noop())

/* The geometry. gSP1Triangle's and gSP2Triangles' flag picks the vertex the triangle starts at (its flat shading's). */
#define GBI_TRI_SLOT(v) (GBI_FIELD((v) * 2, 8) >> 1)
#define GBI_TRIANGLE_ARGS(v0, v1, v2, flag) \
    ((flag) == 0 ? GBI_TRI_SLOT(v0) : (flag) == 1 ? GBI_TRI_SLOT(v1) : GBI_TRI_SLOT(v2)), \
    ((flag) == 0 ? GBI_TRI_SLOT(v1) : (flag) == 1 ? GBI_TRI_SLOT(v2) : GBI_TRI_SLOT(v0)), \
    ((flag) == 0 ? GBI_TRI_SLOT(v2) : (flag) == 1 ? GBI_TRI_SLOT(v0) : GBI_TRI_SLOT(v1))
#define gSPVertex(pkt, v, n, v0) GBI_PUT(pkt, gbi_vertices((void *)(v), GBI_FIELD(n, 8), (v0)))
#define gSP1Triangle(pkt, v0, v1, v2, flag) GBI_PUT(pkt, gbi_triangle(GBI_TRIANGLE_ARGS(v0, v1, v2, flag)))
#define gSP2Triangles(pkt, v00, v01, v02, flag0, v10, v11, v12, flag1) \
    GBI_PUT(pkt, gbi_triangles(GBI_TRIANGLE_ARGS(v00, v01, v02, flag0), GBI_TRIANGLE_ARGS(v10, v11, v12, flag1)))
#define gSPMatrix(pkt, m, p) GBI_PUT(pkt, gbi_matrix((void *)(m), GBI_FIELD(p, 8)))
#define gSPPopMatrixN(pkt, n, num) GBI_PUT(pkt, gbi_pop_matrix((u32)((num) * 64) / 64))
#define gSPPopMatrix(pkt, n) gSPPopMatrixN((pkt), (n), 1)
#define gSPViewport(pkt, v) GBI_PUT(pkt, gbi_viewport((void *)(v)))
#define gSPTexture(pkt, s, t, level, tile, on) \
    GBI_PUT(pkt, gbi_texture(GBI_FIELD(s, 16), GBI_FIELD(t, 16), GBI_FIELD(level, 3), GBI_FIELD(tile, 3), \
                             GBI_FIELD(on, 7)))
#define gSPGeometryMode(pkt, c, s) GBI_PUT(pkt, gbi_geometry_mode(GBI_FIELD(~(u32)(c), 24), (u32)(s)))
#define gSPSetGeometryMode(pkt, word) gSPGeometryMode((pkt), 0, (word))
#define gSPClearGeometryMode(pkt, word) gSPGeometryMode((pkt), (word), 0)
#define gSPPerspNormalize(pkt, s) GBI_PUT(pkt, gbi_persp_normalize(s))
#define gSPFogFactor(pkt, fm, fo) GBI_PUT(pkt, gbi_fog(GBI_FIELD(fm, 16), GBI_FIELD(fo, 16)))
#define gSPFogPosition(pkt, min, max) \
    gSPFogFactor(pkt, (500 * 256 / ((max) - (min))), (500 - (min)) * 256 / ((max) - (min)))

/* The lights: slot 0 and 1 the look-at directions, light n (from 1) slot n + 1; the ambient light after the last. */
#define gSPNumLights(pkt, n) GBI_PUT(pkt, gbi_num_lights(n))
#define gSPLight(pkt, l, n) GBI_PUT(pkt, gbi_light((void *)(l), GBI_FIELD(((n) * 24 + 24) / 8, 8) * 8 / 24))
#define gSPSetLights0(pkt, name) \
{ \
    gSPNumLights(pkt, 1); \
    gSPLight(pkt, &name.l[0], 1); \
    gSPLight(pkt, &name.a, 2); \
}
#define gSPSetLights1(pkt, name) \
{ \
    gSPNumLights(pkt, 1); \
    gSPLight(pkt, &name.l[0], 1); \
    gSPLight(pkt, &name.a, 2); \
}
#define gSPSetLights2(pkt, name) \
{ \
    gSPNumLights(pkt, 2); \
    gSPLight(pkt, &name.l[0], 1); \
    gSPLight(pkt, &name.l[1], 2); \
    gSPLight(pkt, &name.a, 3); \
}
#define gSPLookAtX(pkt, l) GBI_PUT(pkt, gbi_light((void *)(l), 0))
#define gSPLookAtY(pkt, l) GBI_PUT(pkt, gbi_light((void *)(l), 1))
#define gSPLookAt(pkt, la) \
{ \
    gSPLookAtX(pkt, la) \
    gSPLookAtY(pkt, (char *)(la) + 16) \
}

/* The other modes: gSPSetOtherMode sets bits sft..sft+len-1 of the high or the low word (GFX_OP_OTHER_MODE_HIGH,
 * _LOW) to those of data, which holds the field in place. */
#define gSPSetOtherMode(pkt, word, sft, len, data) GBI_PUT(pkt, gbi_other_mode((word), (sft), (len), (u32)(data)))
#define gDPSetColorDither(pkt, mode) gSPSetOtherMode(pkt, GFX_OP_OTHER_MODE_HIGH, G_MDSFT_RGBDITHER, 2, mode)
#define gDPSetTextureFilter(pkt, type) gSPSetOtherMode(pkt, GFX_OP_OTHER_MODE_HIGH, G_MDSFT_TEXTFILT, 2, type)
#define gDPSetTextureLUT(pkt, type) gSPSetOtherMode(pkt, GFX_OP_OTHER_MODE_HIGH, G_MDSFT_TEXTLUT, 2, type)
#define gDPSetTexturePersp(pkt, type) gSPSetOtherMode(pkt, GFX_OP_OTHER_MODE_HIGH, G_MDSFT_TEXTPERSP, 1, type)
#define gDPSetCycleType(pkt, type) gSPSetOtherMode(pkt, GFX_OP_OTHER_MODE_HIGH, G_MDSFT_CYCLETYPE, 2, type)
#define gDPSetAlphaCompare(pkt, type) gSPSetOtherMode(pkt, GFX_OP_OTHER_MODE_LOW, G_MDSFT_ALPHACOMPARE, 2, type)
#define gDPSetDepthSource(pkt, src) gSPSetOtherMode(pkt, GFX_OP_OTHER_MODE_LOW, G_MDSFT_ZSRCSEL, 1, src)
/* The render mode: the blender's two cycles and the coverage and depth bits, 29 bits from bit 3 (c0 | c1 in place). */
#define gDPSetRenderMode(pkt, c0, c1) gSPSetOtherMode(pkt, GFX_OP_OTHER_MODE_LOW, G_MDSFT_RENDERMODE, 29, (c0) | (c1))
#define gDPSetRenderModeRaw(pkt, m) gDPSetRenderMode(pkt, m, 0)   /* both cycles' bits as one word */

/* The combiner: a mode for each cycle (G_CC_*), or each selector by its input's name. */
#define gDPSetCombineMode(pkt, a, b) gDPSetCombineLERP(pkt, a, b)
#define gDPSetCombineLERP(pkt, a0, b0, c0, d0, Aa0, Ab0, Ac0, Ad0, a1, b1, c1, d1, Aa1, Ab1, Ac1, Ad1) \
    GBI_PUT(pkt, gbi_combine(G_CCMUX_##a0, G_CCMUX_##b0, G_CCMUX_##c0, G_CCMUX_##d0, \
                             G_ACMUX_##Aa0, G_ACMUX_##Ab0, G_ACMUX_##Ac0, G_ACMUX_##Ad0, \
                             G_CCMUX_##a1, G_CCMUX_##b1, G_CCMUX_##c1, G_CCMUX_##d1, \
                             G_ACMUX_##Aa1, G_ACMUX_##Ab1, G_ACMUX_##Ac1, G_ACMUX_##Ad1))

/* The colours. */
#define gDPSetFillColor(pkt, d) GBI_PUT(pkt, gbi_fill_color((u32)(d)))
#define GBI_COLOR(pkt, op, m, l, r, g, b, a) \
    GBI_PUT(pkt, gbi_color(op, GBI_FIELD(m, 8) & 0x1F, GBI_FIELD(l, 8), GBI_FIELD(r, 8), GBI_FIELD(g, 8), \
                           GBI_FIELD(b, 8), GBI_FIELD(a, 8)))
#define gDPSetFogColor(pkt, r, g, b, a) GBI_COLOR(pkt, GFX_OP_FOG_COLOR, 0, 0, r, g, b, a)
#define gDPSetBlendColor(pkt, r, g, b, a) GBI_COLOR(pkt, GFX_OP_BLEND_COLOR, 0, 0, r, g, b, a)
#define gDPSetEnvColor(pkt, r, g, b, a) GBI_COLOR(pkt, GFX_OP_ENV_COLOR, 0, 0, r, g, b, a)
#define gDPSetPrimColor(pkt, m, l, r, g, b, a) GBI_COLOR(pkt, GFX_OP_PRIM_COLOR, m, l, r, g, b, a)
#define gDPSetPrimDepth(pkt, z, dz) GBI_PUT(pkt, gbi_prim_depth(GBI_FIELD(z, 16), GBI_FIELD(dz, 16)))

/* The images: the frame's, the depth buffer, the texture loads come from. */
#define gDPSetColorImage(pkt, f, s, w, i) \
    GBI_PUT(pkt, gbi_image(GFX_OP_COLOR_IMAGE, GBI_FIELD(f, 3), GBI_FIELD(s, 2), GBI_FIELD((w) - 1, 12) + 1, \
                           (void *)(i)))
#define gDPSetDepthImage(pkt, i) GBI_PUT(pkt, gbi_image(GFX_OP_DEPTH_IMAGE, 0, 0, 1, (void *)(i)))
#define gDPSetTextureImage(pkt, f, s, w, i) \
    GBI_PUT(pkt, gbi_image(GFX_OP_TEXTURE_IMAGE, GBI_FIELD(f, 3), GBI_FIELD(s, 2), GBI_FIELD((w) - 1, 12) + 1, \
                           (void *)(i)))

/* The scissor, in pixels (fractions kept to the quarter). */
#define gDPSetScissor(pkt, mode, ulx, uly, lrx, lry) \
    GBI_PUT(pkt, gbi_scissor(GBI_FIELD(mode, 2), GBI_FIELD((int)((float)(ulx) * 4.0F), 12), \
                             GBI_FIELD((int)((float)(uly) * 4.0F), 12), GBI_FIELD((int)((float)(lrx) * 4.0F), 12), \
                             GBI_FIELD((int)((float)(lry) * 4.0F), 12)))

/* The tiles and the loads into texture memory (coordinates in quarter texels). */
#define gDPSetTile(pkt, fmt, siz, line, tmem, tile, palette, cmt, maskt, shiftt, cms, masks, shifts) \
    GBI_PUT(pkt, gbi_set_tile(GBI_FIELD(fmt, 3), GBI_FIELD(siz, 2), GBI_FIELD(line, 9), GBI_FIELD(tmem, 9), \
                              GBI_FIELD(tile, 3), GBI_FIELD(palette, 4), GBI_FIELD(cmt, 2), GBI_FIELD(maskt, 4), \
                              GBI_FIELD(shiftt, 4), GBI_FIELD(cms, 2), GBI_FIELD(masks, 4), GBI_FIELD(shifts, 4)))
#define GBI_TILE_RECT(pkt, op, t, uls, ult, lrs, lrt) \
    GBI_PUT(pkt, gbi_tile_rect(op, GBI_FIELD(t, 3), GBI_FIELD(uls, 12), GBI_FIELD(ult, 12), GBI_FIELD(lrs, 12), \
                               GBI_FIELD(lrt, 12)))
#define gDPSetTileSize(pkt, t, uls, ult, lrs, lrt) GBI_TILE_RECT(pkt, GFX_OP_TILE_SIZE, t, uls, ult, lrs, lrt)
#define gDPLoadTile(pkt, t, uls, ult, lrs, lrt) GBI_TILE_RECT(pkt, GFX_OP_LOAD_TILE, t, uls, ult, lrs, lrt)
#define gDPLoadBlock(pkt, tile, uls, ult, lrs, dxt) \
    GBI_PUT(pkt, gbi_load_block(GBI_FIELD(tile, 3), GBI_FIELD(uls, 12), GBI_FIELD(ult, 12), \
                                GBI_FIELD(((lrs) < G_TX_LDBLK_MAX_TXL ? (lrs) : G_TX_LDBLK_MAX_TXL), 12), \
                                GBI_FIELD(dxt, 12)))
#define gDPLoadTLUTCmd(pkt, tile, count) GBI_PUT(pkt, gbi_load_tlut(GBI_FIELD(tile, 3), 0, 0, GBI_FIELD(count, 10)))

/* The SDK's load sequences: a palette into the TLUT; a texture's block (whole rows) or tile (a rectangle) into tile 7
 * and the render tile 0 set up to draw it. */
#define GBI_MAX(a, b) ((a) > (b) ? (a) : (b))
#define GBI_TXL2WORDS(txls, b_txl) GBI_MAX(1, ((txls) * (b_txl) / 8))
#define GBI_CALC_DXT(width, b_txl) \
    (((1 << G_TX_DXT_FRAC) + GBI_TXL2WORDS(width, b_txl) - 1) / GBI_TXL2WORDS(width, b_txl))
#define GBI_TXL2WORDS_4b(txls) GBI_MAX(1, ((txls) / 16))
#define GBI_CALC_DXT_4b(width) (((1 << G_TX_DXT_FRAC) + GBI_TXL2WORDS_4b(width) - 1) / GBI_TXL2WORDS_4b(width))

#define gDPLoadTLUT(pkt, count, tmemaddr, dram) \
{ \
    gDPSetTextureImage(pkt, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, dram); \
    gDPTileSync(pkt); \
    gDPSetTile(pkt, 0, 0, 0, tmemaddr, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0); \
    gDPLoadSync(pkt); \
    gDPLoadTLUTCmd(pkt, G_TX_LOADTILE, ((count) - 1)); \
    gDPPipeSync(pkt); \
}
#define gDPLoadTLUT_pal16(pkt, pal, dram) gDPLoadTLUT(pkt, 16, 256 + (((pal) & 0xF) * 16), dram)
#define gDPLoadTLUT_pal256(pkt, dram) gDPLoadTLUT(pkt, 256, 256, dram)

#define gDPLoadTextureBlock(pkt, timg, fmt, siz, width, height, pal, cms, cmt, masks, maskt, shifts, shiftt) \
{ \
    gDPSetTextureImage(pkt, fmt, siz##_LOAD_BLOCK, 1, timg); \
    gDPSetTile(pkt, fmt, siz##_LOAD_BLOCK, 0, 0, G_TX_LOADTILE, 0, cmt, maskt, shiftt, cms, masks, shifts); \
    gDPLoadSync(pkt); \
    gDPLoadBlock(pkt, G_TX_LOADTILE, 0, 0, (((width) * (height) + siz##_INCR) >> siz##_SHIFT) - 1, \
                 GBI_CALC_DXT(width, siz##_BYTES)); \
    gDPPipeSync(pkt); \
    gDPSetTile(pkt, fmt, siz, (((width) * siz##_LINE_BYTES) + 7) >> 3, 0, G_TX_RENDERTILE, pal, cmt, maskt, shiftt, \
               cms, masks, shifts); \
    gDPSetTileSize(pkt, G_TX_RENDERTILE, 0, 0, ((width) - 1) << G_TEXTURE_IMAGE_FRAC, \
                   ((height) - 1) << G_TEXTURE_IMAGE_FRAC) \
}
#define gDPLoadTextureBlock_4b(pkt, timg, fmt, width, height, pal, cms, cmt, masks, maskt, shifts, shiftt) \
{ \
    gDPSetTextureImage(pkt, fmt, G_IM_SIZ_16b, 1, timg); \
    gDPSetTile(pkt, fmt, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, cmt, maskt, shiftt, cms, masks, shifts); \
    gDPLoadSync(pkt); \
    gDPLoadBlock(pkt, G_TX_LOADTILE, 0, 0, (((width) * (height) + 3) >> 2) - 1, GBI_CALC_DXT_4b(width)); \
    gDPPipeSync(pkt); \
    gDPSetTile(pkt, fmt, G_IM_SIZ_4b, ((((width) >> 1) + 7) >> 3), 0, G_TX_RENDERTILE, pal, cmt, maskt, shiftt, cms, \
               masks, shifts); \
    gDPSetTileSize(pkt, G_TX_RENDERTILE, 0, 0, ((width) - 1) << G_TEXTURE_IMAGE_FRAC, \
                   ((height) - 1) << G_TEXTURE_IMAGE_FRAC) \
}
#define gDPLoadTextureTile(pkt, timg, fmt, siz, width, height, uls, ult, lrs, lrt, pal, cms, cmt, masks, maskt, \
                           shifts, shiftt) \
{ \
    gDPSetTextureImage(pkt, fmt, siz, width, timg); \
    gDPSetTile(pkt, fmt, siz, (((((lrs) - (uls) + 1) * siz##_TILE_BYTES) + 7) >> 3), 0, G_TX_LOADTILE, 0, cmt, \
               maskt, shiftt, cms, masks, shifts); \
    gDPLoadSync(pkt); \
    gDPLoadTile(pkt, G_TX_LOADTILE, (uls) << G_TEXTURE_IMAGE_FRAC, (ult) << G_TEXTURE_IMAGE_FRAC, \
                (lrs) << G_TEXTURE_IMAGE_FRAC, (lrt) << G_TEXTURE_IMAGE_FRAC); \
    gDPPipeSync(pkt); \
    gDPSetTile(pkt, fmt, siz, (((((lrs) - (uls) + 1) * siz##_LINE_BYTES) + 7) >> 3), 0, G_TX_RENDERTILE, pal, cmt, \
               maskt, shiftt, cms, masks, shifts); \
    gDPSetTileSize(pkt, G_TX_RENDERTILE, (uls) << G_TEXTURE_IMAGE_FRAC, (ult) << G_TEXTURE_IMAGE_FRAC, \
                   (lrs) << G_TEXTURE_IMAGE_FRAC, (lrt) << G_TEXTURE_IMAGE_FRAC) \
}
#define gDPLoadTextureTile_4b(pkt, timg, fmt, width, height, uls, ult, lrs, lrt, pal, cms, cmt, masks, maskt, shifts, \
                              shiftt) \
{ \
    gDPSetTextureImage(pkt, fmt, G_IM_SIZ_8b, ((width) >> 1), timg); \
    gDPSetTile(pkt, fmt, G_IM_SIZ_8b, (((((lrs) - (uls) + 1) >> 1) + 7) >> 3), 0, G_TX_LOADTILE, 0, cmt, maskt, \
               shiftt, cms, masks, shifts); \
    gDPLoadSync(pkt); \
    gDPLoadTile(pkt, G_TX_LOADTILE, (uls) << (G_TEXTURE_IMAGE_FRAC - 1), (ult) << (G_TEXTURE_IMAGE_FRAC), \
                (lrs) << (G_TEXTURE_IMAGE_FRAC - 1), (lrt) << (G_TEXTURE_IMAGE_FRAC)); \
    gDPPipeSync(pkt); \
    gDPSetTile(pkt, fmt, G_IM_SIZ_4b, (((((lrs) - (uls) + 1) >> 1) + 7) >> 3), 0, G_TX_RENDERTILE, pal, cmt, maskt, \
               shiftt, cms, masks, shifts); \
    gDPSetTileSize(pkt, G_TX_RENDERTILE, (uls) << G_TEXTURE_IMAGE_FRAC, (ult) << G_TEXTURE_IMAGE_FRAC, \
                   (lrs) << G_TEXTURE_IMAGE_FRAC, (lrt) << G_TEXTURE_IMAGE_FRAC) \
}

/* Rectangles: a fill (in pixels), a textured one (in 10.2, its texture coordinates in s10.5 and their steps in s5.10:
 * two records). */
#define gDPFillRectangle(pkt, ulx, uly, lrx, lry) \
    GBI_PUT(pkt, gbi_rectangle(GFX_OP_FILL_RECTANGLE, 0, 0, GBI_FIELD(ulx, 10) << 2, GBI_FIELD(uly, 10) << 2, \
                               GBI_FIELD(lrx, 10) << 2, GBI_FIELD(lry, 10) << 2))
#define gSPTextureRectangle(pkt, xl, yl, xh, yh, tile, s, t, dsdx, dtdy) \
{ \
    GBI_PUT(pkt, gbi_rectangle(GFX_OP_TEXTURE_RECTANGLE, GBI_FIELD(tile, 3), 0, GBI_FIELD(xl, 12), GBI_FIELD(yl, 12), \
                               GBI_FIELD(xh, 12), GBI_FIELD(yh, 12))) \
    GBI_PUT(pkt, gbi_rectangle_texture(GBI_FIELD(s, 16), GBI_FIELD(t, 16), GBI_FIELD(dsdx, 16), \
                                       GBI_FIELD(dtdy, 16))) \
}

#endif /* GAME_GBI_H */
