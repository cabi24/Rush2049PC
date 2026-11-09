#include "game/game.h"
#define Gfx GfxView   /* this file's own Gfx: include/game's Gfx is another type */

typedef union {
    Gwords words;
    long long int force_structure_alignment;
} Gfx;

/* SDK forms, reference/repos/ultralib/include/PR/gbi.h */
#define _SHIFTL(v, s, w) ((u32)(((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define G_SETTIMG 0xFD
#define G_SETTILE 0xF5
#define G_LOADBLOCK 0xF3
#define G_LOADTILE 0xF4
#define G_SETTILESIZE 0xF2
#define G_RDPLOADSYNC 0xE6
#define G_RDPPIPESYNC 0xE7
#define G_SETOTHERMODE_H 0xE3
#define G_MDSFT_TEXTLUT 14
#define G_TT_NONE (0 << G_MDSFT_TEXTLUT)
#define G_TT_RGBA16 (2 << G_MDSFT_TEXTLUT)
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
#define G_TX_LOADTILE 7
#define G_TX_RENDERTILE 0
#define G_TX_NOMIRROR 0
#define G_TX_WRAP 0
#define G_TX_MIRROR 0x1
#define G_TX_CLAMP 0x2
#define G_TX_NOMASK 0
#define G_TX_NOLOD 0
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define G_TX_DXT_FRAC 11
#define G_TX_LDBLK_MAX_TXL 2047
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
#define G_DL 0xDE
#define G_DL_PUSH 0x00
#define gDma1p(pkt, c, s, l, p) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL((c), 24, 8) | _SHIFTL((p), 16, 8) | _SHIFTL((l), 0, 16)); \
    _g->words.w1 = (uintptr_t)(s); \
}
#define gSPDisplayList(pkt,dl) gDma1p(pkt,G_DL,dl,0,G_DL_PUSH)
#define RDL_STORAGE
#define TEX_NOPALETTE 0x20000000
#define TEX_TEXTURE 0x08000000
#define TEX_MIRROR_T 0x00080000
#define TEX_MIRROR_S 0x00040000
#define TEX_CLAMP_T 0x00020000
#define TEX_CLAMP_S 0x00010000
/* PC: Texture, Palette and LodSet are a WAD's TXHD, PLHD and OBHD records, which keep their N64 layout
 * (port/assets/wad.c): image, table and dl are 32-bit RDRAM-window addresses, unsigned so they widen to host
 * pointers by zero extension. */
#define WADP(type) u32
#define WAD_ADDR u32
typedef struct Texture {
    u8 pad0[16];
    u16 width;
    u16 height;
    u8 siz;
    u8 fmt;
    s16 palette;
    WADP(void) image;
    u32 flags;
    u32 unk20;
} Texture;
typedef struct TexRect {
    u16 x0, y0, x1, y1;
} TexRect;
typedef struct Palette {
    u8 pad0[16];
    u8 first;
    u8 last;
    u8 pad12[2];
    WAD_ADDR table;
} Palette;

#define G_LOADTLUT 0xF0
#define G_RDPTILESYNC 0xE8
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


/* ---- F3DEX2 forms (reference/repos/ultralib/include/PR/gbi.h, F3DEX_GBI_2) ---- */
#define G_VTX 0x01
#define G_CULLDL 0x03
#define G_TRI1 0x05
#define G_TRI2 0x06
#define G_TEXTURE 0xD7
#define G_POPMTX 0xD8
#define G_GEOMETRYMODE 0xD9
#define G_MTX 0xDA
#define G_MOVEWORD 0xDB
#define G_MOVEMEM 0xDC
#define G_ENDDL 0xDF
#define G_SETOTHERMODE_L 0xE2
#define G_SETPRIMDEPTH 0xEE
#define G_SETSCISSOR 0xED
#define G_FILLRECT 0xF6
#define G_SETFOGCOLOR 0xF8
#define G_SETPRIMCOLOR 0xFA
#define G_SETENVCOLOR 0xFB
#define G_SETCOMBINE 0xFC
#define G_DL_NOPUSH 0x01
#define G_MV_VIEWPORT 8
#define G_MV_LIGHT 10
#define G_MVO_LOOKATX 0
#define G_MVO_LOOKATY 24
#define G_MW_NUMLIGHT 0x02
#define G_MW_FOG 0x08
#define G_MW_PERSPNORM 0x0E
#define G_MWO_NUMLIGHT 0x00
#define G_MWO_FOG 0x00
#define NUML(n) ((n) * 24)
#define NUMLIGHTS_0 1
#define NUMLIGHTS_1 1
#define NUMLIGHTS_2 2
#define G_MTX_NOPUSH 0x00
#define G_MTX_PUSH 0x01
#define G_MTX_MUL 0x00
#define G_MTX_LOAD 0x02
#define G_MTX_MODELVIEW 0x00
#define G_MTX_PROJECTION 0x04
#define G_FOG 0x00010000
#define G_LIGHTING 0x00020000
#define G_TEXTURE_GEN 0x00040000
#define G_MDSFT_CYCLETYPE 20
#define G_MDSFT_ZSRCSEL 2
#define G_MDSFT_RENDERMODE 3
#define G_CYC_1CYCLE (0 << G_MDSFT_CYCLETYPE)
#define G_CYC_2CYCLE (1 << G_MDSFT_CYCLETYPE)
#define G_ZS_PIXEL (0 << G_MDSFT_ZSRCSEL)
#define G_ZS_PRIM (1 << G_MDSFT_ZSRCSEL)
#define G_ON 1
#define G_SC_NON_INTERLACE 0
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

/* ---- types ---- */
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

typedef union Vtx {
    Vtx_t v;
    long long int force_structure_alignment;
} Vtx;
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

