/* ZOID_SetFog/ZOID_SetFog.c: set a window's fog (MAT3 zoidwindow.c, ZOID_SetFog()). */
#include "game/game.h"

/* Give window `winidx` fog from depth `min` to `max` in colour (red, green, blue, alpha), and clear the screen to
 * that colour (ZOID_SetBGColor, in the 5-5-5-1 format: the sky behind the fog matches it). Returns the window. */
tWindowInfo *ZOID_SetFog(s16 min, s16 max, u8 red, u8 green, u8 blue, u8 alpha, u32 winidx)
{
    tWindowInfo *win = ZoidWinInfo + winidx;
    win->fogmin = min;
    win->fogmax = max;
    win->fogcolor[0] = red;
    win->fogcolor[1] = green;
    win->fogcolor[2] = blue;
    win->fogcolor[3] = alpha;
    ZOID_SetBGColor((u16)(((red<<8)&0xf800)|((green<<3)&0x7c0)|((blue>>2)&0x3e))|1);
    return win;
}
