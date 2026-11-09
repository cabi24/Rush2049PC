/* animLapTime: the box behind a checkpoint's split time, a blit animation function (MAT3 hud.c). */
#include "game/game.h"

#define GAME_STATE_ATTRACT 0x8       /* gGameState: attract mode (the demo) */

/* The split time's box of window animID (past the windows: hidden, its animation stopped): shown while the split
 * time is (gCheckPtTimeout), fading out over its last 3 seconds; sized to seven digits and two separators of the
 * clock's font, centred on lapTimePos, in lapTimeBGColor, depth 100. Hidden in attract mode, when the HUD hides the
 * clock or the car finished. Returns 1. */
s32 animLapTime(tBlit *blt)
{
    s32 x;
    s32 w;
    s32 digitW;
    s32 colonW;
    s32 show;
    s32 win;
    s8 fixedWidth;

    win = blt->animID;
    if (win >= gNumWindows) {
        blt->animFunc = 0;
        Hidden(blt, 1);
        return 1;
    }
    show = (gGameState & GAME_STATE_ATTRACT) || !gShowElapsed || gCheckPtTimeout[win] <= 0.0f || PC_CAR_FINISHED(win) == 1;
    if (Hidden(blt, show)) {
        return 1;
    }
    if (gNumWindows == 1) {
        HAL_SetFont(1);
    } else {
        HAL_SetFont(2);
    }
    digitW = HAL_GetCharWidth('8') + 1;
    fixedWidth = HAL_SetFontFixedWidth(0);
    colonW = HAL_GetCharWidth(':') + 1;
    HAL_SetFontFixedWidth(fixedWidth);
    if (gCheckPtTimeout[win] < 3.0f) {
        lapTimeBGColor[win][3] = gCheckPtTimeout[win] * 192.0f / 3.0f;
    } else {
        lapTimeBGColor[win][3] = 192;
    }
    w = digitW * 7 + colonW * 2;
    blt->x = lapTimePos[gNumWindows - 1][win].x - w / 2;
    blt->width = w;
    blt->y = lapTimePos[gNumWindows - 1][win].y;
    blt->height = TB_GetFontLineHeight();
    blt->alpha = lapTimeBGColor[win][3];
    blt->z = 100;
    blt->palette = lapTimeBGColor[win];
    UpdateBlit(blt);
    return 1;
}
