/* BlitRect: a filled rectangle (MAT3 blit_pc.c; the N64 side of it). */
#include "game/game.h"

/* Fills the rectangle x0..x1, y0..y1 (inclusive, clipped to the scissor box) with the RGBA colour color: the primitive
 * colour, a fill combiner (BLIT_ATTR_FILL, cleared again after), a fill rectangle. The colour's alpha becomes the
 * blitter's (blitPrimColor) and the colour is remembered as the last loaded (BlitColors with no format). */
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
    blitPrimColor.prim.c[3] = color[3];
    BlitColors(color, -1, -1, 0);
    blit_set_mode(BLIT_MODE_TEXTURE);
    BlitSetAttribute(BLIT_ATTR_FILL);
    gDPFillRectangle(blit_dl++, x0, y0, x1 + 1, y1 + 1);
    gDPPipeSync(blit_dl++);
    BlitClearAttribute(BLIT_ATTR_FILL);
}