/* per-view graphics buffer (*frame_dynamic) */
typedef struct {
    Vp vp[4];       /* 0x000 */
    Vtx vtx[4];     /* 0x040 */
    Mtx proj[4];    /* 0x080 */
    Mtx look[4];    /* 0x180 */
    LookAt la[32];  /* 0x280 */
    Mtx ident;      /* 0x680 */
} GfxBuf;

typedef struct View {
    f32 rot[3][3];
    f32 pos[3];
    u8 pad30[0x98 - 0x30];
} View;

/* LOD level of a model; the set's levels start at +20 (count is the s16 at +22) */
typedef struct {
    s16 unk0;
    s16 count;
    u16 tex;
    u16 flags;
    f32 dist;
    WADP(Gfx) dl;
} LodLevel;
typedef struct {
    u8 pad0[20];
    LodLevel lv[4];
    u8 pad54[4];
} LodSet; /* 88 bytes */

/* scene node ObjInstanceList[] (0x44 bytes) */
typedef struct Node {
    u32 flags;
    u32 lights;
    View *obj;
    f32 scale;
    f32 w;
    u16 model;
    s16 child;
    s16 next;
    s16 mode;
    Lights2 *light;
    Texture *tex[4];
    TexRect *rect;
    TexRect *clip;
    Palette *pal;
    u8 prim[4];
    u8 env[4];
} Node;

typedef struct {
    LodSet *sets;
    s32 count;
} LodTable;
typedef struct {
    Texture *tex;
    s32 count;
} TexTable;
typedef struct {
    Palette *pal;
    s32 count;
} PalTable;

VIEW(GfxBuf *, frame_dynamic);
#define frame_dynamic frame_dynamic_view
VIEW(Mtx *, frame_matrix_cursor);
#define frame_matrix_cursor frame_matrix_cursor_view
VIEW(Node[600], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view
VIEW(View[], gCamera);
#define gCamera gCamera_view
VIEW(LodTable[], gRomObjDefPtrs);
#define gRomObjDefPtrs gRomObjDefPtrs_view
VIEW(TexTable[], gTexDefPtrs);
#define gTexDefPtrs gTexDefPtrs_view
VIEW(PalTable[], gPalDefPtrs);
#define gPalDefPtrs gPalDefPtrs_view

VIEW(f32[4][4], zoid_view_matrix);
#define zoid_view_matrix zoid_view_matrix_view
VIEW(f32[4][4], zoid_upright_matrix);
#define zoid_upright_matrix zoid_upright_matrix_view
VIEW(f32[4][4], zoid_scaled_matrix);
#define zoid_scaled_matrix zoid_scaled_matrix_view
VIEW(f32[3], zoid_node_position);
#define zoid_node_position zoid_node_position_view

VIEW(Texture *, zoid_current_texture);
#define zoid_current_texture zoid_current_texture_view
VIEW(Vtx[], cull_vertices);
#define cull_vertices cull_vertices_view
VIEW(Vtx[], cull_box_vertices_a);
#define cull_box_vertices_a cull_box_vertices_a_view
VIEW(Vtx[], cull_box_vertices_b);
#define cull_box_vertices_b cull_box_vertices_b_view

float sqrtf(float);
#pragma intrinsic(sqrtf)
float fabsf(float);
#pragma intrinsic(fabsf)
VIEW(Gfx *, zoid_branch_slot);
#define zoid_branch_slot zoid_branch_slot_view
s32 zoid_draw_object(Gfx **gp, Node *parent, s32 count, s32 flag, Node *node, s32 view, Palette *pal);
s32 Traverse(Gfx **gp, Node *parent, Node *node, s32 view, s32 count, s32 more_in, s32 flags6, Palette *pal);

void zoid_load_texture(Gfx **gp, Texture *tex, TexRect *rect, TexRect *clip, Palette *palettes);
void zoid_load_palette(Gfx **gfxp, Palette *tex);
#define gDPSetTileSizeS(pkt, t, uls, ult, lrs, lrt) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (G_SETTILESIZE << 24) | (((uls) & 0xFFF) << 12) | ((ult) & 0xFFF); \
    _g->words.w1 = (((t) & 7) << 24) | (((lrs) & 0xFFF) << 12) | ((lrt) & 0xFFF); \
}

void zoid_load_palette(Gfx **gfxp, Palette *tex) {
    Gfx *gfx = *gfxp;

    if (zoid_current_tlut != tex->table || zoid_branch_slot != 0) {
        zoid_current_tlut = tex->table;
        gDPLoadTLUT(gfx++, tex->last - tex->first + 1, 0x100 + tex->first, tex->table);
        *gfxp = gfx;
    }
}

#define TILE(fmt, sz) gDPLoadTextureTile(gfx++, tex->image, fmt, G_IM_SIZ_##sz, tex->width, tex->height, rect->x0 >> 5, rect->y0 >> 5, rect->x1 >> 5, rect->y1 >> 5, 0, cms, cmt, masks, maskt, G_TX_NOLOD, G_TX_NOLOD)
#define TILE4(fmt) gDPLoadTextureTile_4b(gfx++, tex->image, fmt, tex->width, tex->height, rect->x0 >> 5, rect->y0 >> 5, rect->x1 >> 5, rect->y1 >> 5, 0, cms, cmt, masks, maskt, G_TX_NOLOD, G_TX_NOLOD)
#define BLOCK(fmt, sz) gDPLoadTextureBlock(gfx++, tex->image, fmt, G_IM_SIZ_##sz, tex->width, tex->height, 0, cms, cmt, masks, maskt, G_TX_NOLOD, G_TX_NOLOD)
#define BLOCK4(fmt) gDPLoadTextureBlock_4b(gfx++, tex->image, fmt, tex->width, tex->height, 0, cms, cmt, masks, maskt, G_TX_NOLOD, G_TX_NOLOD)

