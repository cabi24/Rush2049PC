/* zoid_draw/group.c: the scene draw: the scene tree, the flat polygons and the 2D blits into the frame's display list
 * (the N64's back end of the ZOID renderer; MAT3's is NGC/ngcZoid.c, ZOID_EndFrame and Traverse).
 *
 * Each frame ZOID_EndFrame draws every window (zoid_draw_window): it sets the window's viewport, scissor, fog and
 * projection, walks the scene tree from ObjHead (Traverse: a node's matrix, its culling box, its lights, then its
 * object at the level of detail its distance picks, zoid_draw_object, then its children), draws the flat polygons
 * (PolyInstanceList: shadows, arrows, skid marks) and the nodes held back for the two late passes; then it draws the
 * 2D blit instances through the blitter. What it writes are N64 display lists (F3DEX2 microcode, RDP commands), as
 * libultra's gbi.h macros below build them; on the PC port/gfx renders them. Also here: the arcsine and the
 * quaternion interpolation the camera paths use. */
#include "game/game.h"

/* ---- libultra's display-list macros (PR/gbi.h), the forms this file uses ---- */
#define _SHIFTL(v, s, w) ((u32)(((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define TXL2WORDS(txls, b_txl) MAX(1, ((txls)*(b_txl)/8))
#define CALC_DXT(width, b_txl) \
    (((1 << G_TX_DXT_FRAC) + TXL2WORDS(width, b_txl) - 1) / TXL2WORDS(width, b_txl))
#define TXL2WORDS_4b(txls) MAX(1, ((txls)/16))
#define CALC_DXT_4b(width) \
    (((1 << G_TX_DXT_FRAC) + TXL2WORDS_4b(width) - 1) / TXL2WORDS_4b(width))

#define gSPSetOtherMode(pkt, cmd, sft, len, data) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(cmd, 24, 8) | _SHIFTL(32 - (sft) - (len), 8, 8) | _SHIFTL((len) - 1, 0, 8)); \
    _g->words.w1 = (unsigned int)(data); \
}
#define gDPSetTextureLUT(pkt, type) gSPSetOtherMode(pkt, G_SETOTHERMODE_H, G_MDSFT_TEXTLUT, 2, type)
#define gSetImage(pkt, cmd, fmt, siz, width, i) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(cmd, 24, 8) | _SHIFTL(fmt, 21, 3) | _SHIFTL(siz, 19, 2) | _SHIFTL((width)-1, 0, 12); \
    _g->words.w1 = (uintptr_t)(i); \
}
#define gDPSetTextureImage(pkt, f, s, w, i) gSetImage(pkt, G_SETTIMG, f, s, w, i)
#define gDPLoadTileGeneric(pkt, c, tile, uls, ult, lrs, lrt) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(c, 24, 8) | _SHIFTL(uls, 12, 12) | _SHIFTL(ult, 0, 12); \
    _g->words.w1 = _SHIFTL(tile, 24, 3) | _SHIFTL(lrs, 12, 12) | _SHIFTL(lrt, 0, 12); \
}
#define gDPSetTileSize(pkt, t, uls, ult, lrs, lrt) gDPLoadTileGeneric(pkt, G_SETTILESIZE, t, uls, ult, lrs, lrt)
#define gDPLoadTile(pkt, t, uls, ult, lrs, lrt) gDPLoadTileGeneric(pkt, G_LOADTILE, t, uls, ult, lrs, lrt)
#define gDPSetTile(pkt, fmt, siz, line, tmem, tile, palette, cmt, maskt, shiftt, cms, masks, shifts) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(G_SETTILE, 24, 8) | _SHIFTL(fmt, 21, 3) | _SHIFTL(siz, 19, 2) | _SHIFTL(line, 9, 9) | _SHIFTL(tmem, 0, 9); \
    _g->words.w1 = _SHIFTL(tile, 24, 3) | _SHIFTL(palette, 20, 4) | _SHIFTL(cmt, 18, 2) | _SHIFTL(maskt, 14, 4) | \
                   _SHIFTL(shiftt, 10, 4) | _SHIFTL(cms, 8, 2) | _SHIFTL(masks, 4, 4) | _SHIFTL(shifts, 0, 4); \
}
#define gDPLoadBlock(pkt, tile, uls, ult, lrs, dxt) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(G_LOADBLOCK, 24, 8) | _SHIFTL(uls, 12, 12) | _SHIFTL(ult, 0, 12)); \
    _g->words.w1 = (_SHIFTL(tile, 24, 3) | _SHIFTL((MIN(lrs,G_TX_LDBLK_MAX_TXL)), 12, 12) | _SHIFTL(dxt, 0, 12)); \
}
#define gDPNoParam(pkt, cmd) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(cmd, 24, 8); \
    _g->words.w1 = 0; \
}
#define gDPPipeSync(pkt) gDPNoParam(pkt, G_RDPPIPESYNC)
#define gDPLoadSync(pkt) gDPNoParam(pkt, G_RDPLOADSYNC)

