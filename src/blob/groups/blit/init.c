/* BlitInit: start the blitter's display list for a frame (MAT3 blit_pc.c; the N64 side of it), and the PC's storage
 * for the two lists. */
#include "game/game.h"
#define Gfx GfxView   /* this file's own Gfx: include/game's Gfx is another type */

typedef union {
    Gwords words;
    s64 force_structure_alignment;
} Gfx;

/* SDK GBI forms, using the same self-contained types as gfx_modes. */
#define _SHIFTL(v, s, w) ((u32)(((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define G_SETBLENDCOLOR 0xF9
#define G_RDPPIPESYNC 0xE7
#define G_SETOTHERMODE_H 0xE3
#define G_GEOMETRYMODE 0xD9
#define G_MDSFT_TEXTPERSP 19
#define G_TP_NONE (0 << G_MDSFT_TEXTPERSP)

#define gDPSetColor(pkt, c, d) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(c, 24, 8); \
    _g->words.w1 = (unsigned int)(d); \
}
#define gDPSetBlendColor(pkt, r, g, b, a) \
    gDPSetColor(pkt, G_SETBLENDCOLOR, \
        _SHIFTL(r, 24, 8) | _SHIFTL(g, 16, 8) | \
        _SHIFTL(b, 8, 8) | _SHIFTL(a, 0, 8))
#define gDPPipeSync(pkt) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(G_RDPPIPESYNC, 24, 8); \
    _g->words.w1 = 0; \
}
#define gSPSetOtherMode(pkt, cmd, sft, len, data) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(cmd, 24, 8) | \
        _SHIFTL(32 - (sft) - (len), 8, 8) | _SHIFTL((len) - 1, 0, 8)); \
    _g->words.w1 = (unsigned int)(data); \
}
#define gDPSetTexturePersp(pkt, type) \
    gSPSetOtherMode(pkt, G_SETOTHERMODE_H, G_MDSFT_TEXTPERSP, 1, type)
#define gSPGeometryMode(pkt, c, s) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(G_GEOMETRYMODE, 24, 8) | _SHIFTL(~(u32)(c), 0, 24); \
    _g->words.w1 = (u32)(s); \
}
#define gSPClearGeometryMode(pkt, c) gSPGeometryMode(pkt, c, 0)

/* PC: the two text display lists hold 16-byte host Gfx; blit_display_lists is sized for 8-byte commands. */
Gfx pc_text_dl[2][2400];
#define TEXT_DL pc_text_dl

VIEW(u8[4], blitPrimColor);
#define blitPrimColor blitPrimColor_view

/* Starts the blitter for a frame, once (nothing when blit_dl is already set): resets its state (no attributes, the
 * scissor box the whole screen, depth 0, opaque, no mode, no palette, nothing cached), switches to the other of its two
 * display lists (blitDLnum), and opens it with the RDP set-up every blit relies on: the blend colour's alpha 16
 * (the alpha threshold), no perspective correction, no geometry modes, then BLIT_MODE_TEXTURE. On a screen taller
 * than 220 lines BLIT_ATTR_TALL draws textures at double height. BlitFinish closes the list. */
void BlitInit(void)
{
    if (blit_dl == 0) {
        blitAttribute = 0;
        blitXmin = 0;
        blitYmin = 0;
        blitXmax = screen_width - 1;
        blitYmax = screen_height - 1;
        blitZDist = 0;
        blitPrimColor[3] = 255;
        blit_cache_image = 0;
        blit_cache_tile = 0;
        if (++blitDLnum >= 2) blitDLnum = 0;
        blit_dl = TEXT_DL[blitDLnum];
        blitMode = -1;          /* no mode yet: the first blit_set_mode sets the cycle type */
        blitLUT = -1;
        blitColors = 0;
        gDPSetBlendColor(blit_dl++, 0, 0, 0, 16);
        gDPPipeSync(blit_dl++);
        gDPSetTexturePersp(blit_dl++, G_TP_NONE);
        gSPClearGeometryMode(blit_dl++, -1);
        blit_set_mode(BLIT_MODE_TEXTURE);
        if (screen_height >= 221) BlitSetAttribute(BLIT_ATTR_TALL);
    }
}
