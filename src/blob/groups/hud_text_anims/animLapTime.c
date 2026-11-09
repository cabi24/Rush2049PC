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

VIEW(Pos[][4], lapTimePos);
#define lapTimePos lapTimePos_view

s32 animLapTime(Blit *blt)
{
    s32 x;
    s32 w;
    s32 digitW;
    s32 colonW;
    s32 show;
    s32 win;
    s8 fixedWidth;

    win = blt->data;
    if (win >= gNumWindows) {
        blt->state = 0;
        Hidden(blt, 1);
        return 1;
    }
    show = (gGameState & 8) || !gShowElapsed || gCheckPtTimeout[win] <= 0.0f || PC_CAR_FINISHED(win) == 1;
    if (Hidden(blt, show)) {
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
    if (gCheckPtTimeout[win] < 3.0f) {
        lapTimeBGColor[win][3] = gCheckPtTimeout[win] * 192.0f / 3.0f;
    } else {
        lapTimeBGColor[win][3] = 192;
    }
    w = digitW * 7 + colonW * 2;
    blt->x = lapTimePos[gNumWindows - 1][win].x - w / 2;
    blt->w = w;
    blt->y = lapTimePos[gNumWindows - 1][win].y;
    blt->h = TB_GetFontLineHeight();
    blt->alpha = lapTimeBGColor[win][3];
    blt->half18 = 100;
    blt->rgba = lapTimeBGColor[win];
    UpdateBlit(blt);
    return 1;
}
