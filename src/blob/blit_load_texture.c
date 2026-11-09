/* blit_load_texture: load a texture, or a part of it, into the RDP's texture memory for the blitter (the N64 side of
 * MAT3 blit_pc.c, which MAT3's GameCube build has no need of). */
#include "game/game.h"
#define Gfx GfxView   /* this file's own Gfx: include/game's Gfx is another type */

typedef union {
    Gwords words;
    long long int force_structure_alignment;
} Gfx;

/* SDK forms, reference/repos/ultralib/include/PR/gbi.h */
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
/* Game variants of gDPLoadTextureBlock / _4b: identical to the SDK macros except that the render tile's
 * size starts at (uls, ult) instead of (0, 0). */
#define gDPLoadTextureBlockAt(pkt, timg, fmt, siz, width, height, uls, ult, pal, cms, cmt, masks, maskt, shifts, shiftt) \
{ \
    gDPSetTextureImage(pkt, fmt, siz##_LOAD_BLOCK, 1, timg); \
    gDPSetTile(pkt, fmt, siz##_LOAD_BLOCK, 0, 0, G_TX_LOADTILE, 0 , cmt, maskt, shiftt, cms, masks, shifts); \
    gDPLoadSync(pkt); \
    gDPLoadBlock(pkt, G_TX_LOADTILE, 0, 0, (((width)*(height) + siz##_INCR) >> siz##_SHIFT) -1, CALC_DXT(width, siz##_BYTES)); \
    gDPPipeSync(pkt); \
    gDPSetTile(pkt, fmt, siz, (((width) * siz##_LINE_BYTES)+7)>>3, 0, G_TX_RENDERTILE, pal, cmt, maskt, shiftt, cms, masks, shifts); \
    gDPSetTileSize(pkt, G_TX_RENDERTILE, (uls) << G_TEXTURE_IMAGE_FRAC, (ult) << G_TEXTURE_IMAGE_FRAC, \
        ((uls)+(width)-1) << G_TEXTURE_IMAGE_FRAC, ((ult)+(height)-1) << G_TEXTURE_IMAGE_FRAC) \
}
#define gDPLoadTextureBlockAt_4b(pkt, timg, fmt, width, height, uls, ult, pal, cms, cmt, masks, maskt, shifts, shiftt) \
{ \
    gDPSetTextureImage(pkt, fmt, G_IM_SIZ_16b, 1, timg); \
    gDPSetTile(pkt, fmt, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0 , cmt, maskt, shiftt, cms, masks, shifts); \
    gDPLoadSync(pkt); \
    gDPLoadBlock(pkt, G_TX_LOADTILE, 0, 0, (((width)*(height)+3)>>2)-1, CALC_DXT_4b(width)); \
    gDPPipeSync(pkt); \
    gDPSetTile(pkt, fmt, G_IM_SIZ_4b, ((((width)>>1)+7)>>3), 0, G_TX_RENDERTILE, pal, cmt, maskt, shiftt, cms, masks, shifts); \
    gDPSetTileSize(pkt, G_TX_RENDERTILE, (uls) << G_TEXTURE_IMAGE_FRAC, (ult) << G_TEXTURE_IMAGE_FRAC, \
        ((uls)+(width)-1) << G_TEXTURE_IMAGE_FRAC, ((ult)+(height)-1) << G_TEXTURE_IMAGE_FRAC) \
}

VIEW(Gfx *, blit_dl);
#define blit_dl blit_dl_view

/* The four ways of loading: a rectangle of the texture (TILE) or whole rows from row ult down (BLOCK), 4-bit texels
 * apart (TILE4, BLOCK4); clamped, masked to the size's power of two. */
#define TILE(fmt, sz) gDPLoadTextureTile(blit_dl++, img, fmt, G_IM_SIZ_##sz, width, height, uls, ult, lrs, lrt, pal, G_TX_CLAMP, G_TX_CLAMP, masks, maskt, G_TX_NOLOD, G_TX_NOLOD)
#define TILE4(fmt) gDPLoadTextureTile_4b(blit_dl++, img, fmt, width, height, uls, ult, lrs, lrt, pal, G_TX_CLAMP, G_TX_CLAMP, masks, maskt, G_TX_NOLOD, G_TX_NOLOD)
#define BLOCK(fmt, sz) gDPLoadTextureBlockAt(blit_dl++, img, fmt, G_IM_SIZ_##sz, width, height, uls, ult, pal, G_TX_CLAMP, G_TX_CLAMP, masks, maskt, G_TX_NOLOD, G_TX_NOLOD)
#define BLOCK4(fmt) gDPLoadTextureBlockAt_4b(blit_dl++, img, fmt, width, height, uls, ult, pal, G_TX_CLAMP, G_TX_CLAMP, masks, maskt, G_TX_NOLOD, G_TX_NOLOD)

/* Loads the texture img for the next BlitDraw and chooses the blitter's mode for it:
 *   type        its format: 0 RGBA, 2 colour-indexed, 3 IA, 4 I (G_IM_FMT_*), 5 colour-indexed 8-bit with a 4-bit
 *               second tile (BLIT_MODE_TWO_TILE)
 *   siz         its texel size (G_IM_SIZ_*: 0 4-bit, 1 8-bit, 2 16-bit, 3 32-bit)
 *   width, height  its size in texels
 *   uls, ult, lrs, lrt  the part wanted: columns uls..lrs, rows ult..lrt
 *   pal         the palette bank of a 4-bit colour-indexed texture
 *   tile        0: load whole rows ult..lrt as one block (the image pointer advanced to row ult); else the rectangle
 *               alone, which the RDP's texture memory may need when the rows don't fit
 * RGBA and colour-indexed textures use copy mode unless translucent, depth-tested or tall; I and IA ones the
 * intensity modes (with the alpha threshold when BLIT_ATTR_ALPHA_COMPARE); IA is always translucent. Nothing is loaded
 * when the same image and part are already there (blit_cache_image, blit_cache_tile: the part packed in 64 bits). */
void blit_load_texture(void *img, u16 type, u16 siz, u16 width, u16 height, u16 uls, u16 ult, u16 lrs, u16 lrt,
                   u16 pal, s32 tile)
{
    s32 mode;
    u16 masks;
    u16 maskt;

    if (tile == 0) {
        if (siz == 3) {
            img = (u32 *)img + ult * width;
        }
        else if (siz == 2) {
            img = (u16 *)img + ult * width;
        }
        else if (siz == 1) {
            img = (u8 *)img + ult * width;
        }
        else {
            img = (u8 *)img + ult * width / 2;
        }
        height = lrt - ult + 1;
    }
    else {
        tile = ((s64)uls << 48) + ((s64)ult << 32) + ((s64)lrs << 16) + lrt;
    }
    if (type == G_IM_FMT_RGBA || type == G_IM_FMT_CI) {
        if ((blitAttribute & BLIT_ATTR_XLU) || (blitAttribute & BLIT_ATTR_ZDIST) || (blitAttribute & BLIT_ATTR_TALL)) {
            mode = BLIT_MODE_TEXTURE;
        }
        else {
            mode = BLIT_MODE_COPY;
        }
    }
    else if (type == 5) {
        mode = BLIT_MODE_TWO_TILE;
    }
    else if (type == G_IM_FMT_I || type == G_IM_FMT_IA) {
        if (blitAttribute & BLIT_ATTR_ALPHA_COMPARE) {
            mode = BLIT_MODE_INTENSITY_ALPHA;
        }
        else {
            mode = BLIT_MODE_INTENSITY;
        }
    }
    if (type == G_IM_FMT_IA) {
        BlitSetAttribute(BLIT_ATTR_XLU);
    }
    if (mode != blitMode) {
        blit_set_mode(mode);
    }
    if (img == blit_cache_image && blit_cache_tile == tile) {
        return;
    }
    if (tile != 0) {
        masks = blit_mask_bits(uls - lrs);
        maskt = blit_mask_bits(ult - lrt);
    }
    else {
        masks = blit_mask_bits(width);
        maskt = blit_mask_bits(height);
    }
    if (type == G_IM_FMT_RGBA) {
        if (blitLUT != 0) {
            gDPSetTextureLUT(blit_dl++, G_TT_NONE);
            blitLUT = 0;
        }
        if (siz == 2) {
            if (tile != 0) {
                TILE(G_IM_FMT_RGBA, 16b);
            }
            else {
                BLOCK(G_IM_FMT_RGBA, 16b);
            }
        }
        else {
            if (tile != 0) {
                TILE(G_IM_FMT_RGBA, 32b);
            }
            else {
                BLOCK(G_IM_FMT_RGBA, 32b);
            }
        }
    }
    else if (type == G_IM_FMT_CI || type == 5) {
        if (blitLUT != 1) {
            gDPSetTextureLUT(blit_dl++, G_TT_RGBA16);
            blitLUT = 1;
        }
        if (siz == 1) {
            if (tile != 0) {
                TILE(G_IM_FMT_CI, 8b);
            }
            else {
                BLOCK(G_IM_FMT_CI, 8b);
            }
            if (type == 5) {
                gDPSetTile(blit_dl++, G_IM_FMT_CI, G_IM_SIZ_4b, ((width + 7) >> 3), 0, 1, pal, G_TX_CLAMP, maskt,
                           G_TX_NOLOD, G_TX_CLAMP, masks + 1, 15);
                gDPSetTileSize(blit_dl++, 1, uls << G_TEXTURE_IMAGE_FRAC, ult << G_TEXTURE_IMAGE_FRAC,
                               (uls + width - 1) << G_TEXTURE_IMAGE_FRAC, (ult + height - 1) << G_TEXTURE_IMAGE_FRAC);
            }
        }
        else {
            if (tile != 0) {
                TILE4(G_IM_FMT_CI);
            }
            else {
                BLOCK4(G_IM_FMT_CI);
            }
        }
    }
    else if (type == G_IM_FMT_I) {
        if (blitLUT != 0) {
            gDPSetTextureLUT(blit_dl++, G_TT_NONE);
            blitLUT = 0;
        }
        if (siz == 1) {
            if (tile != 0) {
                TILE(G_IM_FMT_I, 8b);
            }
            else {
                BLOCK(G_IM_FMT_I, 8b);
            }
        }
        else {
            if (tile != 0) {
                TILE4(G_IM_FMT_I);
            }
            else {
                BLOCK4(G_IM_FMT_I);
            }
        }
    }
    else if (type == G_IM_FMT_IA) {
        if (blitLUT != 0) {
            gDPSetTextureLUT(blit_dl++, G_TT_NONE);
            blitLUT = 0;
        }
        if (siz == 2) {
            if (tile != 0) {
                TILE(G_IM_FMT_IA, 16b);
            }
            else {
                BLOCK(G_IM_FMT_IA, 16b);
            }
        }
        else if (siz == 1) {
            if (tile != 0) {
                TILE(G_IM_FMT_IA, 8b);
            }
            else {
                BLOCK(G_IM_FMT_IA, 8b);
            }
        }
        else {
            if (tile != 0) {
                TILE4(G_IM_FMT_IA);
            }
            else {
                BLOCK4(G_IM_FMT_IA);
            }
        }
    }
    blit_cache_image = img;
    blit_cache_tile = tile;
}
