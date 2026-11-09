#include "game/game.h"

typedef struct { f32 x, y; } Point;
VIEW(Point[], menu_text_depth_curve);
#define menu_text_depth_curve menu_text_depth_curve_view

void HAL_SetMenuTextDepth(f32 value) {
    s32 i;
    f32 a, b, d;

    i = 0;
    while (value > menu_text_depth_curve[i].x && menu_text_depth_curve[i].x < 32000.0f) {
        i++;
    }
    if (i > 0) {
        d = menu_text_depth_curve[i].x - menu_text_depth_curve[i - 1].x;
        b = menu_text_depth_curve[i].y - menu_text_depth_curve[i - 1].y;
        a = value - menu_text_depth_curve[i - 1].x;
        value = a * b / d;
        value += menu_text_depth_curve[i - 1].y;
    }
    gDepth = value;
    if (value > 0.0f) {
        gFontAttribute |= 0x10;
        BlitZDist(value);
    } else {
        gFontAttribute &= ~0x10;
    }
}
