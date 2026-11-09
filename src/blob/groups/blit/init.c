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

VIEW(Gfx *, blit_dl);
#define blit_dl blit_dl_view

/* PC: the two text display lists hold 16-byte host Gfx; blit_display_lists is sized for 8-byte commands. */
Gfx pc_text_dl[2][2400];
#define TEXT_DL pc_text_dl

VIEW(u8[4], blitPrimColor);
#define blitPrimColor blitPrimColor_view

VIEW(intptr_t, blitColors);
#define blitColors blitColors_view

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
        blitMode = -1;
        blitLUT = -1;
        blitColors = 0;
        gDPSetBlendColor(blit_dl++, 0, 0, 0, 16);
        gDPPipeSync(blit_dl++);
        gDPSetTexturePersp(blit_dl++, G_TP_NONE);
        gSPClearGeometryMode(blit_dl++, -1);
        blit_set_mode(1);
        if (screen_height >= 221) BlitSetAttribute(0x8000);
    }
}
