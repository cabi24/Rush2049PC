/* animSpeed: the box behind the speedometer's digits, a blit animation function (MAT3 hud.c). */
#include "game/game.h"

typedef struct Pos { s32 x, y; } Pos;

VIEW(u8[4], hud_box_color);
#define hud_box_color hud_box_color_view
VIEW(Pos[][4], speedPos);
#define speedPos speedPos_view

/* The speedometer box of the window in animID's low nibble; its high nibble says for which screens (1: one window
 * only, 2: split screens only). Hidden on other screens (and its animation then stops), when the HUD hides the
 * speed or the car has finished; else placed left of speedPos by two digits of the speed's font, at the HUD's box
 * alpha, depth 100. Returns 1. */
s32 animSpeed(tBlit *blt)
{
    s32 x;          /* unused */
    s32 win;
    s32 mode;
    s32 hide;
    s32 digitW;
    s32 y;          /* unused */
    s32 yoff;       /* unused */
    s32 w;          /* unused */
    s32 h;          /* unused */

    win = blt->animID & 15;
    mode = ((u32)blt->animID & 0xF0) >> 4;
    hide = (mode == 1 && gNumWindows >= 2) || (mode == 2 && gNumWindows < 2) || win >= gNumWindows;
    if (Hidden(blt, hide)) {
        blt->animFunc = 0;
        return 1;
    }
    hide = !gShowSpeed || PC_CAR_PLACE_LOCKED(PC_MODEL_SLOT(win)) == 1;
    if (Hidden(blt, hide)) {
        return 1;
    }
    if (gNumWindows == 1) {
        HAL_SetFont(4);
    } else {
        HAL_SetFont(1);
    }
    digitW = HAL_GetCharWidth(56);
    blt->x = speedPos[gNumWindows - 1][win].x - digitW * 2 - 2;
    blt->y = speedPos[gNumWindows - 1][win].y + ((gNumWindows == 1) ? 1 : 0);
    blt->alpha = hud_box_color[3];
    blt->z = 100;
    UpdateBlit(blt);
    return 1;
}
