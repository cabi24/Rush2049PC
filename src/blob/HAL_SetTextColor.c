#include "game/game.h"

VIEW(unsigned char[4], gFontColor);
#define gFontColor gFontColor_view
VIEW(intptr_t, blitColors);
#define blitColors blitColors_view
void HAL_SetTextColor(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
{
    if (a != gFontColor[0] || b != gFontColor[1] || c != gFontColor[2] || d != gFontColor[3]) {
        gFontColor[0] = a;
        gFontColor[1] = b;
        gFontColor[2] = c;
        gFontColor[3] = d;
        blitColors = 0;
    }
}
