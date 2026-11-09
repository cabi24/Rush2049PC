#include "game/game.h"

/* PC: the Blit's host layout (NewMultiBlit.c), as every view has it (docs/port/POINTERS.md 9). */
typedef struct Blit {
    uintptr_t word0;
    void *rgba;
    uintptr_t word8;
    u16 half12;
    s16 x, y;
    u16 half18;
    s16 w, h;
    u8 alpha, byte25;
    s8 hidden;
    u8 byte27;
    s16 left, right, top, bottom;
    u32 word36;
    intptr_t state;     /* AnimFunc */
    s32 data;
} Blit;

typedef struct Pos { s32 x, y; } Pos;

VIEW(u8[4], hud_box_color);
#define hud_box_color hud_box_color_view
VIEW(Pos[][4], speedPos);
#define speedPos speedPos_view

s32 animSpeed(Blit *blt)
{
    s32 x;          /* unused (frame residual) */
    s32 win;
    s32 mode;
    s32 hide;
    s32 digitW;
    s32 y;          /* unused (frame residual) */
    s32 yoff;       /* unused (frame residual) */
    s32 w;          /* unused (frame residual) */
    s32 h;          /* unused (frame residual) */

    win = blt->data & 15;
    mode = ((u32)blt->data & 0xF0) >> 4;
    hide = (mode == 1 && gNumWindows >= 2) || (mode == 2 && gNumWindows < 2) || win >= gNumWindows;
    if (Hidden(blt, hide)) {
        blt->state = 0;
        return 1;
    }
    hide = !gShowSpeed || PC_CAR_FINISHED(PC_MODEL_SLOT(win)) == 1;
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
    blt->half18 = 100;
    UpdateBlit(blt);
    return 1;
}
