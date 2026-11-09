/* HAL_SetTextColor: the colour of the text drawn next (MAT3 HAL_text.c). */
#include "game/game.h"

VIEW(unsigned char[4], gFontColor);
#define gFontColor gFontColor_view

/* Makes red, green, blue, alpha the text's colour; a new colour forgets the blitter's colour cache so that the next
 * glyph loads it (MAT3 calls BlitClearColors). */
void HAL_SetTextColor(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha)
{
    if (red != gFontColor[0] || green != gFontColor[1] || blue != gFontColor[2] || alpha != gFontColor[3]) {
        gFontColor[0] = red;
        gFontColor[1] = green;
        gFontColor[2] = blue;
        gFontColor[3] = alpha;
        blitColors = 0;
    }
}
