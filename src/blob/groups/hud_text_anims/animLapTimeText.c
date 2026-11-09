/* animLapTimeText: a checkpoint's split time, a text blit's draw function (MAT3 hud.c). */
#include "game/game.h"

#define GAME_STATE_ATTRACT 0x8       /* gGameState: attract mode (the demo) */

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

/* For each window whose split time shows (gCheckPtTimeout, counted down here while not paused): the time since the
 * last checkpoint (gLapTime) as m:ss.hhh centred on lapTimePos, each digit over a dim "8", in the flashing colour,
 * fading out over its last 3 seconds. Nothing in attract mode or when the HUD hides the clock. Returns 1. */
s32 animLapTimeText(s32 blit)
{
    s32 t;
    u8 digits[9];
    u8 str[2];
    s32 digitW;
    s32 colonW;
    s32 x;
    s32 y;
    s32 digitIdx;
    s32 slot;
    s8 fixedWidth;
    f32 remaining;

    if ((gGameState & GAME_STATE_ATTRACT) || !gShowElapsed) {
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    TB_SetJust(kTB_JustCenter, kTB_JustTop);
    if (gNumWindows == 1) {
        HAL_SetFont(1);
    } else {
        HAL_SetFont(2);
    }
    digitW = HAL_GetCharWidth('8') + 1;
    fixedWidth = HAL_SetFontFixedWidth(0);
    colonW = HAL_GetCharWidth(':') + 1;
    HAL_SetFontFixedWidth(fixedWidth);
    for (slot = 0; slot < gNumWindows; slot++) {
        if (gPauseState == 0) {
            gCheckPtTimeout[slot] -= gFrameSeconds;
        }
        if (gCheckPtTimeout[slot] <= 0.f || PC_CAR_FINISHED(slot) == 1) {
            gCheckPtTimeout[slot] = 0.f;
            continue;
        }
        remaining = gCheckPtTimeout[slot];
        t = gLapTime[slot] * 1000.0f;
        if (t < 0) {
            t = 0;
        }
        digits[0] = t / 600000 + '0';
        digits[1] = t / 60000 % 10 + '0';
        digits[2] = ':';
        digits[3] = t / 10000 % 6 + '0';
        digits[4] = t / 1000 % 10 + '0';
        digits[5] = '.';
        digits[6] = t / 100 % 10 + '0';
        digits[7] = t / 10 % 10 + '0';
        digits[8] = t % 10 + '0';
        y = lapTimePos[gNumWindows - 1][slot].y;
        str[1] = 0;
        if (remaining < 3.0f) {
            HAL_SetTextTranslucency((s32)(remaining * 255.0f / 3.0f));
        }
        x = lapTimePos[gNumWindows - 1][slot].x - (digitW * 6 + colonW * 2) / 2;
        for (digitIdx = 0; digitIdx < 9; digitIdx++) {
            str[0] = digits[digitIdx];
            if (digits[digitIdx] != ':' && digits[digitIdx] != '.') {
                str[0] = '8';
                TB_SetTextColor(kTB_BlackText);
                TB_DrawText(x, y, str);
                str[0] = digits[digitIdx];
            }
            TB_SetTextColor(kTB_FlashText);
            TB_DrawText(x, y, str);
            if (digitIdx == 1 || digitIdx == 2 || digitIdx == 4 || digitIdx == 5) {
                x += (digitW + colonW) / 2;
            } else {
                x += digitW;
            }
        }
        HAL_SetTextTranslucency(-1.0f);
    }
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
