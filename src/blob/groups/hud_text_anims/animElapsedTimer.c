/* animElapsedTimer: the box behind the race clock, a blit animation function (MAT3 hud.c). */
#include "game/game.h"

#define GAME_STATE_ATTRACT 0x8       /* gGameState: attract mode (the demo) */
#define GAME_STATE_RESULTS 0x400000  /* gGameState: the race is on (and its end) */

/* The race clock's box: animID says for which screens (1: one window only, 2: split screens only); hidden on other
 * screens (its animation then stops), in attract mode, outside a race, or when the HUD hides the clock; else as wide
 * as the clock's seven digits and two separators in its font, centred on elapsedTimerPos, at the HUD's box alpha,
 * depth 100. Returns 1. */
s32 animElapsedTimer(tBlit *blt)
{
    s32 x;
    s32 w;
    s32 show;
    s32 win;
    s32 digitW;
    s32 colonW;
    s8 fixedWidth;
    s32 y;

    win = blt->animID;
    if (Hidden(blt, (win == 1 && gNumWindows > 1) || (win == 2 && gNumWindows < 2))) {
        blt->animFunc = 0;
        return 1;
    }
    if (Hidden(blt, (gGameState & GAME_STATE_ATTRACT) || !(gGameState & GAME_STATE_RESULTS) || !gShowElapsed)) {
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
    blt->x = elapsedTimerPos[gNumWindows - 1].x - (7 * digitW + 2 * colonW) / 2;
    blt->y = elapsedTimerPos[gNumWindows - 1].y - 1;
    blt->alpha = hud_box_color[3];
    blt->z = 100;
    UpdateBlit(blt);
    return 1;
}
