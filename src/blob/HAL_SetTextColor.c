/* HAL_SetTextColor: the colour of the text drawn next (MAT3 HAL_text.c). */
#include "game/game.h"

/* Makes red, green, blue, alpha the text's colour; a new colour forgets the blitter's colour cache so that the next
 * glyph loads it (MAT3 calls BlitClearColors). */
void HAL_SetTextColor(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha)
{
    if (red != gFontColor.c[0] || green != gFontColor.c[1] || blue != gFontColor.c[2] || alpha != gFontColor.c[3]) {
        gFontColor.c[0] = red;
        gFontColor.c[1] = green;
        gFontColor.c[2] = blue;
        gFontColor.c[3] = alpha;
        blitColors = 0;
    }
}
