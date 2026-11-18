/* blit_load_texture: load a texture, or a part of it, into the RDP's texture memory for the blitter (the N64 side of
 * MAT3 blit_pc.c, which MAT3's GameCube build has no need of). */
#include "game/game.h"

/* The game's variants of gDPLoadTextureBlock and _4b (include/game/gbi.h): the same but for the render tile's
 * rectangle, which starts at (uls, ult) instead of (0, 0). */
#define gDPLoadTextureBlockAt(pkt, timg, fmt, siz, width, height, uls, ult, pal, cms, cmt, masks, maskt, shifts, shiftt) \
{ \
    gDPSetTextureImage(pkt, fmt, siz##_LOAD_BLOCK, 1, timg); \
    gDPSetTile(pkt, fmt, siz##_LOAD_BLOCK, 0, 0, G_TX_LOADTILE, 0 , cmt, maskt, shiftt, cms, masks, shifts); \
    gDPLoadSync(pkt); \
    gDPLoadBlock(pkt, G_TX_LOADTILE, 0, 0, (((width)*(height) + siz##_INCR) >> siz##_SHIFT) -1, GBI_CALC_DXT(width, siz##_BYTES)); \
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
    gDPLoadBlock(pkt, G_TX_LOADTILE, 0, 0, (((width)*(height)+3)>>2)-1, GBI_CALC_DXT_4b(width)); \
    gDPPipeSync(pkt); \
    gDPSetTile(pkt, fmt, G_IM_SIZ_4b, ((((width)>>1)+7)>>3), 0, G_TX_RENDERTILE, pal, cmt, maskt, shiftt, cms, masks, shifts); \
    gDPSetTileSize(pkt, G_TX_RENDERTILE, (uls) << G_TEXTURE_IMAGE_FRAC, (ult) << G_TEXTURE_IMAGE_FRAC, \
        ((uls)+(width)-1) << G_TEXTURE_IMAGE_FRAC, ((ult)+(height)-1) << G_TEXTURE_IMAGE_FRAC) \
}

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