RDL_STORAGE void zoid_load_texture(Gfx **gp, Texture *tex, TexRect *rect, TexRect *clip, Palette *palettes)
{
    u16 masks, maskt;
    u16 cms, cmt;
    Gfx *gfx = *gp;

    if (tex == NULL) {
        return;
    }
    if (!(tex->flags & TEX_TEXTURE)) {
        gSPDisplayList(gfx++, tex->image);
    } else {
        if (rect != NULL) {
            masks = blit_mask_bits(rect->x0 - rect->x1) - 5;
            maskt = blit_mask_bits(rect->y0 - rect->y1) - 5;
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
        if (tex->fmt == G_IM_FMT_RGBA) {
            if (zoid_tlut_mode != 0) {
                gDPSetTextureLUT(gfx++, G_TT_NONE);
                zoid_tlut_mode = 0;
            }
            if (tex->siz == G_IM_SIZ_16b) {
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
        } else if (tex->fmt == G_IM_FMT_CI) {
            if (zoid_tlut_mode != 1) {
                gDPSetTextureLUT(gfx++, G_TT_RGBA16);
                zoid_tlut_mode = 1;
            }
            if (tex->siz == G_IM_SIZ_8b) {
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
        } else if (tex->fmt == G_IM_FMT_I) {
            if (zoid_tlut_mode != 0) {
                gDPSetTextureLUT(gfx++, G_TT_NONE);
                zoid_tlut_mode = 0;
            }
            if (tex->siz == G_IM_SIZ_8b) {
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
        } else if (tex->fmt == G_IM_FMT_IA) {
            if (zoid_tlut_mode != 0) {
                gDPSetTextureLUT(gfx++, G_TT_NONE);
                zoid_tlut_mode = 0;
            }
            if (tex->siz == G_IM_SIZ_16b) {
                if (rect != NULL) {
                    TILE(G_IM_FMT_IA, 16b);
                } else {
                    BLOCK(G_IM_FMT_IA, 16b);
                }
            } else if (tex->siz == G_IM_SIZ_8b) {
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
        gDPSetTileSizeS(gfx++, G_TX_RENDERTILE, clip->x0 >> 3, clip->y0 >> 3, clip->x1 >> 3, clip->y1 >> 3);
    } else if (rect != NULL) {
        gfx--;
        gDPSetTileSizeS(gfx++, G_TX_RENDERTILE, rect->x0 >> 3, rect->y0 >> 3, rect->x1 >> 3, rect->y1 >> 3);
    }
    if (tex->palette >= 0 && !(tex->flags & TEX_NOPALETTE) && palettes != NULL) {
        zoid_load_palette(&gfx, &palettes[tex->palette]);
    }
    *gp = gfx;
}

VIEW(f32[4], asin_offsets);
#define asin_offsets asin_offsets_view
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
/* a model's last level's distance, as far as the draw distance reaches; not for a node that fades out by then (flag
 * 0x200000, the car's sheen: alpha 0 past 80 ft), which would spend a lookat and its list on nothing */
#define PC_LOD_REACH(d, flags) (((flags) & 0x200000) ? (d) : PC_DRAW_REACH(d))
#define PC_LOD_FORCED(l) (pc_detail() == PC_DETAIL_HIGH ? 0 : (l))   /* CreateCar's tires at level 1 in split screen */
#define PC_VIEW_BEGIN pc_scene_view();

#define PC_SCENERY_CHECK(view, obj, count) if (pc_scenery_unplaced(view, obj, count)) { PC_SCENE(VIEW_HIDDEN) goto next; }
s32 zoid_draw_object(Gfx **gp, Node *parent, s32 count, s32 flag, Node *node, s32 view, Palette *pal)
{
    Gfx *gfx = *gp;
    LodSet *set;
    s32 lod;
    TexRect rect;
    Gfx *dl;
    View *v;
    f32 d[3];
    s32 drawn = 0;
    f32 dir[3];
    Palette *pals;
    u8 alpha;
    f32 x, y, z;
    f32 yaw, pitch;

    set = &gRomObjDefPtrs[node->model >> 10].sets[node->model & 0x3FF];
    lod = set->lv[0].count - 1;
    if (node->flags & 4) {
        lod = node->flags & 3;
        if (lod == 0) {
            node_eye_distance = 0.0f;
        } else {
            node_eye_distance = set->lv[lod - 1].dist;
        }
    } else if ((set->lv[lod].dist != 0.0f && !(node->flags & (0x100 << view)) && !flag) || (node->flags & 8)) {
        if (count < 2) {
            if (count > 0) {
                d[0] = parent->obj->pos[0] + node->obj->pos[0];
                d[1] = parent->obj->pos[1] + node->obj->pos[1];
                d[2] = parent->obj->pos[2] + node->obj->pos[2];
                v = &gCamera[view];
                d[0] = d[0] - v->pos[0];
                d[1] = d[1] - v->pos[1];
                d[2] = d[2] - v->pos[2];
            } else {
                v = &gCamera[view];
                d[0] = node->obj->pos[0] - v->pos[0];
                d[1] = node->obj->pos[1] - v->pos[1];
                d[2] = node->obj->pos[2] - v->pos[2];
            }
        }
        x = d[0];
        y = d[1];
        z = d[2];
        node_eye_distance = sqrtf(x * x + y * y + z * z);
    }
    if (!(node->flags & (0x100 << view))) {
        if (set->lv[lod].dist == 0.0f || !(PC_LOD_REACH(set->lv[lod].dist, node->flags) * node->scale < node_eye_distance)) {
            if (node->flags & 4) {
                lod = PC_LOD_FORCED(node->flags & 3);
                if (lod >= set->lv[0].count) {
                    lod = set->lv[0].count - 1;
                }
            } else if (lod > 0) {
                while (lod != 0 && PC_LOD_DIST(node_eye_distance) < set->lv[lod - 1].dist * node->scale) {
                    lod--;
                }
            }
            dl = set->lv[lod].dl;
            if (dl != NULL) {
                if (node->flags & 0x80000) {
                    v = &gCamera[view];
                    lookat_reflect_fill(&frame_dynamic->la[frame_lookat_count], -v->pos[0], v->pos[1], v->pos[2],
                                    -zoid_node_position[0], zoid_node_position[1], zoid_node_position[2], 0.0f, 1.0f, 0.0f);
                    gSPLookAt(gfx++, &frame_dynamic->la[frame_lookat_count]);
                    gSPSetGeometryMode(gfx++, G_TEXTURE_GEN);
                    frame_lookat_count++;
                    gSPClearGeometryMode(gfx++, G_FOG);
                    gDPPipeSync(gfx++);
                    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
                    if (node->tex[lod] != NULL) {
                        gSPTexture(gfx++, node->tex[lod]->width << 7, node->tex[lod]->height << 6, 0,
                                   G_TX_RENDERTILE, G_ON);
                    }
                    dir[0] = zoid_node_position[0] - v->pos[0];
                    dir[1] = zoid_node_position[1] - v->pos[1];
                    dir[2] = zoid_node_position[2] - v->pos[2];
                    NormalVector(dir);
                    yaw = fatan2(dir[0], dir[2]);
                    rect.x0 = -yaw * 1303.7972f + 6144.0f;
                    pitch = farcsine(dir[1], 0);
                    rect.y0 = -pitch * 1303.7972f + 2048.0f;
                    rect.x1 = rect.x0 + 4064;
                    rect.y1 = rect.y0 + 2016;
                    node->clip = &rect;
                }
                if (set->lv[lod].flags & 0x10) {
                    gSPSetGeometryMode(gfx++, G_LIGHTING);
                }
                if (node->pal != NULL) {
                    zoid_load_palette(&gfx, node->pal);
                }
                if (set->lv[lod].flags & 1) {
                    pals = (node->pal != NULL) ? NULL : gPalDefPtrs[node->model >> 10].pal;
                    if (node->tex[lod] != NULL) {
                        zoid_load_texture(&gfx, node->tex[lod], node->rect, node->clip, pals);
                    } else if (set->lv[lod].flags & 0x8000) {
                        zoid_load_texture(&gfx, (set->lv[lod].tex & 0x3FF) + gTexDefPtrs[set->lv[lod].tex >> 10].tex, node->rect, node->clip, pals);
                    }
                } else if (node->clip != NULL) {
                    gDPSetTileSize(gfx++, G_TX_RENDERTILE, node->clip->x0 >> 3, node->clip->y0 >> 3,
                                   node->clip->x1 >> 3, node->clip->y1 >> 3);
                } else if (node->rect != NULL) {
                    gDPSetTileSize(gfx++, G_TX_RENDERTILE, node->rect->x0 >> 3, node->rect->y0 >> 3,
                                   node->rect->x1 >> 3, node->rect->y1 >> 3);
                }
                if ((set->lv[lod].flags & 4) || (node->flags & 0x2000)) {
                    alpha = node->prim[3];
                    if ((node->flags & 0x200000) && node_eye_distance > 24.0f) {
                        if (node_eye_distance > 80.0f) {
                            alpha = 0;
                        } else {
                            alpha = alpha * (80.0f - node_eye_distance) / 56.0f;
                        }
                    }
                    gDPSetPrimColor(gfx++, 0, 0, node->prim[0], node->prim[1], node->prim[2], alpha);
                }
                if (node->flags & 0x4000) {
                    gDPSetEnvColor(gfx++, node->env[0], node->env[1], node->env[2], node->env[3]);
                }
                gSPDisplayList(gfx++, dl);
                if (zoid_tlut_mode == 0) {
                    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
                    zoid_tlut_mode = 1;
                }
                if (set->lv[lod].flags & 2) {
                    zoid_current_tlut = 0;
                    if (pal != NULL) {
                        zoid_load_palette(&gfx, pal);
                    }
                }
                drawn = 1; PC_SCENE(DRAWN) PC_SCENE_LEVEL(lod)
                if (node->flags & 0x80000) {
                    gDPPipeSync(gfx++);
                    gDPSetCycleType(gfx++, G_CYC_2CYCLE);
                    gSPSetGeometryMode(gfx++, G_FOG);
                    gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
                    gSPClearGeometryMode(gfx++, G_TEXTURE_GEN);
                    node->clip = NULL;
                }
                if (set->lv[lod].flags & 0x10) {
                    gSPClearGeometryMode(gfx++, G_LIGHTING);
                }
            }
        } PC_LOD_CUT
    }
    *gp = gfx;
    return drawn;
}

s32 Traverse(Gfx **gp, Node *parent, Node *node, s32 view, s32 count, s32 more_in, s32 flags6,
                         Palette *pal)
{
    Gfx *gfx = *gp;
    s32 more;
    s32 pushed = 0, total = 0, saved = 0;
    s32 i, j;
    s32 n;
    Vtx *vtx;
    View *v;
    f32 x, z, r;
    s32 pad[4];

    for (;;) {
        if (node->flags & 0x80000000) {
            PC_SCENE(HIDDEN) goto next;
        }
        if (node->next >= 0 || more_in) {
            more = 1;
        } else {
            more = 0;
        }
        if (zoid_late_pass_a == 0 && (node->flags & 0x40000)) {
            if (zoid_late_node_a < 0) {
                zoid_late_node_a = node - ObjInstanceList;
            }
            goto next;
        }
        if (zoid_late_pass_b == 0 && (node->flags & 0x800000)) {
            if (zoid_late_node_b < 0) {
                zoid_late_node_b = node - ObjInstanceList;
            }
            goto next;
        }
        if (!zoid_node_visible(node, view)) {
            PC_SCENE(VIEW_HIDDEN) goto next;
        }
        if (node->child >= 0 || !(node->flags & (0x100 << view))) {
            if (node->flags & 0x1000) {
                gSPVertex(gfx++, &frame_dynamic->vtx[view], 1, 8);
            }
            if (node->flags & 0x80) {
                more = 0;
            } else {
                zoid_node_position[0] = node->obj->pos[0];
                zoid_node_position[1] = node->obj->pos[1];
                zoid_node_position[2] = node->obj->pos[2];
                if (node->flags & 0x10000) {
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
                        if (!build_node_matrix_rows4(view, zoid_scaled_matrix, node->obj->pos, frame_matrix_cursor, node->w, count)) {
                            PC_SCENE(RANGE) goto next;
                        }
                    } else if (!build_node_matrix_rows4(view, zoid_view_matrix, node->obj->pos, frame_matrix_cursor, node->w, count)) {
                        PC_SCENE(RANGE) goto next;
                    }
                } else if (node->flags & 0x8000) {
                    if (!build_node_matrix_rows4(view, zoid_upright_matrix, node->obj->pos, frame_matrix_cursor, node->w, count)) {
                        PC_SCENE(RANGE) goto next;
                    }
                } else if (node->flags & 0x400000) {
                    PC_SCENERY_CHECK(view, node->obj, count) build_far_node_matrix(view, node->obj, frame_matrix_cursor, node->w, count);
                } else if (!build_node_matrix(view, node->obj, frame_matrix_cursor, node->w, count)) {
                    PC_SCENE(RANGE) goto next;
                }
                gSPMatrix(gfx++, frame_matrix_cursor, more);
                frame_matrix_cursor++;
            }
            if (node->mode >= 0) {
                switch (node->mode) {
                    case 0:
                        gSPSetLights0(gfx++, (*node->light));
                        break;
                    case 1:
                        gSPSetLights1(gfx++, (*node->light));
                        break;
                    case 2:
                        gSPSetLights2(gfx++, (*node->light));
                        break;
                }
            }
            if ((node->flags & 0x10) || (node->flags & 0x20)) {
                zoid_branch_slot = gfx + 2;
                gSPDisplayList(gfx++, zoid_branch_slot);
                gSPBranchList(gfx++, NULL);
                pushed = 1;
                saved = total;
                total = 0;
                if (node->flags & 0x10) {
                    gSPVertex(gfx++, cull_box_vertices_a, 8, 0);
                } else {
                    gSPVertex(gfx++, cull_box_vertices_b, 8, 0);
                }
                gSPCullDisplayList(gfx++, 0, 7);
            } else if (node->lights & 7) {
                n = node->lights & 7;
                vtx = &cull_vertices[(node->lights & 0x1FF8) >> 3];
                if (!(node->flags & 0x400000) && gViewableDistance < 2000.0f) {
                    v = &gCamera[view];
                    x = (vtx[0].v.ob[0] < vtx[7].v.ob[0]) ? vtx[7].v.ob[0] : vtx[0].v.ob[0];
                    z = (vtx[0].v.ob[2] < vtx[7].v.ob[2]) ? vtx[7].v.ob[2] : vtx[0].v.ob[2];
                    r = sqrtf(x * x + z * z) * 0.0625f;
                    x = node->obj->pos[0] - v->pos[0];
                    z = node->obj->pos[2] - v->pos[2];
                    if (PC_DRAW_REACH(gViewableDistance) < sqrtf(x * x + z * z) - r) {
                        PC_SCENE(RADIUS) goto children;
                    }
                }
                zoid_branch_slot = gfx + 2;
                gSPDisplayList(gfx++, zoid_branch_slot);
                gSPBranchList(gfx++, NULL);
                zoid_current_tlut = 0;
                pushed = 1;
                zoid_current_texture = NULL;
                saved = total;
                gSPVertex(gfx++, vtx, n + 1, 0);
                gSPCullDisplayList(gfx++, 0, n);
                total = 0;
            }
            total += zoid_draw_object(&gfx, parent, count, flags6, node, view, (node->pal != NULL) ? node->pal : pal);
        } else {
            more = 0;
        }
    children:
        if (node->child >= 0) {
            total += Traverse(&gfx, node, &ObjInstanceList[node->child], view,
                                          (more == 1) ? count + 1 : count, node->next >= 0,
                                          (node->flags & 8) | flags6, (node->pal != NULL) ? node->pal : pal);
        }
        if (pushed) {
            pushed = 0;
            if (total == 0) {
                gfx = zoid_branch_slot - 2;
            } else {
                gSPEndDisplayList(gfx++);
                zoid_branch_slot[-1].words.w1 = (uintptr_t)gfx;
            }
            zoid_branch_slot = NULL;
            total += saved;
            saved = 0;
        }
        if (more == 1) {
            if (total == 0) {
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
        if (zoid_late_pass_a != 0 && !(node->flags & 0x40000)) {
            goto next;
        }
        if (zoid_late_pass_b != 0 && !(node->flags & 0x800000)) {
            goto next;
        }
    }
    *gp = gfx;
    return total;
}

typedef struct Camera {
    Vp *vp;          /* 0x00 */
    u16 *scissor;    /* 0x04 */
    s32 ortho;       /* 0x08 */
    u8 pad0C[4];
    f32 fov;         /* 0x10 */
    u8 pad14[0x10];
    f32 w;           /* 0x24 */
    f32 h;           /* 0x28 */
    f32 x;           /* 0x2C */
    f32 y;           /* 0x30 */
    f32 aspect;      /* 0x34 */
    f32 near;        /* 0x38 */
    f32 far;         /* 0x3C */
    u16 fogmin;      /* 0x40 */
    u16 fogmax;      /* 0x42 */
    u8 fog[4];       /* 0x44 */
} Camera; /* 0x48 */

typedef struct {
    f32 pos[3];
    s16 tc[2];
    u8 cn[4];
} PolyVtx; /* 20 bytes */

typedef struct Poly {
    s16 count;       /* 0x00 */
    u16 flags;       /* 0x02 */
    u16 tex;         /* 0x04 */
    s16 z;           /* 0x06 */
    union {
        PolyVtx v[4];
        struct {
            f32 x0, y0;      /* 0x08 */
            u8 pad10[8];
            u8 col[4];       /* 0x18 */
            f32 x1, y1;      /* 0x1C */
        } rect;
    } u;
} Poly; /* 0x58 */

VIEW(Gfx *, frame_dl);
#define frame_dl frame_dl_view
VIEW(Camera[], ZoidWinInfo);
#define ZoidWinInfo ZoidWinInfo_view

VIEW(MtxF, zoid_lookat_matrix);
#define zoid_lookat_matrix zoid_lookat_matrix_view

VIEW(s16, ObjHead);
#define ObjHead ObjHead_view

VIEW(Poly[], PolyInstanceList);
#define PolyInstanceList PolyInstanceList_view
VIEW(Vtx *, zoid_poly_vertices);
#define zoid_poly_vertices zoid_poly_vertices_view

void *memcpy(void *, const void *, unsigned int);

void zoid_draw_window(s32 view, View *eye)
{
    Camera *cam = &ZoidWinInfo[view];
    Vp *vp = &frame_dynamic->vp[view];
    Poly *poly;
    u16 ulx, uly, lrx, lry;
    s32 mode;
    s32 fog;
    s32 depth;
    s32 i;

    PC_VIEW_BEGIN WorldCullObjects(view, eye->pos, 0);
    memcpy(vp, cam->vp, sizeof(Vp));
    vp->vp.vscale[0] = (s16)cam->w * 2;
    vp->vp.vscale[1] = (s16)cam->h * 2;
    vp->vp.vtrans[0] = (s16)cam->x * 4;
    vp->vp.vtrans[1] = (s16)cam->y * 4;
    ulx = cam->scissor[0];
    uly = cam->scissor[1];
    lrx = cam->scissor[2];
    lry = cam->scissor[3];
    gSPViewport(frame_dl++, &frame_dynamic->vp[view]);
    gDPPipeSync(frame_dl++);
    gDPSetScissor(frame_dl++, G_SC_NON_INTERLACE, ulx, uly, lrx, lry);
    gDPSetFogColor(frame_dl++, cam->fog[0], cam->fog[1], cam->fog[2], cam->fog[3]);
    gSPFogPosition(frame_dl++, cam->fogmin, cam->fogmax);
    gDPSetCycleType(frame_dl++, G_CYC_2CYCLE);
    gSPSetGeometryMode(frame_dl++, G_FOG);
    depth = -1;
    gDPSetDepthSource(frame_dl++, G_ZS_PIXEL);
    if (zoid_tlut_mode != 1) {
        gDPSetTextureLUT(frame_dl++, G_TT_RGBA16);
        zoid_tlut_mode = 1;
    }
    if (cam->ortho) {
        Mtx *m = &frame_dynamic->proj[view];

        guOrtho(m, -cam->w * 0.5f, cam->w * 0.5f, -cam->h * 0.5f, cam->h * 0.5f,
                cam->near * 16.0f, cam->far * 16.0f, 1.0f);
    } else {
        u16 perspNorm;

        guPerspective(&frame_dynamic->proj[view], &perspNorm, cam->fov * 57.295776f, cam->aspect, cam->near * 16.0f,
                      cam->far * 16.0f, 1.0f);
        gSPPerspNormalize(frame_dl++, perspNorm);
    }
    gSPMatrix(frame_dl++, &frame_dynamic->proj[view], G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    if (gUpsideDownTrack) {
        f32 uv[3][3];

        CopyUV(&gCamera[view], uv);
        RollUV(3.1415927f, uv);
        if (gMirrorWaves) {
            uv[2][0] *= -1.0f;
            uv[1][0] *= -1.0f;
        }
        guLookAtF(zoid_lookat_matrix, 0.0f, 0.0f, 0.0f, -uv[2][0], uv[2][1], uv[2][2], -uv[1][0], uv[1][1], uv[1][2]);
    } else if (gMirrorWaves) {
        guLookAtF(zoid_lookat_matrix, 0.0f, 0.0f, 0.0f, -(-gCamera[view].rot[2][0]), gCamera[view].rot[2][1],
                  gCamera[view].rot[2][2], -(-gCamera[view].rot[1][0]), gCamera[view].rot[1][1],
                  gCamera[view].rot[1][2]);
    } else {
        guLookAtF(zoid_lookat_matrix, 0.0f, 0.0f, 0.0f, -gCamera[view].rot[2][0], gCamera[view].rot[2][1],
                  gCamera[view].rot[2][2], -gCamera[view].rot[1][0], gCamera[view].rot[1][1],
                  gCamera[view].rot[1][2]);
    }
    guLookAtF(zoid_view_matrix, gCamera[view].rot[2][0], gCamera[view].rot[2][1], gCamera[view].rot[2][2], 0.0f,
              0.0f, 0.0f, gCamera[view].rot[1][0], gCamera[view].rot[1][1], gCamera[view].rot[1][2]);
    if (gGlobalCameraFacingScale != 1) {
        s32 j;

        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) zoid_view_matrix[i][j] *= gGlobalCameraFacingScale;
        }
    }
    zoid_build_upright_matrix(zoid_upright_matrix[0]);
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

        if (poly->flags & 0x8000) {
            continue;
        }
        if (!(poly->flags & (1 << view))) {
            continue;
        }
        if (poly->count == 0) {
            continue;
        }
        gDPPipeSync(frame_dl++);
        if (poly->flags & 0x2000) {
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
        if (poly->flags & 0x4000) {
            if (mode != 0) {
                mode = 0;
                gDPSetRenderMode(frame_dl++, 0x00504240, 0);
                gDPSetCombine(frame_dl++, 0xFFFFFF, 0xFFFDF6FB);
            }
            gDPSetPrimColor(frame_dl++, 0, 0, poly->u.rect.col[0], poly->u.rect.col[1], poly->u.rect.col[2],
                            poly->u.rect.col[3]);
            gDPFillRectangle(frame_dl++, (s16)poly->u.rect.x0, (s16)poly->u.rect.y0, (s16)poly->u.rect.x1,
                             (s16)poly->u.rect.y1);
            gDPPipeSync(frame_dl++);
            continue;
        }
        for (j = 0; j < poly->count; j++) {
            f32 p[3];

            if (!(poly->flags & 0x10)) {
                p[0] = poly->u.v[j].pos[0] - gCamera[view].pos[0] * 16.0f;
                p[1] = poly->u.v[j].pos[1] - gCamera[view].pos[1] * 16.0f;
                p[2] = poly->u.v[j].pos[2] - gCamera[view].pos[2] * 16.0f;
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
        if ((poly->flags & 0x100) && fog) {
            fog = 0;
            mode = -1;
            gSPClearGeometryMode(frame_dl++, G_FOG);
            gDPPipeSync(frame_dl++);
            gDPSetCycleType(frame_dl++, G_CYC_1CYCLE);
        } else if (!(poly->flags & 0x100) && !fog) {
            fog = 1;
            mode = -1;
            gSPSetGeometryMode(frame_dl++, G_FOG);
            gDPPipeSync(frame_dl++);
            gDPSetCycleType(frame_dl++, G_CYC_2CYCLE);
        }
        if (fog) {
            gDPSetPrimColor(frame_dl++, 0, 0, poly->u.rect.col[0], poly->u.rect.col[1], poly->u.rect.col[2],
                            poly->u.rect.col[3]);
        }
        if (poly->flags & 0x400) {
            Texture *tex;

            tex = &gTexDefPtrs[poly->tex >> 10].tex[poly->tex & 0x3FF];
            if (tex != zoid_current_texture) {
                zoid_load_texture(&frame_dl, &gTexDefPtrs[poly->tex >> 10].tex[poly->tex & 0x3FF], NULL, NULL,
                                    gPalDefPtrs[poly->tex >> 10].pal);
                zoid_current_texture = tex;
            }
            if (poly->flags & 0x200) {
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
        gSPVertex(frame_dl++, zoid_poly_vertices, poly->count, 0);
        zoid_poly_vertices += poly->count;
        if (poly->flags & 0x1000) {

            gSP2Triangles(frame_dl++, 0, 2, 1, 0, 2, 0, 1, 0);
            if (poly->count >= 4) {
                gSP2Triangles(frame_dl++, 2, 0, 3, 0, 0, 2, 3, 0);
            }
        } else if (gMirrorWaves) {
            if (poly->count == 3) {
                gSP1Triangle(frame_dl++, 1, 0, 2, 0);
            } else {
                gSP2Triangles(frame_dl++, 1, 0, 2, 0, 0, 3, 2, 0);
            }
        } else {
            if (poly->count == 3) {
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
        Node *n = &ObjInstanceList[zoid_late_node_a];

        zoid_late_pass_a = 1;
        zoid_late_pass_b = 0;
        Traverse(&frame_dl, NULL, n, view, 0, 1, 0, NULL);
        zoid_tlut_mode = -1;
        zoid_current_texture = NULL;
    }
    if (zoid_late_node_b >= 0) {
        Node *n = &ObjInstanceList[zoid_late_node_b];

        zoid_late_pass_a = 0;
        zoid_late_pass_b = 1;
        Traverse(&frame_dl, NULL, n, view, 0, 1, 0, NULL);
        zoid_tlut_mode = -1;
        zoid_current_texture = NULL;
    }
}

/* ---- ZOID_EndFrame (0x800A04C4): per-frame scene pass + 2D sprite list ---- */
#define G_MDSFT_TEXTFILT 12
#define G_TF_POINT (0 << G_MDSFT_TEXTFILT)
#define gDPSetTextureFilter(pkt, type) gSPSetOtherMode(pkt, G_SETOTHERMODE_H, G_MDSFT_TEXTFILT, 2, type)
typedef struct Sprite {
    Palette *pal;   /* 0x00 palette (textured) or u8 colour (fill) */
    Texture *tex;   /* 0x04 texture, or a callback when flags & 0x80 */
    u16 model;      /* 0x08 >>10 palette table */
    s16 x, y;       /* 0x0A */
    u16 mode;       /* 0x0E */
    s16 w, h;       /* 0x10 */
    u8 color;       /* 0x14 */
    u8 flags;       /* 0x15 */
    s8 hidden;      /* 0x16 */
    u8 pad17;
    s16 t0, s0, t1, s1; /* 0x18 */
} Sprite;

VIEW(Lights2 *, gLights);
#define gLights gLights_view
VIEW(s16, gNumLights);
#define gNumLights gNumLights_view

VIEW(Vtx[][3200], zoid_poly_vertex_buffers);
#define zoid_poly_vertex_buffers zoid_poly_vertex_buffers_view

VIEW(Sprite[200], gBlitInstanceList);
#define gBlitInstanceList gBlitInstanceList_view
VIEW(u8[4], blit_ia_palette);
#define blit_ia_palette blit_ia_palette_view

/* A palette's table is a 32-bit N64 address its loader reads through: N64PTR(T, a) the pointer at one (pc_prelude.h,
 * docs/port/NOFIXED.md 2.3). */
#define N64PTR(T, a) ((T)PC_N64P(a))
void ZOID_EndFrame(s32 arg0)
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
    if (arg0 != 0) {
        return;
    }
    if (screen_height > 240) {
        BlitSetAttribute(0x8000);
    }
    for (i = 0; i < gNumBlitInsts; i++) {
        Sprite *e;
        Texture *tex;
        Palette *pal;
        s32 h;
        s32 sw;
        s32 th;
        s32 lines;
        s32 x;
        s32 y;
        s32 tile;
        s32 t;
        s32 next;
        s32 flip;
        s32 x1;

        e = &gBlitInstanceList[i];
        tile = 0;
        tex = e->tex;
        if (e->hidden) {
            continue;
        }
        if (e->flags & 0x80) {
            if (e->tex != NULL) {
                ((void (*)(Palette *))e->tex)(e->pal);
            }
            continue;
        }
        if (e->mode != 0) {
            BlitZDist(e->mode);
        } else {
            BlitClearAttribute(16);
        }
        if (e->flags & 4) {
            BlitSetAttribute(4);
        } else {
            BlitClearAttribute(4);
        }
        if (e->w == 0 || e->h == 0) {
            continue;
        }
        if (tex == NULL) {
            if (e->color == 255) {
                BlitClearAttribute(32);
            } else {
                BlitXLU(e->color);
            }
            x = e->x;
            y = e->y;
            flip = e->x + e->w;
            lines = e->y + e->h;
            BlitScissor(x, y, flip - 1, lines - 1);
            BlitRect(x, y, flip, lines, (u8 *)e->pal);
            continue;
        }
        x = e->x;
        y = e->y;
        lines = e->h;
        if (screen_height > 240) {
            lines = lines + lines;
        }
        BlitScissor(x, y, x + e->w - 1, y + lines - 1);
        sw = e->s1 - e->s0 + 1;
        th = e->t1 - e->t0 + 1;
        flip = tex->width;
        lines = blit_tile_lines[flip >> (4 - tex->siz)];
        if (tex->fmt == 2 || tex->fmt == 5) {
            lines >>= 1;
        }
        if (lines < 2 || ((15 >> tex->siz) & flip)) {
            tile = 1;
            if (tex->siz == 0) {
                lines = 4096 / tex->width;
            } else if (tex->siz == 1 || tex->siz == 2) {
                lines = 2048 / tex->width;
            } else {
                lines = 1024 / tex->width;
            }
        }
        flip = 0;
        if (e->flags & 8) {
            BlitClearAttribute(8);
        } else {
            flip = 1;
            BlitSetAttribute(8);
        }
        if (e->flags & 1) {
            BlitSetAttribute(1);
        }
        if (e->color == 255) {
            BlitClearAttribute(32);
        } else {
            BlitXLU(e->color);
        }
        if (e->pal != NULL) {
            pal = e->pal;
            BlitColors(N64PTR(u8 *, pal->table), tex->fmt, tex->siz, 0);
        } else if (tex->palette >= 0) {
            pal = &gPalDefPtrs[e->model >> 10].pal[tex->palette];
            BlitColors(N64PTR(u8 *, pal->table), tex->fmt, tex->siz, 0);
        } else if (tex->fmt == 3) {
            blit_ia_palette[3] = e->color;
            BlitColors(blit_ia_palette, tex->fmt, tex->siz, 0);
        } else if (tex->fmt != 0) {
            if (gPalDefPtrs[e->model >> 10].count != 0) {
                pal = gPalDefPtrs[e->model >> 10].pal;
                BlitColors(N64PTR(u8 *, pal->table), tex->fmt, tex->siz, 0);
            }
        }
        if (e->t1 >= tex->height) {
            e->t1 = tex->height - 1;
        }
        if (e->t0 < 0) {
            t = 0;
        } else {
            t = e->t0;
        }
        if (th < lines) {
            lines = th;
        }
        if (flip) {
            while (t < e->t1) {
                if (e->t1 < t + lines) {
                    lines = e->t1 - t + 1;
                }
                blit_load_texture(tex->image, tex->fmt, tex->siz, tex->width, tex->height, 0, tex->height - (t + lines), sw - 1, tex->height - t - 1, 0, tile);
                BlitDraw(x, y, x + sw - 1, y + lines - 1, e->s0, tex->height - t - lines);
                if (screen_height > 240) {
                    y += lines;
                }
                t += lines;
                y += lines;
            }
        } else {
            while (t < e->t1) {
                if (e->t1 < t + lines) {
                    lines = e->t1 - t + 1;
                }
                blit_load_texture(tex->image, tex->fmt, tex->siz, tex->width, tex->height, 0, t, sw - 1, t + lines - 1, 0, tile);
                BlitDraw(x, y, x + sw - 1, y + lines - 1, e->s0, t);
                if (screen_height > 240) {
                    y += lines;
                }
                t += lines;
                y += lines;
            }
        }
        if (e->flags & 1) {
            BlitClearAttribute(1);
        }
    }
}
f32 fasin(f32 input) { return farcsine(input, 0); }
f32 facos(f32 input) { return farcsine(input, 1); }

/*
 * : InterpQuats(frac, q1, q2, qr) = arcade LIB/fmath.c InterpQuats with the `#if 0` spherical branch
 * enabled (N64 variant): frac < .001f -> copy q1, frac > .999f -> copy q2; dp = DotQuat * .999f; qr = +-q2
 * (sign flipped so dp >= 0); dp < .98f -> slerp (acosf is facos, inlined: jal farcsine
 * with the dot product left in $f16 by IPA, hence group member), else the arcade linear blend with wraps.
 * Called only from PlacePathTargetQuat (unmatched); kept, standard ABI (f12, a1, a2, a3).
 * Shaping: `(1 - frac)` with int 1 (a separate 1.0f constant web from `1.0f / sint`); the slerp sum is the
 * arcade order `q1[i] * X1 + qr[i] * X2`; CopyQuat as the arcade comma expression.
 */

f32 sinf(f32);

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
