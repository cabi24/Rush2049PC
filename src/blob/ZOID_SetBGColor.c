/* ZOID_SetBGColor.c: set the colour the screen is cleared to (MAT3 zoidwindow.c, ZOID_SetBGColor()). */
#include "game/game.h"

/* Clear the screen to `rgba5551` (the frame buffer's 16-bit format) from now on: gBgColor holds it twice, the 32-bit
 * fill colour of two pixels. */
void ZOID_SetBGColor(u16 rgba5551) {
    gBgColor = (rgba5551 << 16) | rgba5551;
}
