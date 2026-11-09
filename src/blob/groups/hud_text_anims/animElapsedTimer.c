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

typedef struct Pos {
    s32 x, y;
} Pos;

VIEW(Pos[], elapsedTimerPos);
#define elapsedTimerPos elapsedTimerPos_view
VIEW(u8[4], hud_box_color);
#define hud_box_color hud_box_color_view

s32 animElapsedTimer(Blit *blt)
{
    s32 x;
    s32 w;
    s32 show;
    s32 win;
    s32 digitW;
    s32 colonW;
    s8 fixedWidth;
    s32 y;

    win = blt->data;
    if (Hidden(blt, (win == 1 && gNumWindows > 1) || (win == 2 && gNumWindows < 2))) {
        blt->state = 0;
        return 1;
    }
    if (Hidden(blt, (gGameState & 8) || !(gGameState & 0x400000) || !gShowElapsed)) {
        return 1;
    }
    if (gNumWindows == 1) {
        HAL_SetFont(1);
    } else {
        HAL_SetFont(2);
    }
    digitW = HAL_GetCharWidth(56) + 1;
    fixedWidth = HAL_SetFontFixedWidth(0);
    colonW = HAL_GetCharWidth(58) + 1;
    HAL_SetFontFixedWidth(fixedWidth);
    blt->x = elapsedTimerPos[gNumWindows - 1].x - (7 * digitW + 2 * colonW) / 2;
    blt->y = elapsedTimerPos[gNumWindows - 1].y - 1;
    blt->alpha = hud_box_color[3];
    blt->half18 = 100;
    UpdateBlit(blt);
    return 1;
}
