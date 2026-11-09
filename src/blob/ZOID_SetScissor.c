/* ZOID_SetScissor.c: set a scissor rectangle (MAT3 zoidwindow.c, ZOID_SetScissor()). */
#include "game/game.h"

/* Set the scissor rectangle `s` (MAT3 tScissor: four u16) to (ulx, uly)-(lrx, lry), screen pixels: the part of the
 * screen a window may draw on. */
void ZOID_SetScissor(u16 *s, u16 ulx, u16 uly, u16 lrx, u16 lry) {
    s[0] = ulx;
    s[1] = uly;
    s[2] = lrx;
    s[3] = lry;
}
