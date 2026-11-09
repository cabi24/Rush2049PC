/* HAL_SetMenuTextDepth: the depth of menu text, through a curve (MAT3 HAL_text.c). */
#include "game/game.h"

/* Sets the depth text is drawn at like HAL_SetTextDepth, but first maps value through menu_text_depth_curve, a
 * piecewise-linear curve of (x, y) points whose last x is 32000 or more (a value at or below the first x is used as
 * it is). MAT3 uses the value as it is. */
void HAL_SetMenuTextDepth(f32 value) {
    s32 i;
    f32 dx_value, dy, dx;

    i = 0;
    while (value > menu_text_depth_curve[i].x && menu_text_depth_curve[i].x < 32000.0f) {
        i++;
    }
    if (i > 0) {
        dx = menu_text_depth_curve[i].x - menu_text_depth_curve[i - 1].x;
        dy = menu_text_depth_curve[i].y - menu_text_depth_curve[i - 1].y;
        dx_value = value - menu_text_depth_curve[i - 1].x;
        value = dx_value * dy / dx;
        value += menu_text_depth_curve[i - 1].y;
    }
    gDepth = value;
    if (value > 0.0f) {
        gFontAttribute |= BLIT_ATTR_ZDIST;
        BlitZDist(value);
    } else {
        gFontAttribute &= ~BLIT_ATTR_ZDIST;
    }
}
