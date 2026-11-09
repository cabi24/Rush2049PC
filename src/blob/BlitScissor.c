/* BlitScissor: the blitter's clipping rectangle (MAT3 blit_pc.c). */
#include "game/game.h"

/* Sets the rectangle the blitter clips to, ulx..lrx and uly..lry inclusive, bounded by the screen (MAT3 bounds it by
 * its fixed 640x448; the N64 by screen_width and screen_height). BlitDraw and BlitRect clip to it. */
void BlitScissor(s32 ulx, s32 uly, s32 lrx, s32 lry) {
    s32 right_edge;
    s32 bottom_edge;

    if (ulx < 0) {
        blitXmin = 0;
    } else {
        blitXmin = ulx;
    }
    if (uly < 0) {
        blitYmin = 0;
    } else {
        blitYmin = uly;
    }
    right_edge = screen_width - 1;
    if (right_edge < lrx) {
        blitXmax = right_edge;
    } else {
        blitXmax = lrx;
    }
    bottom_edge = screen_height - 1;
    if (bottom_edge < lry) {
        blitYmax = bottom_edge;
        return;
    }
    blitYmax = lry;
}
