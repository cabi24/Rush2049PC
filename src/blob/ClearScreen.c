/* ClearScreen (MAT3 gfx.c, the N64 form): fills the frame being built with one colour. */
#include "game/game.h"
#define Gfx GfxView   /* this file's own Gfx: include/game's Gfx is another type */

typedef union {
    Gwords words;
    long long int force_structure_alignment;
} Gfx;

#define _SHIFTL(v, s, w) ((u32)(((u32)(v) & ((0x01 << (w)) - 1)) << (s)))

#define gSetImage(pkt, cmd, fmt, siz, width, i) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(cmd, 24, 8) | _SHIFTL(fmt, 21, 3) | _SHIFTL(siz, 19, 2) | _SHIFTL((width) - 1, 0, 12); \
    _g->words.w1 = (uintptr_t)(i); \
}
#define gDPSetColorImage(pkt, f, s, w, i) gSetImage(pkt, G_SETCIMG, f, s, w, i)
#define gDPNoParam(pkt, cmd) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(cmd, 24, 8); \
    _g->words.w1 = 0; \
}
#define gDPPipeSync(pkt) gDPNoParam(pkt, G_RDPPIPESYNC)
#define gDPSetColor(pkt, c, d) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(c, 24, 8); \
    _g->words.w1 = (unsigned int)(d); \
}
#define gDPSetFillColor(pkt, d) gDPSetColor(pkt, G_SETFILLCOLOR, d)
#define gDPFillRectangle(pkt, ulx, uly, lrx, lry) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(G_FILLRECT, 24, 8) | _SHIFTL((lrx), 14, 10) | _SHIFTL((lry), 2, 10)); \
    _g->words.w1 = (_SHIFTL((ulx), 14, 10) | _SHIFTL((uly), 2, 10)); \
}

/* Adds to the frame's display list a fill of the whole screen (screen_width x screen_height) of the frame's colour
 * buffer with color, a 16-bit RGBA5551 colour, between pipe syncs. The colour image is the buffer's pointer, where
 * the N64 gives the RDP its physical address (osVirtualToPhysical; StartGraphics.c says more). */
void ClearScreen(u16 color)
{
    gDPPipeSync(frame_dl++);
    gDPSetColorImage(frame_dl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, screen_width, frame_records[frame_record_index].colorBuffer);
    gDPSetFillColor(frame_dl++, ((u32)color << 16) | color);
    gDPFillRectangle(frame_dl++, 0, 0, screen_width - 1, screen_height - 1);
    gDPPipeSync(frame_dl++);
}
