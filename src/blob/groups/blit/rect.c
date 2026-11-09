#include "game/game.h"
#define Gfx GfxView   /* this file's own Gfx: include/game's Gfx is another type */

typedef union {
    Gwords words;
    long long int force_structure_alignment;
} Gfx;

#define _SHIFTL(v, s, w) ((u32)(((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define G_RDPPIPESYNC 0xE7
#define G_SETPRIMCOLOR 0xFA
#define G_FILLRECT 0xF6
#define gDPNoParam(pkt, cmd) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(cmd, 24, 8); \
    _g->words.w1 = 0; \
}
#define gDPPipeSync(pkt) gDPNoParam(pkt, G_RDPPIPESYNC)
#define gDPSetPrimColor(pkt, m, l, r, g, b, a) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(G_SETPRIMCOLOR, 24, 8) | _SHIFTL(m, 8, 8) | _SHIFTL(l, 0, 8)); \
    _g->words.w1 = (_SHIFTL(r, 24, 8) | _SHIFTL(g, 16, 8) | _SHIFTL(b, 8, 8) | _SHIFTL(a, 0, 8)); \
}
#define gDPFillRectangle(pkt, ulx, uly, lrx, lry) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(G_FILLRECT, 24, 8) | _SHIFTL((lrx), 14, 10) | _SHIFTL((lry), 2, 10)); \
    _g->words.w1 = (_SHIFTL((ulx), 14, 10) | _SHIFTL((uly), 2, 10)); \
}

VIEW(Gfx *, blit_dl);
#define blit_dl blit_dl_view

VIEW(u8[4], blitPrimColor);
#define blitPrimColor blitPrimColor_view

void BlitRect(s32 x0, s32 y0, s32 x1, s32 y1, u8 *color) {
    if (x0 < blitXmin) {
        x0 = blitXmin;
    }
    if (y0 < blitYmin) {
        y0 = blitYmin;
    }
    if (x1 > blitXmax) {
        x1 = blitXmax;
    }
    if (y1 > blitYmax) {
        y1 = blitYmax;
    }
    if (x1 < x0 || y1 < y0) {
        return;
    }
    gDPPipeSync(blit_dl++);
    gDPSetPrimColor(blit_dl++, 0, 0, color[0], color[1], color[2], color[3]);
    blitPrimColor[3] = color[3];
    BlitColors(color, -1, -1, 0);
    blit_set_mode(1);
    BlitSetAttribute(0x4000);
    gDPFillRectangle(blit_dl++, x0, y0, x1 + 1, y1 + 1);
    gDPPipeSync(blit_dl++);
    BlitClearAttribute(0x4000);
}