#define gDPLoadTextureTile(pkt, timg, fmt, siz, width, height, uls, ult, lrs, lrt, pal, cms, cmt, masks, maskt, shifts, shiftt) \
{ \
    gDPSetTextureImage(pkt, fmt, siz, width, timg); \
    gDPSetTile(pkt, fmt, siz, (((((lrs)-(uls)+1) * siz##_TILE_BYTES)+7)>>3), 0, G_TX_LOADTILE, 0 , cmt, maskt, shiftt, cms, masks, shifts); \
    gDPLoadSync(pkt); \
    gDPLoadTile(pkt, G_TX_LOADTILE, (uls)<<G_TEXTURE_IMAGE_FRAC, (ult)<<G_TEXTURE_IMAGE_FRAC, (lrs)<<G_TEXTURE_IMAGE_FRAC, (lrt)<<G_TEXTURE_IMAGE_FRAC); \
    gDPPipeSync(pkt); \
    gDPSetTile(pkt, fmt, siz, (((((lrs)-(uls)+1) * siz##_LINE_BYTES)+7)>>3), 0, G_TX_RENDERTILE, pal, cmt, maskt, shiftt, cms, masks, shifts); \
    gDPSetTileSize(pkt, G_TX_RENDERTILE, (uls)<<G_TEXTURE_IMAGE_FRAC, (ult)<<G_TEXTURE_IMAGE_FRAC, (lrs)<<G_TEXTURE_IMAGE_FRAC, (lrt)<<G_TEXTURE_IMAGE_FRAC) \
}
#define gDPLoadTextureTile_4b(pkt, timg, fmt, width, height, uls, ult, lrs, lrt, pal, cms, cmt, masks, maskt, shifts, shiftt) \
{ \
    gDPSetTextureImage(pkt, fmt, G_IM_SIZ_8b, ((width)>>1), timg); \
    gDPSetTile(pkt, fmt, G_IM_SIZ_8b, (((((lrs)-(uls)+1)>>1)+7)>>3), 0, G_TX_LOADTILE, 0 , cmt, maskt, shiftt, cms, masks, shifts); \
    gDPLoadSync(pkt); \
    gDPLoadTile(pkt, G_TX_LOADTILE, (uls)<<(G_TEXTURE_IMAGE_FRAC-1), (ult)<<(G_TEXTURE_IMAGE_FRAC), (lrs)<<(G_TEXTURE_IMAGE_FRAC-1), (lrt)<<(G_TEXTURE_IMAGE_FRAC)); \
    gDPPipeSync(pkt); \
    gDPSetTile(pkt, fmt, G_IM_SIZ_4b, (((((lrs)-(uls)+1)>>1)+7)>>3), 0, G_TX_RENDERTILE, pal, cmt, maskt, shiftt, cms, masks, shifts); \
    gDPSetTileSize(pkt, G_TX_RENDERTILE, (uls)<<G_TEXTURE_IMAGE_FRAC, (ult)<<G_TEXTURE_IMAGE_FRAC, (lrs)<<G_TEXTURE_IMAGE_FRAC, (lrt)<<G_TEXTURE_IMAGE_FRAC) \
}
#define gDPLoadTextureBlock(pkt, timg, fmt, siz, width, height, pal, cms, cmt, masks, maskt, shifts, shiftt) \
{ \
    gDPSetTextureImage(pkt, fmt, siz##_LOAD_BLOCK, 1, timg); \
    gDPSetTile(pkt, fmt, siz##_LOAD_BLOCK, 0, 0, G_TX_LOADTILE, 0 , cmt, maskt, shiftt, cms, masks, shifts); \
    gDPLoadSync(pkt); \
    gDPLoadBlock(pkt, G_TX_LOADTILE, 0, 0, (((width)*(height) + siz##_INCR) >> siz##_SHIFT) -1, CALC_DXT(width, siz##_BYTES)); \
    gDPPipeSync(pkt); \
    gDPSetTile(pkt, fmt, siz, (((width) * siz##_LINE_BYTES)+7)>>3, 0, G_TX_RENDERTILE, pal, cmt, maskt, shiftt, cms, masks, shifts); \
    gDPSetTileSize(pkt, G_TX_RENDERTILE, 0, 0, ((width)-1) << G_TEXTURE_IMAGE_FRAC, ((height)-1) << G_TEXTURE_IMAGE_FRAC) \
}
#define gDPLoadTextureBlock_4b(pkt, timg, fmt, width, height, pal, cms, cmt, masks, maskt, shifts, shiftt) \
{ \
    gDPSetTextureImage(pkt, fmt, G_IM_SIZ_16b, 1, timg); \
    gDPSetTile(pkt, fmt, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0 , cmt, maskt, shiftt, cms, masks, shifts); \
    gDPLoadSync(pkt); \
    gDPLoadBlock(pkt, G_TX_LOADTILE, 0, 0, (((width)*(height)+3)>>2)-1, CALC_DXT_4b(width)); \
    gDPPipeSync(pkt); \
    gDPSetTile(pkt, fmt, G_IM_SIZ_4b, ((((width)>>1)+7)>>3), 0, G_TX_RENDERTILE, pal, cmt, maskt, shiftt, cms, masks, shifts); \
    gDPSetTileSize(pkt, G_TX_RENDERTILE, 0, 0, ((width)-1) << G_TEXTURE_IMAGE_FRAC, ((height)-1) << G_TEXTURE_IMAGE_FRAC) \
}
#define gDma1p(pkt, c, s, l, p) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL((c), 24, 8) | _SHIFTL((p), 16, 8) | _SHIFTL((l), 0, 16)); \
    _g->words.w1 = (uintptr_t)(s); \
}
#define gSPDisplayList(pkt,dl) gDma1p(pkt,G_DL,dl,0,G_DL_PUSH)

/* The model files' records the draw reads (the texture, palette and object definitions: tTexture, tPaletteDef,
 * tModelDef) are the WAD's records, built beside the file with host pointers to its texels, colours and display lists
 * (pc_game_types.h, port/assets/wad.c); on the N64 they were the file's, with 32-bit addresses. */
/* A texture tile, the part of a texture a node draws (MAT3 anon_uls_ult_8), in 10.5 fixed point (32 a texel). */
typedef struct TexTile {
    u16 uls, ult, lrs, lrt;
} TexTile;
#define gDPTileSync(pkt) gDPNoParam(pkt, G_RDPTILESYNC)
#define gDPLoadTLUTCmd(pkt, tile, count) \
{ \
    Gfx *_g = (Gfx *)pkt; \
    _g->words.w0 = _SHIFTL(G_LOADTLUT, 24, 8); \
    _g->words.w1 = _SHIFTL((tile), 24, 3) | _SHIFTL((count), 14, 10); \
}
#define gDPLoadTLUT(pkt, count, tmemaddr, dram) \
{ \
    gDPSetTextureImage(pkt, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, dram); \
    gDPTileSync(pkt); \
    gDPSetTile(pkt, 0, 0, 0, tmemaddr, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0); \
    gDPLoadSync(pkt); \
    gDPLoadTLUTCmd(pkt, G_TX_LOADTILE, ((count) - 1)); \
    gDPPipeSync(pkt); \
}

/* ---- the F3DEX2 microcode's forms (PR/gbi.h with F3DEX_GBI_2) ---- */
#define NUML(n) ((n) * 24)
#define NUMLIGHTS_0 1
#define NUMLIGHTS_1 1
#define NUMLIGHTS_2 2
#define BOWTIE_VAL 0

#define gDma2p(pkt, c, adrs, len, idx, ofs) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL((c), 24, 8) | _SHIFTL(((len) - 1) / 8, 19, 5) | _SHIFTL((ofs) / 8, 8, 8) | _SHIFTL((idx), 0, 8)); \
    _g->words.w1 = (uintptr_t)(adrs); \
}
#define gSPBranchList(pkt, dl) gDma1p(pkt, G_DL, dl, 0, G_DL_NOPUSH)
#define gSPEndDisplayList(pkt) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(G_ENDDL, 24, 8); \
    _g->words.w1 = 0; \
}
#define gSPMatrix(pkt, m, p) gDma2p((pkt), G_MTX, (m), sizeof(Mtx), (p) ^ G_MTX_PUSH, 0)
#define gSPPopMatrixN(pkt, n, num) gDma2p((pkt), G_POPMTX, (num) * 64, 64, 2, 0)
#define gSPPopMatrix(pkt, n) gSPPopMatrixN((pkt), (n), 1)
#define gSPViewport(pkt, v) gDma2p((pkt), G_MOVEMEM, (v), sizeof(Vp), G_MV_VIEWPORT, 0)
#define gSPVertex(pkt, v, n, v0) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(G_VTX, 24, 8) | _SHIFTL((n), 12, 8) | _SHIFTL((v0) + (n), 1, 7); \
    _g->words.w1 = (uintptr_t)(v); \
}
#define gSPCullDisplayList(pkt, vstart, vend) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(G_CULLDL, 24, 8) | _SHIFTL((vstart) * 2, 0, 16); \
    _g->words.w1 = _SHIFTL((vend) * 2, 0, 16); \
}
#define __gsSP1Triangle_w1(v0, v1, v2) (_SHIFTL((v0) * 2, 16, 8) | _SHIFTL((v1) * 2, 8, 8) | _SHIFTL((v2) * 2, 0, 8))
#define __gsSP1Triangle_w1f(v0, v1, v2, flag) \
    (((flag) == 0) ? __gsSP1Triangle_w1(v0, v1, v2) : \
     ((flag) == 1) ? __gsSP1Triangle_w1(v1, v2, v0) : \
                     __gsSP1Triangle_w1(v2, v0, v1))
#define gSP1Triangle(pkt, v0, v1, v2, flag) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(G_TRI1, 24, 8) | __gsSP1Triangle_w1f(v0, v1, v2, flag); \
    _g->words.w1 = 0; \
}
#define gSP2Triangles(pkt, v00, v01, v02, flag0, v10, v11, v12, flag1) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(G_TRI2, 24, 8) | __gsSP1Triangle_w1f(v00, v01, v02, flag0)); \
    _g->words.w1 = __gsSP1Triangle_w1f(v10, v11, v12, flag1); \
}
#define gMoveWd(pkt, index, offset, data) gDma1p((pkt), G_MOVEWORD, data, offset, index)
#define gSPNumLights(pkt, n) gMoveWd(pkt, G_MW_NUMLIGHT, G_MWO_NUMLIGHT, NUML(n))
#define gSPLight(pkt, l, n) gDma2p((pkt), G_MOVEMEM, (l), sizeof(Light), G_MV_LIGHT, (n) * 24 + 24)
#define gSPSetLights0(pkt, name) \
{ \
    gSPNumLights(pkt, NUMLIGHTS_0); \
    gSPLight(pkt, &name.l[0], 1); \
    gSPLight(pkt, &name.a, 2); \
}
#define gSPSetLights1(pkt, name) \
{ \
    gSPNumLights(pkt, NUMLIGHTS_1); \
    gSPLight(pkt, &name.l[0], 1); \
    gSPLight(pkt, &name.a, 2); \
}
#define gSPSetLights2(pkt, name) \
{ \
    gSPNumLights(pkt, NUMLIGHTS_2); \
    gSPLight(pkt, &name.l[0], 1); \
    gSPLight(pkt, &name.l[1], 2); \
    gSPLight(pkt, &name.a, 3); \
}
#define gSPLookAtX(pkt, l) gDma2p((pkt), G_MOVEMEM, (l), sizeof(Light), G_MV_LIGHT, G_MVO_LOOKATX)
#define gSPLookAtY(pkt, l) gDma2p((pkt), G_MOVEMEM, (l), sizeof(Light), G_MV_LIGHT, G_MVO_LOOKATY)
#define gSPLookAt(pkt, la) \
{ \
    gSPLookAtX(pkt, la) \
    gSPLookAtY(pkt, (char *)(la) + 16) \
}
#define gSPGeometryMode(pkt, c, s) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(G_GEOMETRYMODE, 24, 8) | _SHIFTL(~(u32)(c), 0, 24); \
    _g->words.w1 = (u32)(s); \
}
#define gSPSetGeometryMode(pkt, word) gSPGeometryMode((pkt), 0, (word))
#define gSPClearGeometryMode(pkt, word) gSPGeometryMode((pkt), (word), 0)
#define gSPTexture(pkt, s, t, level, tile, on) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(G_TEXTURE, 24, 8) | _SHIFTL(BOWTIE_VAL, 16, 8) | _SHIFTL((level), 11, 3) | \
                    _SHIFTL((tile), 8, 3) | _SHIFTL((on), 1, 7)); \
    _g->words.w1 = (_SHIFTL((s), 16, 16) | _SHIFTL((t), 0, 16)); \
}
#define gSPPerspNormalize(pkt, s) gMoveWd(pkt, G_MW_PERSPNORM, 0, (s))
#define gSPFogFactor(pkt, fm, fo) gMoveWd(pkt, G_MW_FOG, G_MWO_FOG, (_SHIFTL(fm, 16, 16) | _SHIFTL(fo, 0, 16)))
#define gSPFogPosition(pkt, min, max) \
    gSPFogFactor(pkt, (500 * 256 / ((max) - (min))), (500 - (min)) * 256 / ((max) - (min)))
#define gDPSetCycleType(pkt, type) gSPSetOtherMode(pkt, G_SETOTHERMODE_H, G_MDSFT_CYCLETYPE, 2, type)
#define gDPSetDepthSource(pkt, src) gSPSetOtherMode(pkt, G_SETOTHERMODE_L, G_MDSFT_ZSRCSEL, 1, src)
#define gDPSetRenderMode(pkt, c0, c1) gSPSetOtherMode(pkt, G_SETOTHERMODE_L, G_MDSFT_RENDERMODE, 29, (c0) | (c1))
#define gDPSetCombine(pkt, muxs0, muxs1) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(G_SETCOMBINE, 24, 8) | _SHIFTL(muxs0, 0, 24); \
    _g->words.w1 = (unsigned int)(muxs1); \
}
#define gDPSetColor(pkt, c, d) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(c, 24, 8); \
    _g->words.w1 = (unsigned int)(d); \
}
#define DPRGBColor(pkt, cmd, r, g, b, a) \
    gDPSetColor(pkt, cmd, (_SHIFTL(r, 24, 8) | _SHIFTL(g, 16, 8) | _SHIFTL(b, 8, 8) | _SHIFTL(a, 0, 8)))
#define gDPSetEnvColor(pkt, r, g, b, a) DPRGBColor(pkt, G_SETENVCOLOR, r, g, b, a)
#define gDPSetFogColor(pkt, r, g, b, a) DPRGBColor(pkt, G_SETFOGCOLOR, r, g, b, a)
#define gDPSetPrimColor(pkt, m, l, r, g, b, a) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(G_SETPRIMCOLOR, 24, 8) | _SHIFTL(m, 8, 8) | _SHIFTL(l, 0, 8)); \
    _g->words.w1 = (_SHIFTL(r, 24, 8) | _SHIFTL(g, 16, 8) | _SHIFTL(b, 8, 8) | _SHIFTL(a, 0, 8)); \
}
#define gDPSetPrimDepth(pkt, z, dz) gDPSetColor(pkt, G_SETPRIMDEPTH, _SHIFTL(z, 16, 16) | _SHIFTL(dz, 0, 16))
#define gDPSetScissor(pkt, mode, ulx, uly, lrx, lry) \
{ \
    Gfx *_g = (Gfx *)pkt; \
    _g->words.w0 = _SHIFTL(G_SETSCISSOR, 24, 8) | _SHIFTL((int)((float)(ulx) * 4.0F), 12, 12) | \
                   _SHIFTL((int)((float)(uly) * 4.0F), 0, 12); \
    _g->words.w1 = _SHIFTL(mode, 24, 2) | _SHIFTL((int)((float)(lrx) * 4.0F), 12, 12) | \
                   _SHIFTL((int)((float)(lry) * 4.0F), 0, 12); \
}
#define gDPFillRectangle(pkt, ulx, uly, lrx, lry) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(G_FILLRECT, 24, 8) | _SHIFTL((lry), 2, 10) | _SHIFTL((lrx), 14, 10)); \
    _g->words.w1 = (_SHIFTL((uly), 2, 10) | _SHIFTL((ulx), 14, 10)); \
}

/* ---- libultra's types (gbi.h, mbi.h) ---- */
typedef s32 Mtx_t[4][4];
typedef union {
    Mtx_t m;
    long long int force_structure_alignment;
} Mtx;
typedef struct {
    short vscale[4];
    short vtrans[4];
} Vp_t;
typedef union {
    Vp_t vp;
    long long int force_structure_alignment[2];
} Vp;

typedef struct {
    unsigned char col[3];
    char pad1;
    unsigned char colc[3];
    char pad2;
    signed char dir[3];
    char pad3;
} Light_t;
typedef union {
    Light_t l;
    long long int force_structure_alignment[2];
} Light;
typedef struct {
    unsigned char col[3];
    char pad1;
    unsigned char colc[3];
    char pad2;
} Ambient_t;
typedef union {
    Ambient_t l;
    long long int force_structure_alignment[1];
} Ambient;
typedef struct Lights2 {
    Ambient a;
    Light l[2];
} Lights2;
typedef struct LookAt {
    Light l[2];
} LookAt;
typedef f32 MtxF[4][4];

/* The frame's buffer of RSP data a view's commands point at (*frame_dynamic): per view its viewport, its vertex (the
 * OBJ_VIEW_VERTEX one), projection and lookat matrices; the reflections' lookats (frame_lookat_count of them); the
 * identity. */
typedef struct {
    Vp vp[4];       /* 0x000 */
    Vtx vtx[4];     /* 0x040 */
    Mtx proj[4];    /* 0x080 */
    Mtx look[4];    /* 0x180 */
    LookAt la[32];  /* 0x280 */
    Mtx ident;      /* 0x680 */
} GfxBuf;

/* A matrix (MAT3 tUVMatrix, as its mat3 view): the rotation's three rows, then the position. */
typedef struct tUVMatrix { f32 uvs[3][3]; f32 pos[3]; } tUVMatrix;

VIEW(GfxBuf *, frame_dynamic);
#define frame_dynamic frame_dynamic_view
VIEW(Mtx *, frame_matrix_cursor);
#define frame_matrix_cursor frame_matrix_cursor_view

float sqrtf(float);
float fabsf(float);

/* gDPSetTileSize with its coordinates masked to their 12 bits (the coordinates here are already 10.2) */
#define gDPSetTileSizeS(pkt, t, uls, ult, lrs, lrt) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (G_SETTILESIZE << 24) | (((uls) & 0xFFF) << 12) | ((ult) & 0xFFF); \
    _g->words.w1 = (((t) & 7) << 24) | (((lrs) & 0xFFF) << 12) | ((lrt) & 0xFFF); \
}

/* Load palette `pal` into the TLUT (colours start..end, at TMEM 0x100 + start), unless it is the one loaded already
 * (zoid_current_tlut). Inside a culled branch (zoid_branch_slot) it always loads: the branch may be skipped, and
 * what follows must not count on what it loads. Advances *gfxp past the commands. */
void zoid_load_palette(Gfx **gfxp, tPaletteDef *pal) {
    Gfx *gfx = *gfxp;

    if (zoid_current_tlut != pal->palBase || zoid_branch_slot != 0) {
        zoid_current_tlut = pal->palBase;
        gDPLoadTLUT(gfx++, pal->end - pal->start + 1, 0x100 + pal->start, pal->palBase);
        *gfxp = gfx;
    }
}

/* The four ways zoid_load_texture loads `tex`: a tile (part of it, `rect`) or the whole block, 4 bits a texel or more. */
#define TILE(fmt, sz) gDPLoadTextureTile(gfx++, tex->texBase, fmt, G_IM_SIZ_##sz, tex->width, tex->height, rect->uls >> 5, rect->ult >> 5, rect->lrs >> 5, rect->lrt >> 5, 0, cms, cmt, masks, maskt, G_TX_NOLOD, G_TX_NOLOD)
#define TILE4(fmt) gDPLoadTextureTile_4b(gfx++, tex->texBase, fmt, tex->width, tex->height, rect->uls >> 5, rect->ult >> 5, rect->lrs >> 5, rect->lrt >> 5, 0, cms, cmt, masks, maskt, G_TX_NOLOD, G_TX_NOLOD)
#define BLOCK(fmt, sz) gDPLoadTextureBlock(gfx++, tex->texBase, fmt, G_IM_SIZ_##sz, tex->width, tex->height, 0, cms, cmt, masks, maskt, G_TX_NOLOD, G_TX_NOLOD)
#define BLOCK4(fmt) gDPLoadTextureBlock_4b(gfx++, tex->texBase, fmt, tex->width, tex->height, 0, cms, cmt, masks, maskt, G_TX_NOLOD, G_TX_NOLOD)

/* Load texture `tex` for what is drawn next: a texture without TEX_IMAGE_DIRECT is loaded by its own display list; else
 * its image, all of it or the tile `rect` (10.5 texels), with its clamp and mirror flags and, for a colour-index one,
 * the TLUT mode set (zoid_tlut_mode keeps the mode last set). The render tile then spans `clip` if given, else
 * `rect`. Its palette (palIndex in `palettes`, the model's palette table; NULL: the caller has set one) is loaded
 * last. Advances *gp past the commands. */
void zoid_load_texture(Gfx **gp, tTexture *tex, TexTile *rect, TexTile *clip, tPaletteDef *palettes)
{
    u16 masks, maskt;
    u16 cms, cmt;
    Gfx *gfx = *gp;

    if (tex == NULL) {
        return;
    }
    if (!(tex->flags & TEX_IMAGE_DIRECT)) {
        gSPDisplayList(gfx++, tex->texBase);
    } else {
        if (rect != NULL) {
            masks = blit_mask_bits(rect->uls - rect->lrs) - 5;
            maskt = blit_mask_bits(rect->ult - rect->lrt) - 5;
        } else {
            masks = blit_mask_bits(tex->width);
            maskt = blit_mask_bits(tex->height);
        }
        if (tex->flags & TEX_CLAMP_S) {
            cms = G_TX_CLAMP;
            masks = 0;
        } else {
            cms = G_TX_WRAP;
        }
        cms = (tex->flags & TEX_MIRROR_S) ? cms | G_TX_MIRROR : cms;
        if (tex->flags & TEX_CLAMP_T) {
            cmt = G_TX_CLAMP;
            maskt = 0;
        } else {
            cmt = G_TX_WRAP;
        }
        cmt = (tex->flags & TEX_MIRROR_T) ? cmt | G_TX_MIRROR : cmt;
        if (tex->format == G_IM_FMT_RGBA) {
            if (zoid_tlut_mode != 0) {
                gDPSetTextureLUT(gfx++, G_TT_NONE);
                zoid_tlut_mode = 0;
            }
            if (tex->size == G_IM_SIZ_16b) {
                if (rect != NULL) {
                    TILE(G_IM_FMT_RGBA, 16b);
                } else {
                    BLOCK(G_IM_FMT_RGBA, 16b);
                }
            } else {
                if (rect != NULL) {
                    TILE(G_IM_FMT_RGBA, 32b);
                } else {
                    BLOCK(G_IM_FMT_RGBA, 32b);
                }
            }
        } else if (tex->format == G_IM_FMT_CI) {
            if (zoid_tlut_mode != 1) {
                gDPSetTextureLUT(gfx++, G_TT_RGBA16);
                zoid_tlut_mode = 1;
            }
            if (tex->size == G_IM_SIZ_8b) {
                if (rect != NULL) {
                    TILE(G_IM_FMT_CI, 8b);
                } else {
                    BLOCK(G_IM_FMT_CI, 8b);
                }
            } else {
                if (rect != NULL) {
                    TILE4(G_IM_FMT_CI);
                } else {
                    BLOCK4(G_IM_FMT_CI);
                }
            }
        } else if (tex->format == G_IM_FMT_I) {
            if (zoid_tlut_mode != 0) {
                gDPSetTextureLUT(gfx++, G_TT_NONE);
                zoid_tlut_mode = 0;
            }
            if (tex->size == G_IM_SIZ_8b) {
                if (rect != NULL) {
                    TILE(G_IM_FMT_I, 8b);
                } else {
                    BLOCK(G_IM_FMT_I, 8b);
                }
            } else {
                if (rect != NULL) {
                    TILE4(G_IM_FMT_I);
                } else {
                    BLOCK4(G_IM_FMT_I);
                }
            }
        } else if (tex->format == G_IM_FMT_IA) {
            if (zoid_tlut_mode != 0) {
                gDPSetTextureLUT(gfx++, G_TT_NONE);
                zoid_tlut_mode = 0;
            }
            if (tex->size == G_IM_SIZ_16b) {
                if (rect != NULL) {
                    TILE(G_IM_FMT_IA, 16b);
                } else {
                    BLOCK(G_IM_FMT_IA, 16b);
                }
            } else if (tex->size == G_IM_SIZ_8b) {
                if (rect != NULL) {
                    TILE(G_IM_FMT_IA, 8b);
                } else {
                    BLOCK(G_IM_FMT_IA, 8b);
                }
            } else {
                if (rect != NULL) {
                    TILE4(G_IM_FMT_IA);
                } else {
                    BLOCK4(G_IM_FMT_IA);
                }
            }
        }
    }
    if (clip != NULL) {
        gfx--;
        gDPSetTileSizeS(gfx++, G_TX_RENDERTILE, clip->uls >> 3, clip->ult >> 3, clip->lrs >> 3, clip->lrt >> 3);
    } else if (rect != NULL) {
        gfx--;
        gDPSetTileSizeS(gfx++, G_TX_RENDERTILE, rect->uls >> 3, rect->ult >> 3, rect->lrs >> 3, rect->lrt >> 3);
    }
    if (tex->palIndex >= 0 && !(tex->flags & TEX_NOPALETTE) && palettes != NULL) {
        zoid_load_palette(&gfx, &palettes[tex->palIndex]);
    }
    *gp = gfx;
}

/* The arcsine of x (flag 0) or its arccosine (flag 1), radians, by Cody and Waite's rational approximation (MAT3
 * asincos.c, farcsine()):
 * |x| above 1/2 is reduced through asin(x) = pi/2 - 2 asin(sqrt((1 - x) / 2)); asin_offsets holds the multiples of
 * pi/4 the reductions add (0 and pi/4, then pi/2 and pi/4 for a negative x's arccosine). Below 2.3e-10 the arcsine is
 * x; from 1 on it is pi/2. */
f32 farcsine(f32 x, s32 flag)
{
    f32 g;
    f32 y;
    f32 r;
    s32 i;

    y = fabsf(x);
    i = flag;
    if (y < 2.3e-10f) {
        r = y;
    } else if (y >= 1.0f) {
        r = 1.5707964f;
    } else {
        if (y > 0.5f) {
            i = 1 - flag;
            g = ((0.5f - y) + 0.5f) / 2.0f;
            y = sqrtf(g);
            y = -(y + y);
        } else {
            g = y * y;
        }
        r = (((((-0.69674575f * g + 10.152522f) * g + -39.688862f) * g + 57.20823f) * g + -27.368494f) * g) /
            (((((g + -23.823858f) * g + 150.95271f) * g + -381.86304f) * g + 417.14432f) * g + -164.21097f) * y + y;
    }
    if (flag != 0) {
        if (x < 0.0f) {
            r = (asin_offsets[i + 2] + r) + asin_offsets[i + 2];
        } else {
            r = (asin_offsets[i] - r) + asin_offsets[i];
        }
    } else {
        r = (asin_offsets[i] + r) + asin_offsets[i];
        if (x < 0.0f) {
            r = -r;
        }
    }
    return r;
}

/* PC: a vertex's float coordinate to s16, out of range for far geometry (32800); undefined in C. Through s32, whose
 * low half the conversion keeps, as the N64's trunc.w.s and sh do (docs/port/SANITIZE.md). */
#define V16 (s32)
/* PC: video.draw_distance and video.detail (docs/port/DETAIL.md 2). PC_DRAW_REACH (a model's last level of detail,
 * PC_LOD_REACH, and the fog's radius), PC_LOD_DIST (the level chosen) and PC_SCENE (the scene's counts) are
 * pc_game_types.h's; a node past its last level is counted, a level forced by its flags (CreateCar's tires with 2-4
 * players) is the first with detail high (the distance its children and the cull read stays the N64's), and a view's
 * walk begins at its visibility. */
#define PC_LOD_CUT else { PC_SCENE(LOD) }
/* a model's last level's distance, as far as the draw distance reaches; not for a node that fades out by then
 * (OBJ_FADE, the car's sheen: alpha 0 past 80 ft), which would spend a lookat and its list on nothing */
#define PC_LOD_REACH(d, flags) (((flags) & OBJ_FADE) ? (d) : PC_DRAW_REACH(d))
#define PC_LOD_FORCED(l) (pc_detail() == PC_DETAIL_HIGH ? 0 : (l))   /* CreateCar's tires at level 1 in split screen */
#define PC_VIEW_BEGIN pc_scene_view();

/* PC: far scenery the draw distance places nowhere in this view (pc_scenery_unplaced) is skipped. */
#define PC_SCENERY_CHECK(view, obj, count) if (pc_scenery_unplaced(view, obj, count)) { PC_SCENE(VIEW_HIDDEN) goto next; }

/* Draw node `node`'s object in view `view`, its matrix already loaded: pick its level of detail (OBJ_FORCE_LOD's, or
 * by its distance from the camera, node_eye_distance: worked out here for the top two levels of the tree, `depth`
 * 0 or 1, unless `use_parent_distance` says the parent's stands; nothing beyond the last level's lodRad times its
 * scale), then set what that level needs (reflection, lighting, palette, texture or tile, colours) and call its
 * display list. `parent` is the node above (depth 1). `palette` is the palette in force from above, loaded again
 * after a level that changes it (LOD_RESET_TLUT). Advances *gp; returns 1 if it drew, else 0. */
s32 zoid_draw_object(Gfx **gp, ObjectInstance *parent, s32 depth, s32 use_parent_distance, ObjectInstance *node, s32 view,
                     tPaletteDef *palette)
{
    Gfx *gfx = *gp;
    tModelDef *def;
    s32 lod;
    TexTile reflection;
    Gfx *dl;
    tCamera *cam;
    f32 d[3];
    s32 drawn = 0;
    f32 dir[3];
    tPaletteDef *palettes;
    u8 alpha;
    f32 x, y, z;
    f32 yaw, pitch;

    def = &gRomObjDefPtrs[node->index >> 10].romObjDefs[node->index & 0x3FF];
    lod = def->lodCount - 1;
    if (node->flags & OBJ_FORCE_LOD) {
        lod = node->flags & OBJ_LOD_MASK;
        if (lod == 0) {
            node_eye_distance = 0.0f;
        } else {
            node_eye_distance = def->lod[lod - 1].lodRad;
        }
    } else if ((def->lod[lod].lodRad != 0.0f && !(node->flags & (OBJ_HIDDEN_VIEW0 << view)) && !use_parent_distance) || (node->flags & OBJ_PARENT_DISTANCE)) {
        if (depth < 2) {
            if (depth > 0) {
                d[0] = parent->mat->pos[0] + node->mat->pos[0];
                d[1] = parent->mat->pos[1] + node->mat->pos[1];
                d[2] = parent->mat->pos[2] + node->mat->pos[2];
                cam = &gCamera[view];
                d[0] = d[0] - cam->mat.pos[0];
                d[1] = d[1] - cam->mat.pos[1];
                d[2] = d[2] - cam->mat.pos[2];
            } else {
                cam = &gCamera[view];
                d[0] = node->mat->pos[0] - cam->mat.pos[0];
                d[1] = node->mat->pos[1] - cam->mat.pos[1];
                d[2] = node->mat->pos[2] - cam->mat.pos[2];
            }
        }
        /* Original game: from depth 2 on, d is never set and the distance comes from whatever it holds (MAT3 the
         * same, past its lvl 1). */
        x = d[0];
        y = d[1];
        z = d[2];
        node_eye_distance = sqrtf(x * x + y * y + z * z);
    }
    if (!(node->flags & (OBJ_HIDDEN_VIEW0 << view))) {
        if (def->lod[lod].lodRad == 0.0f || !(PC_LOD_REACH(def->lod[lod].lodRad, node->flags) * node->scale < node_eye_distance)) {
            if (node->flags & OBJ_FORCE_LOD) {
                lod = PC_LOD_FORCED(node->flags & OBJ_LOD_MASK);
                if (lod >= def->lodCount) {
                    lod = def->lodCount - 1;
                }
            } else if (lod > 0) {
                while (lod != 0 && PC_LOD_DIST(node_eye_distance) < def->lod[lod - 1].lodRad * node->scale) {
                    lod--;
                }
            }
            dl = (Gfx *)def->lod[lod].displayList;   /* the file's list, in the N64's form, or the track map's own */
            if (dl != NULL) {
                if (node->flags & OBJ_ENV_MAPPED) {
                    /* A reflection: texture coordinates generated from the normals by a lookat from the camera to
                     * the node (lookat_reflect_fill), and the texture's tile moved by the direction it is seen from:
                     * 1303.8 = 4096 / pi, so a turn of the yaw moves it 8192 (two texture widths of 128 texels in
                     * 10.5), the pitch likewise. */
                    cam = &gCamera[view];
                    lookat_reflect_fill(&frame_dynamic->la[frame_lookat_count], -cam->mat.pos[0], cam->mat.pos[1], cam->mat.pos[2],
                                    -zoid_node_position[0], zoid_node_position[1], zoid_node_position[2], 0.0f, 1.0f, 0.0f);
                    gSPLookAt(gfx++, &frame_dynamic->la[frame_lookat_count]);
                    gSPSetGeometryMode(gfx++, G_TEXTURE_GEN);
                    frame_lookat_count++;
                    gSPClearGeometryMode(gfx++, G_FOG);
                    gDPPipeSync(gfx++);
                    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
                    if (node->texture[lod] != NULL) {
                        gSPTexture(gfx++, node->texture[lod]->width << 7, node->texture[lod]->height << 6, 0,
                                   G_TX_RENDERTILE, G_ON);
                    }
                    dir[0] = zoid_node_position[0] - cam->mat.pos[0];
                    dir[1] = zoid_node_position[1] - cam->mat.pos[1];
                    dir[2] = zoid_node_position[2] - cam->mat.pos[2];
                    NormalVector(dir);
                    yaw = fatan2(dir[0], dir[2]);
                    reflection.uls = -yaw * 1303.7972f + 6144.0f;
                    pitch = farcsine(dir[1], 0);
                    reflection.ult = -pitch * 1303.7972f + 2048.0f;
                    reflection.lrs = reflection.uls + 4064;
                    reflection.lrt = reflection.ult + 2016;
                    node->tileSize = &reflection;
                }
                if (def->lod[lod].flags & LOD_LIGHTING) {
                    gSPSetGeometryMode(gfx++, G_LIGHTING);
                }
                if (node->palette != NULL) {
                    zoid_load_palette(&gfx, node->palette);
                }
                if (def->lod[lod].flags & LOD_TEXTURED) {
                    palettes = (node->palette != NULL) ? NULL : gPalDefPtrs[node->index >> 10].palDefs;
                    if (node->texture[lod] != NULL) {
                        zoid_load_texture(&gfx, node->texture[lod], node->tile, node->tileSize, palettes);
                    } else if (def->lod[lod].flags & LOD_TEXTURE_SET) {
                        zoid_load_texture(&gfx, (def->lod[lod].texidx & 0x3FF) + gTexDefPtrs[def->lod[lod].texidx >> 10].texDefs, node->tile, node->tileSize, palettes);
                    }
                } else if (node->tileSize != NULL) {
                    gDPSetTileSize(gfx++, G_TX_RENDERTILE, node->tileSize->uls >> 3, node->tileSize->ult >> 3,
                                   node->tileSize->lrs >> 3, node->tileSize->lrt >> 3);
                } else if (node->tile != NULL) {
                    gDPSetTileSize(gfx++, G_TX_RENDERTILE, node->tile->uls >> 3, node->tile->ult >> 3,
                                   node->tile->lrs >> 3, node->tile->lrt >> 3);
                }
                if ((def->lod[lod].flags & LOD_PRIM_COLOR) || (node->flags & OBJ_PRIM_COLOR)) {
                    alpha = OBJ_COLOR(node->color)[3];
                    /* OBJ_FADE: full from 0 to 24 feet, gone at 80 */
                    if ((node->flags & OBJ_FADE) && node_eye_distance > 24.0f) {
                        if (node_eye_distance > 80.0f) {
                            alpha = 0;
                        } else {
                            alpha = alpha * (80.0f - node_eye_distance) / 56.0f;
                        }
                    }
                    gDPSetPrimColor(gfx++, 0, 0, OBJ_COLOR(node->color)[0], OBJ_COLOR(node->color)[1], OBJ_COLOR(node->color)[2], alpha);
                }
                if (node->flags & OBJ_ENV_COLOR) {
                    gDPSetEnvColor(gfx++, OBJ_COLOR(node->envcolor)[0], OBJ_COLOR(node->envcolor)[1], OBJ_COLOR(node->envcolor)[2],
                                   OBJ_COLOR(node->envcolor)[3]);
                }
                gSPDisplayList(gfx++, dl);
                if (zoid_tlut_mode == 0) {
                    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
                    zoid_tlut_mode = 1;
                }
                if (def->lod[lod].flags & LOD_RESET_TLUT) {
                    zoid_current_tlut = 0;
                    if (palette != NULL) {
                        zoid_load_palette(&gfx, palette);
                    }
                }
                drawn = 1; PC_SCENE(DRAWN) PC_SCENE_LEVEL(lod)
                if (node->flags & OBJ_ENV_MAPPED) {
                    gDPPipeSync(gfx++);
                    gDPSetCycleType(gfx++, G_CYC_2CYCLE);
                    gSPSetGeometryMode(gfx++, G_FOG);
                    gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
                    gSPClearGeometryMode(gfx++, G_TEXTURE_GEN);
                    node->tileSize = NULL;
                }
                if (def->lod[lod].flags & LOD_LIGHTING) {
                    gSPClearGeometryMode(gfx++, G_LIGHTING);
                }
            }
        } PC_LOD_CUT
    }
    *gp = gfx;
    return drawn;
}

/* Draw the scene tree in view `view` from `node` along its sibling list, each node with its children (recursively),
 * as display-list commands at *gp. `parent` is the list's parent node (NULL at the root), `depth` how many matrices
 * are pushed above it, `more_after` whether the caller has more to draw after this list (then every node pushes its
 * matrix so it can be popped), `use_parent_distance` whether the levels of detail go by an ancestor's distance
 * (OBJ_PARENT_DISTANCE), `palette` the palette in force from above. A node is skipped when hidden (OBJ_HIDDEN, or in
 * this view with nothing visible below), held back for a late pass (OBJ_LATE_A, OBJ_LATE_B: its index noted in
 * zoid_late_node_a or _b; during that pass only those nodes are drawn), or past the draw range. A culled node
 * (OBJ_CULL_BOX_A/B or its culling box) is drawn inside a branch the RSP skips when the box is off screen, the
 * branch dropped again if nothing in it was drawn. Advances *gp; returns how many objects it drew. */
s32 Traverse(Gfx **gp, ObjectInstance *parent, ObjectInstance *node, s32 view, s32 depth, s32 more_after,
             s32 use_parent_distance, tPaletteDef *palette)
{
    Gfx *gfx = *gp;
    s32 push_matrix;            /* this node's matrix is pushed, to be popped after it and its children */
    s32 in_branch = 0, drawn = 0, drawn_before = 0;
    s32 i, j;
    s32 n;
    Vtx *box;
    tCamera *cam;
    f32 x, z, radius;
    s32 pad[4];                 /* unused */

    for (;;) {
        if (node->flags & OBJ_HIDDEN) {
            PC_SCENE(HIDDEN) goto next;
        }
        if (node->next >= 0 || more_after) {
            push_matrix = 1;
        } else {
            push_matrix = 0;
        }
        if (zoid_late_pass_a == 0 && (node->flags & OBJ_LATE_A)) {
            if (zoid_late_node_a < 0) {
                zoid_late_node_a = node - ObjInstanceList;
            }
            goto next;
        }
        if (zoid_late_pass_b == 0 && (node->flags & OBJ_LATE_B)) {
            if (zoid_late_node_b < 0) {
                zoid_late_node_b = node - ObjInstanceList;
            }
            goto next;
        }
        if (!zoid_node_visible(node, view)) {
            PC_SCENE(VIEW_HIDDEN) goto next;
        }
        if (node->child >= 0 || !(node->flags & (OBJ_HIDDEN_VIEW0 << view))) {
            if (node->flags & OBJ_VIEW_VERTEX) {
                gSPVertex(gfx++, &frame_dynamic->vtx[view], 1, 8);
            }
            if (node->flags & OBJ_NO_MATRIX) {
                push_matrix = 0;
            } else {
                zoid_node_position[0] = node->mat->pos[0];
                zoid_node_position[1] = node->mat->pos[1];
                zoid_node_position[2] = node->mat->pos[2];
                if (node->flags & OBJ_FACE_CAMERA) {
                    if (node->scale != 1.0f) {
                        for (i = 0; i < 3; i++) {
                            for (j = 0; j < 3; j++) {
                                zoid_scaled_matrix[i][j] = zoid_view_matrix[i][j] * node->scale;
                            }
                        }
                        for (j = 0; j < 3; j++) {
                            zoid_scaled_matrix[3][j] = zoid_view_matrix[3][j];
                            zoid_scaled_matrix[j][3] = zoid_view_matrix[j][3];
                        }
                        if (!build_node_matrix_rows4(view, zoid_scaled_matrix, node->mat->pos, frame_matrix_cursor, node->w, depth)) {
                            PC_SCENE(RANGE) goto next;
                        }
                    } else if (!build_node_matrix_rows4(view, zoid_view_matrix, node->mat->pos, frame_matrix_cursor, node->w, depth)) {
                        PC_SCENE(RANGE) goto next;
                    }
                } else if (node->flags & OBJ_UPRIGHT) {
                    if (!build_node_matrix_rows4(view, zoid_upright_matrix, node->mat->pos, frame_matrix_cursor, node->w, depth)) {
                        PC_SCENE(RANGE) goto next;
                    }
                } else if (node->flags & OBJ_FAR) {
                    PC_SCENERY_CHECK(view, node->mat, depth) build_far_node_matrix(view, node->mat, frame_matrix_cursor, node->w, depth);
                } else if (!build_node_matrix(view, node->mat, frame_matrix_cursor, node->w, depth)) {
                    PC_SCENE(RANGE) goto next;
                }
                gSPMatrix(gfx++, frame_matrix_cursor, push_matrix);
                frame_matrix_cursor++;
            }
            if (node->numLights >= 0) {
                switch (node->numLights) {
                    case 0:
                        gSPSetLights0(gfx++, (*node->lights));
                        break;
                    case 1:
                        gSPSetLights1(gfx++, (*node->lights));
                        break;
                    case 2:
                        gSPSetLights2(gfx++, (*node->lights));
                        break;
                }
            }
            if ((node->flags & OBJ_CULL_BOX_A) || (node->flags & OBJ_CULL_BOX_B)) {
                zoid_branch_slot = gfx + 2;
                gSPDisplayList(gfx++, zoid_branch_slot);
                gSPBranchList(gfx++, NULL);
                in_branch = 1;
                drawn_before = drawn;
                drawn = 0;
                if (node->flags & OBJ_CULL_BOX_A) {
                    gSPVertex(gfx++, cull_box_vertices_a, 8, 0);
                } else {
                    gSPVertex(gfx++, cull_box_vertices_b, 8, 0);
                }
                gSPCullDisplayList(gfx++, 0, 7);
            } else if (node->culling & 7) {
                /* Its culling box (ZOID_BuildCullingList). Out of the fog's reach (gViewableDistance, short of 2,000
                 * feet: a foggy track) by more than the box's horizontal radius, it is not drawn, only its
                 * children are visited. */
                n = node->culling & 7;
                box = &cull_vertices[(node->culling & 0x1FF8) >> 3];
                if (!(node->flags & OBJ_FAR) && gViewableDistance < 2000.0f) {
                    cam = &gCamera[view];
                    x = (box[0].v.ob[0] < box[7].v.ob[0]) ? box[7].v.ob[0] : box[0].v.ob[0];
                    z = (box[0].v.ob[2] < box[7].v.ob[2]) ? box[7].v.ob[2] : box[0].v.ob[2];
                    radius = sqrtf(x * x + z * z) * 0.0625f;
                    x = node->mat->pos[0] - cam->mat.pos[0];
                    z = node->mat->pos[2] - cam->mat.pos[2];
                    if (PC_DRAW_REACH(gViewableDistance) < sqrtf(x * x + z * z) - radius) {
                        PC_SCENE(RADIUS) goto children;
                    }
                }
                zoid_branch_slot = gfx + 2;
                gSPDisplayList(gfx++, zoid_branch_slot);
                gSPBranchList(gfx++, NULL);
                zoid_current_tlut = 0;
                in_branch = 1;
                zoid_current_texture = NULL;
                drawn_before = drawn;
                gSPVertex(gfx++, box, n + 1, 0);
                gSPCullDisplayList(gfx++, 0, n);
                drawn = 0;
            }
            drawn += zoid_draw_object(&gfx, parent, depth, use_parent_distance, node, view, (node->palette != NULL) ? node->palette : palette);
        } else {
            push_matrix = 0;
        }
    children:
        if (node->child >= 0) {
            drawn += Traverse(&gfx, node, &ObjInstanceList[node->child], view,
                                          (push_matrix == 1) ? depth + 1 : depth, node->next >= 0,
                                          (node->flags & OBJ_PARENT_DISTANCE) | use_parent_distance, (node->palette != NULL) ? node->palette : palette);
        }
        if (in_branch) {
            in_branch = 0;
            if (drawn == 0) {
                gfx = zoid_branch_slot - 2;
            } else {
                gSPEndDisplayList(gfx++);
                zoid_branch_slot[-1].words.w1 = (uintptr_t)gfx;
            }
            zoid_branch_slot = NULL;
            drawn += drawn_before;
            drawn_before = 0;
        }
        if (push_matrix == 1) {
            if (drawn == 0) {
                gfx--;
            } else {
                gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
            }
        }
    next:
        if (node->next < 0) {
            break;
        }
        node = &ObjInstanceList[node->next];
        if (zoid_late_pass_a != 0 && !(node->flags & OBJ_LATE_A)) {
            goto next;
        }
        if (zoid_late_pass_b != 0 && !(node->flags & OBJ_LATE_B)) {
            goto next;
        }
    }
    *gp = gfx;
    return drawn;
}

/* A window (MAT3 tWindowInfo; 0x48 bytes on the N64, 0x50 on the PC, whose two pointers are 8 bytes; offsets
 * N64's). */
typedef struct tWindowInfo {
    Vp *vp;                     /* 0x00 its viewport */
    u16 *s;                     /* 0x04 its scissor rectangle: left, top, right, bottom */
    s32 flags;                  /* 0x08 1: orthographic (no field of view given) */
    u8 pad0C[4];
    f32 wvviewang;              /* 0x10 the vertical field of view, radians */
    u8 pad14[0x10];
    f32 wwidth;                 /* 0x24 the size, pixels */
    f32 wheight;                /* 0x28 */
    f32 whcenter;               /* 0x2C the centre, pixels */
    f32 wvcenter;               /* 0x30 */
    f32 aspect;                 /* 0x34 */
    f32 frontclip;              /* 0x38 the near and far planes, feet */
    f32 backclip;               /* 0x3C */
    u16 fogmin;                 /* 0x40 the fog's range */
    u16 fogmax;                 /* 0x42 */
    u8 fogcolor[4];             /* 0x44 */
} tWindowInfo;

/* A flat polygon's vertex (20 bytes): position (scene units, 16 a foot, or screen pixels), texture coordinates,
 * colour. */
typedef struct {
    f32 pos[3];
    s16 tc[2];
    u8 cn[4];
} PolyVtx;

/* A flat polygon (MAT3 tPoly, 0x58 bytes). A POLY_SCREEN one is a rectangle from its first vertex's x and y to its
 * second's, in its first vertex's colour (`screen`). */
typedef struct tPoly {
    s16 nverts;                 /* 0x00 0: free */
    u16 flags;                  /* 0x02 POLY_* */
    u16 texidx;                 /* 0x04 its texture, a handle */
    s16 z;                      /* 0x06 its depth with POLY_FIXED_DEPTH */
    union {
        PolyVtx v[4];
        struct {
            f32 x0, y0;         /* 0x08 the first corner */
            u8 pad10[8];
            u8 col[4];          /* 0x18 the colour */
            f32 x1, y1;         /* 0x1C the opposite corner */
        } screen;
    } u;
} tPoly;


void *memcpy(void *, const void *, unsigned int);

/* Draw window `view` seen from camera `eye` (gCamera[view]): the world's visibility for the camera's place
 * (WorldCullObjects), the window's viewport, scissor and fog, the projection (perspective, or orthographic for a
 * window with flags 1; a near plane at frontclip and a far one at backclip, in the scene's units of 1/16 foot) and the
 * camera's lookat, mirrored for a mirrored track (gMirrorWaves) and rolled over for an upside-down one; the camera-
 * facing matrices; then the scene tree (Traverse), the flat polygons shown in this view (POLY_VIEW0 << view), and the
 * two late passes. */
void zoid_draw_window(s32 view, tCamera *eye)
{
    tWindowInfo *win = &ZoidWinInfo[view];
    Vp *vp = &frame_dynamic->vp[view];
    tPoly *poly;
    u16 ulx, uly, lrx, lry;
    s32 mode;                   /* the polygons' render mode set: -1 none, 0 fill, 1 shaded, 2 textured z-buffered,
                                 * 3 textured */
    s32 fog;                    /* the fog is on */
    s32 depth;                  /* the primitive depth set, -1 the pixels' own depth */
    s32 i;

    PC_VIEW_BEGIN WorldCullObjects(view, eye->mat.pos, 0);
    memcpy(vp, win->vp, sizeof(Vp));
    vp->vp.vscale[0] = (s16)win->wwidth * 2;
    vp->vp.vscale[1] = (s16)win->wheight * 2;
    vp->vp.vtrans[0] = (s16)win->whcenter * 4;
    vp->vp.vtrans[1] = (s16)win->wvcenter * 4;
    ulx = win->s[0];
    uly = win->s[1];
    lrx = win->s[2];
    lry = win->s[3];
    gSPViewport(frame_dl++, &frame_dynamic->vp[view]);
    gDPPipeSync(frame_dl++);
    gDPSetScissor(frame_dl++, G_SC_NON_INTERLACE, ulx, uly, lrx, lry);
    gDPSetFogColor(frame_dl++, win->fogcolor[0], win->fogcolor[1], win->fogcolor[2], win->fogcolor[3]);
    gSPFogPosition(frame_dl++, win->fogmin, win->fogmax);
    gDPSetCycleType(frame_dl++, G_CYC_2CYCLE);
    gSPSetGeometryMode(frame_dl++, G_FOG);
    depth = -1;
    gDPSetDepthSource(frame_dl++, G_ZS_PIXEL);
    if (zoid_tlut_mode != 1) {
        gDPSetTextureLUT(frame_dl++, G_TT_RGBA16);
        zoid_tlut_mode = 1;
    }
    if (win->flags) {
        Mtx *m = &frame_dynamic->proj[view];

        guOrtho(m, -win->wwidth * 0.5f, win->wwidth * 0.5f, -win->wheight * 0.5f, win->wheight * 0.5f,
                win->frontclip * 16.0f, win->backclip * 16.0f, 1.0f);
    } else {
        u16 perspNorm;

        guPerspective(&frame_dynamic->proj[view], &perspNorm, win->wvviewang * 57.295776f, win->aspect, win->frontclip * 16.0f,
                      win->backclip * 16.0f, 1.0f);
        gSPPerspNormalize(frame_dl++, perspNorm);
    }
    gSPMatrix(frame_dl++, &frame_dynamic->proj[view], G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    if (gUpsideDownTrack) {
        f32 uv[3][3];

        CopyUV(gCamera[view].mat.uvs, uv);
        RollUV(3.1415927f, uv);
        if (gMirrorWaves) {
            uv[2][0] *= -1.0f;
            uv[1][0] *= -1.0f;
        }
        guLookAtF(zoid_lookat_matrix, 0.0f, 0.0f, 0.0f, -uv[2][0], uv[2][1], uv[2][2], -uv[1][0], uv[1][1], uv[1][2]);
    } else if (gMirrorWaves) {
        guLookAtF(zoid_lookat_matrix, 0.0f, 0.0f, 0.0f, -(-gCamera[view].mat.uvs[2][0]), gCamera[view].mat.uvs[2][1],
                  gCamera[view].mat.uvs[2][2], -(-gCamera[view].mat.uvs[1][0]), gCamera[view].mat.uvs[1][1],
                  gCamera[view].mat.uvs[1][2]);
    } else {
        guLookAtF(zoid_lookat_matrix, 0.0f, 0.0f, 0.0f, -gCamera[view].mat.uvs[2][0], gCamera[view].mat.uvs[2][1],
                  gCamera[view].mat.uvs[2][2], -gCamera[view].mat.uvs[1][0], gCamera[view].mat.uvs[1][1],
                  gCamera[view].mat.uvs[1][2]);
    }
    guLookAtF(zoid_view_matrix, gCamera[view].mat.uvs[2][0], gCamera[view].mat.uvs[2][1], gCamera[view].mat.uvs[2][2], 0.0f,
              0.0f, 0.0f, gCamera[view].mat.uvs[1][0], gCamera[view].mat.uvs[1][1], gCamera[view].mat.uvs[1][2]);
    if (gGlobalCameraFacingScale != 1) {
        s32 j;

        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) zoid_view_matrix[i][j] *= gGlobalCameraFacingScale;
        }
    }
    zoid_build_upright_matrix(zoid_upright_matrix[0]);
    /* The view's vertex (OBJ_VIEW_VERTEX): zoid_view_matrix's third column times 480 with one player, else the
     * origin. */
    i = gNumPlayers;
    if ((i < 2) != 0) {
        frame_dynamic->vtx[view].v.ob[0] = zoid_view_matrix[0][2] * 480.0f;
        frame_dynamic->vtx[view].v.ob[1] = zoid_view_matrix[1][2] * 480.0f;
        frame_dynamic->vtx[view].v.ob[2] = zoid_view_matrix[2][2] * 480.0f;
    } else {
        frame_dynamic->vtx[view].v.ob[0] = 0;
        frame_dynamic->vtx[view].v.ob[1] = 0;
        frame_dynamic->vtx[view].v.ob[2] = 0;
    }
    mtx_rotation_to_fixed(zoid_lookat_matrix, (u32 *)&frame_dynamic->look[view]);
    gSPMatrix(frame_dl++, &frame_dynamic->look[view], G_MTX_PROJECTION | G_MTX_MUL | G_MTX_NOPUSH);
    guMtxIdent(&frame_dynamic->ident);
    /* The modelview starts as the identity with x negated (s15.16 -1.0: the game's x runs the other way from the
     * RSP's), but for a mirrored track, which leaves x as it is and so mirrors the picture. */
    if (!gMirrorWaves) {
        frame_dynamic->ident.m[0][0] = 0xFFFF0000;
    }
    gSPMatrix(frame_dl++, &frame_dynamic->ident, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    zoid_branch_slot = NULL;
    zoid_late_pass_a = 0;
    zoid_late_node_a = -1;
    zoid_late_pass_b = 0;
    zoid_late_node_b = -1;
    if (ObjHead >= 0) {
        Traverse(&frame_dl, NULL, &ObjInstanceList[ObjHead], view, 0, 1, 0, NULL);
        zoid_tlut_mode = -1;
        zoid_current_texture = NULL;
    }
    fog = 1;
    poly = PolyInstanceList;
    mode = -1;
    for (i = 0; i < gNumPolyInsts; i++, poly++) {
        s32 j;

        if (poly->flags & POLY_HIDDEN) {
            continue;
        }
        if (!(poly->flags & (POLY_VIEW0 << view))) {
            continue;
        }
        if (poly->nverts == 0) {
            continue;
        }
        gDPPipeSync(frame_dl++);
        if (poly->flags & POLY_FIXED_DEPTH) {
            if (depth < 0) {
                gDPSetDepthSource(frame_dl++, G_ZS_PRIM);
                if (depth != poly->z) {
                    gDPSetPrimDepth(frame_dl++, poly->z, 0);
                }
                depth = poly->z;
            }
        } else if (depth >= 0) {
            depth = -1;
            gDPSetDepthSource(frame_dl++, G_ZS_PIXEL);
        }
        if (poly->flags & POLY_SCREEN) {
            if (mode != 0) {
                mode = 0;
                gDPSetRenderMode(frame_dl++, 0x00504240, 0);
                gDPSetCombine(frame_dl++, 0xFFFFFF, 0xFFFDF6FB);
            }
            gDPSetPrimColor(frame_dl++, 0, 0, poly->u.screen.col[0], poly->u.screen.col[1], poly->u.screen.col[2],
                            poly->u.screen.col[3]);
            gDPFillRectangle(frame_dl++, (s16)poly->u.screen.x0, (s16)poly->u.screen.y0, (s16)poly->u.screen.x1,
                             (s16)poly->u.screen.y1);
            gDPPipeSync(frame_dl++);
            continue;
        }
        for (j = 0; j < poly->nverts; j++) {
            f32 p[3];

            /* Each vertex relative to the camera, in the scene's units, as an s16: a polygon out of that range
             * (2,048 feet) is skipped. */
            if (!(poly->flags & POLY_CAMERA_FRAME)) {
                p[0] = poly->u.v[j].pos[0] - gCamera[view].mat.pos[0] * 16.0f;
                p[1] = poly->u.v[j].pos[1] - gCamera[view].mat.pos[1] * 16.0f;
                p[2] = poly->u.v[j].pos[2] - gCamera[view].mat.pos[2] * 16.0f;
                if (p[0] <= -32768.0f || 32768.0f <= p[0] || p[1] <= -32768.0f || 32768.0f <= p[1] ||
                    p[2] <= -32768.0f || 32768.0f <= p[2]) {
                    goto skip;
                }
            } else {
                WorldVector(poly->u.v[j].pos, p, (f32 *)&gCamera[view]);
            }
            zoid_poly_vertices[j].v.ob[0] = V16 p[0];
            zoid_poly_vertices[j].v.ob[1] = V16 p[1];
            zoid_poly_vertices[j].v.ob[2] = V16 p[2];
            memcpy((j + zoid_poly_vertices)->v.tc, poly->u.v[j].tc, 8);
        }
        if ((poly->flags & POLY_NO_FOG) && fog) {
            fog = 0;
            mode = -1;
            gSPClearGeometryMode(frame_dl++, G_FOG);
            gDPPipeSync(frame_dl++);
            gDPSetCycleType(frame_dl++, G_CYC_1CYCLE);
        } else if (!(poly->flags & POLY_NO_FOG) && !fog) {
            fog = 1;
            mode = -1;
            gSPSetGeometryMode(frame_dl++, G_FOG);
            gDPPipeSync(frame_dl++);
            gDPSetCycleType(frame_dl++, G_CYC_2CYCLE);
        }
        if (fog) {
            gDPSetPrimColor(frame_dl++, 0, 0, poly->u.screen.col[0], poly->u.screen.col[1], poly->u.screen.col[2],
                            poly->u.screen.col[3]);
        }
        if (poly->flags & POLY_TEXTURED) {
            tTexture *tex;

            tex = &gTexDefPtrs[poly->texidx >> 10].texDefs[poly->texidx & 0x3FF];
            if (tex != zoid_current_texture) {
                zoid_load_texture(&frame_dl, &gTexDefPtrs[poly->texidx >> 10].texDefs[poly->texidx & 0x3FF], NULL, NULL,
                                    gPalDefPtrs[poly->texidx >> 10].palDefs);
                zoid_current_texture = tex;
            }
            if (poly->flags & 0x200) {     /* z-buffered (the render modes with Z_CMP and Z_UPD) */
                if (mode != 2) {
                    mode = 2;
                    if (!fog) {
                        gDPSetRenderMode(frame_dl++, 0x00504A70, 0);
                        gDPSetCombine(frame_dl++, 0x121824, 0xFF33FFFF);
                    } else {
                        gDPSetRenderMode(frame_dl++, 0xC8104A50, 0);
                        gDPSetCombine(frame_dl++, 0x1217FF, 0xFFFFFE38);
                    }
                }
            } else if (mode != 3) {
                mode = 3;
                if (!fog) {
                    gDPSetRenderMode(frame_dl++, 0x00504240, 0);
                    gDPSetCombine(frame_dl++, 0x121824, 0xFF33FFFF);
                } else {
                    gDPSetRenderMode(frame_dl++, 0xC8104240, 0);
                    gDPSetCombine(frame_dl++, 0x1217FF, 0xFFFFFE38);
                }
            }
        } else if (mode != 1) {
            mode = 1;
            if (!fog) {
                gDPSetRenderMode(frame_dl++, 0x00504A50, 0);
                gDPSetCombine(frame_dl++, 0xFFFFFF, 0xFFFE793C);
            } else {
                gDPSetRenderMode(frame_dl++, 0xC8104A70, 0);
                gDPSetCombine(frame_dl++, 0xFFFFFF, 0xFFFE7638);
            }
        }
        gSPVertex(frame_dl++, zoid_poly_vertices, poly->nverts, 0);
        zoid_poly_vertices += poly->nverts;
        if (poly->flags & POLY_TWO_SIDED) {
            gSP2Triangles(frame_dl++, 0, 2, 1, 0, 2, 0, 1, 0);
            if (poly->nverts >= 4) {
                gSP2Triangles(frame_dl++, 2, 0, 3, 0, 0, 2, 3, 0);
            }
        } else if (gMirrorWaves) {
            if (poly->nverts == 3) {
                gSP1Triangle(frame_dl++, 1, 0, 2, 0);
            } else {
                gSP2Triangles(frame_dl++, 1, 0, 2, 0, 0, 3, 2, 0);
            }
        } else {
            if (poly->nverts == 3) {
                gSP1Triangle(frame_dl++, 2, 0, 1, 0);
            } else {
                gSP2Triangles(frame_dl++, 2, 0, 1, 0, 0, 2, 3, 0);
            }
        }
    skip:;
    }
    if (zoid_tlut_mode != 1) {
        gDPSetTextureLUT(frame_dl++, G_TT_RGBA16);
        zoid_tlut_mode = 1;
    }
    if (depth >= 0) {
        gDPSetDepthSource(frame_dl++, G_ZS_PIXEL);
    }
    if (!fog) {
        gSPSetGeometryMode(frame_dl++, G_FOG);
        gDPPipeSync(frame_dl++);
        gDPSetCycleType(frame_dl++, G_CYC_2CYCLE);
    }
    if (zoid_late_node_a >= 0) {
        ObjectInstance *n = &ObjInstanceList[zoid_late_node_a];

        zoid_late_pass_a = 1;
        zoid_late_pass_b = 0;
        Traverse(&frame_dl, NULL, n, view, 0, 1, 0, NULL);
        zoid_tlut_mode = -1;
        zoid_current_texture = NULL;
    }
    if (zoid_late_node_b >= 0) {
        ObjectInstance *n = &ObjInstanceList[zoid_late_node_b];

        zoid_late_pass_a = 0;
        zoid_late_pass_b = 1;
        Traverse(&frame_dl, NULL, n, view, 0, 1, 0, NULL);
        zoid_tlut_mode = -1;
        zoid_current_texture = NULL;
    }
}

/* ---- ZOID_EndFrame: the frame's scene and its 2D blits ---- */
#define gDPSetTextureFilter(pkt, type) gSPSetOtherMode(pkt, G_SETOTHERMODE_H, G_MDSFT_TEXTFILT, 2, type)


/* Draw the frame's scene into frame_dl: the scene's lights, every window (zoid_draw_window), then, unless
 * `skip_blits`, the 2D blit instances in order (gBlitInstanceList, gNumBlitInsts of them) through the blitter: a
 * fill or a texture, cut into loads of as many rows as the texture memory holds, flipped in y unless the instance's
 * flag 8 says not, its rows doubled in the high-resolution (interlaced) modes. MAT3's ZOID_EndFrame has no argument;
 * the callers pass 0. */
void ZOID_EndFrame(s32 skip_blits)
{
    s32 i;

    zoid_current_tlut = 0;
    zoid_tlut_mode = -1;
    zoid_current_texture = NULL;
    gSPClearGeometryMode(frame_dl++, G_LIGHTING | G_TEXTURE_GEN);
    frame_lookat_count = 0;
    if (gNumLights >= 0) {
        switch (gNumLights) {
        case 0:
            gSPSetLights0(frame_dl++, (*gLights));
            break;
        case 1:
            gSPSetLights1(frame_dl++, (*gLights));
            break;
        case 2:
            gSPSetLights2(frame_dl++, (*gLights));
            break;
        }
    }
    zoid_poly_vertices = zoid_poly_vertex_buffers[frame_record_index];
    for (i = 0; i < gNumZoidWindows; i++) {
        zoid_draw_window(i, &gCamera[i]);
    }
    gSPClearGeometryMode(frame_dl++, G_FOG);
    gDPPipeSync(frame_dl++);
    gDPSetScissor(frame_dl++, G_SC_NON_INTERLACE, 0, 0, screen_width, screen_height);
    gDPSetDepthSource(frame_dl++, G_ZS_PIXEL);
    gDPSetCycleType(frame_dl++, G_CYC_1CYCLE);
    gDPSetTextureFilter(frame_dl++, G_TF_POINT);
    if (skip_blits != 0) {
        return;
    }
    if (screen_height > 240) {
        BlitSetAttribute(0x8000);
    }
    for (i = 0; i < gNumBlitInsts; i++) {
        tBlitInstance *blit;
        tTexture *tex;
        tPaletteDef *pal;
        s32 h;                  /* unused */
        s32 sw;                 /* the width drawn, texels */
        s32 th;                 /* the height drawn, texels */
        s32 lines;              /* a fill's bottom edge; then the rows a load holds */
        s32 x;
        s32 y;
        s32 tile;               /* load as a tile, not a block (a width the block load can't take) */
        s32 t;                  /* the next texel row to load */
        s32 next;               /* unused */
        s32 flip;               /* a fill's right edge; then the texture's width; then flipped in y */
        s32 x1;                 /* unused */

        blit = &gBlitInstanceList[i];
        tile = 0;
        tex = blit->texture;
        if (blit->hide) {
            continue;
        }
        if (blit->flags & 0x80) {
            if (blit->texture != NULL) {
                ((void (*)(tPaletteDef *))blit->texture)(blit->palette);
            }
            continue;
        }
        if (blit->z != 0) {
            BlitZDist(blit->z);
        } else {
            BlitClearAttribute(16);
        }
        if (blit->flags & 4) {
            BlitSetAttribute(4);
        } else {
            BlitClearAttribute(4);
        }
        if (blit->width == 0 || blit->height == 0) {
            continue;
        }
        if (tex == NULL) {
            if (blit->alpha == 255) {
                BlitClearAttribute(32);
            } else {
                BlitXLU(blit->alpha);
            }
            x = blit->x;
            y = blit->y;
            flip = blit->x + blit->width;
            lines = blit->y + blit->height;
            BlitScissor(x, y, flip - 1, lines - 1);
            BlitRect(x, y, flip, lines, (u8 *)blit->palette);
            continue;
        }
        x = blit->x;
        y = blit->y;
        lines = blit->height;
        if (screen_height > 240) {
            lines = lines + lines;
        }
        BlitScissor(x, y, x + blit->width - 1, y + lines - 1);
        sw = blit->right - blit->left + 1;
        th = blit->bot - blit->top + 1;
        flip = tex->width;
        lines = blit_tile_lines[flip >> (4 - tex->size)];
        if (tex->format == 2 || tex->format == 5) {
            lines >>= 1;
        }
        if (lines < 2 || ((15 >> tex->size) & flip)) {
            tile = 1;
            if (tex->size == 0) {
                lines = 4096 / tex->width;
            } else if (tex->size == 1 || tex->size == 2) {
                lines = 2048 / tex->width;
            } else {
                lines = 1024 / tex->width;
            }
        }
        flip = 0;
        if (blit->flags & 8) {
            BlitClearAttribute(8);
        } else {
            flip = 1;
            BlitSetAttribute(8);
        }
        if (blit->flags & 1) {
            BlitSetAttribute(1);
        }
        if (blit->alpha == 255) {
            BlitClearAttribute(32);
        } else {
            BlitXLU(blit->alpha);
        }
        if (blit->palette != NULL) {
            /* a palette the game filled in for this blit (healthPal, n64FogPal) */
            BlitColors(((tPaletteDef *)blit->palette)->palBase, tex->format, tex->size, 0);
        } else if (tex->palIndex >= 0) {
            pal = &gPalDefPtrs[blit->texIndex >> 10].palDefs[tex->palIndex];
            BlitColors(pal->palBase, tex->format, tex->size, 0);
        } else if (tex->format == 3) {
            blit_ia_palette[3] = blit->alpha;
            BlitColors(blit_ia_palette, tex->format, tex->size, 0);
        } else if (tex->format != 0) {
            if (gPalDefPtrs[blit->texIndex >> 10].count != 0) {
                pal = gPalDefPtrs[blit->texIndex >> 10].palDefs;
                BlitColors(pal->palBase, tex->format, tex->size, 0);
            }
        }
        if (blit->bot >= tex->height) {
            blit->bot = tex->height - 1;
        }
        if (blit->top < 0) {
            t = 0;
        } else {
            t = blit->top;
        }
        if (th < lines) {
            lines = th;
        }
        if (flip) {
            while (t < blit->bot) {
                if (blit->bot < t + lines) {
                    lines = blit->bot - t + 1;
                }
                blit_load_texture(tex->texBase, tex->format, tex->size, tex->width, tex->height, 0, tex->height - (t + lines), sw - 1, tex->height - t - 1, 0, tile);
                BlitDraw(x, y, x + sw - 1, y + lines - 1, blit->left, tex->height - t - lines);
                if (screen_height > 240) {
                    y += lines;
                }
                t += lines;
                y += lines;
            }
        } else {
            while (t < blit->bot) {
                if (blit->bot < t + lines) {
                    lines = blit->bot - t + 1;
                }
                blit_load_texture(tex->texBase, tex->format, tex->size, tex->width, tex->height, 0, t, sw - 1, t + lines - 1, 0, tile);
                BlitDraw(x, y, x + sw - 1, y + lines - 1, blit->left, t);
                if (screen_height > 240) {
                    y += lines;
                }
                t += lines;
                y += lines;
            }
        }
        if (blit->flags & 1) {
            BlitClearAttribute(1);
        }
    }
}
/* The arcsine of `input`, radians (farcsine). */
f32 fasin(f32 input) { return farcsine(input, 0); }
/* The arccosine of `input`, radians (farcsine). */
f32 facos(f32 input) { return farcsine(input, 1); }

f32 sinf(f32);

/* InterpQuats (MAT3 fmath.c, from the arcade's LIB/fmath.c with its spherical branch on): the quaternion `frac` of
 * the way from q1 to q2 (0..1) into qr. Near the ends (frac under 0.001, over 0.999) it copies one; q2 is negated if
 * that is the shorter way. Far apart (their dot product, times 0.999, under 0.98) it is a spherical interpolation,
 * else a linear blend of the components with each wrapped to -1..1. The moving targets'
 * paths use it (PlacePathTargetQuat). */
void InterpQuats(f32 frac, f32 *q1, f32 *q2, f32 *qr)
{
    f32 dp, theta;
    f32 sint, isint;
    f32 X1, X2;
    s32 i;

    if (frac < .001f) {
        qr[0] = q1[0], qr[1] = q1[1], qr[2] = q1[2], qr[3] = q1[3];
        return;
    } else if (frac > .999f) {
        qr[0] = q2[0], qr[1] = q2[1], qr[2] = q2[2], qr[3] = q2[3];
        return;
    }
    dp = (q1[0] * q2[0] + q1[1] * q2[1] + q1[2] * q2[2] + q1[3] * q2[3]) * .999f;
    if (dp < 0.0f) {
        qr[0] = -q2[0];
        qr[1] = -q2[1];
        qr[2] = -q2[2];
        qr[3] = -q2[3];
        dp = -dp;
    } else {
        qr[0] = q2[0], qr[1] = q2[1], qr[2] = q2[2], qr[3] = q2[3];
    }
    if (dp < .98f) {
        theta = facos(dp);
        sint = sinf(theta);
        isint = 1.0f / sint;
        X1 = sinf((1 - frac) * theta) * isint;
        X2 = sinf(frac * theta) * isint;
        for (i = 0; i < 4; i++) {
            qr[i] = q1[i] * X1 + qr[i] * X2;
        }
    } else {
        for (i = 0; i < 4; i++) {
            X1 = qr[i] - q1[i];
            if (X1 > 1.0f)
                X1 -= 2.0f;
            if (X1 < -1.0f)
                X1 += 2.0f;
            qr[i] = q1[i] + frac * (X1);
            if (qr[i] > 1.0f)
                qr[i] -= 2.0f;
            else if (qr[i] < -1.0f)
                qr[i] += 2.0f;
        }
    }
}
