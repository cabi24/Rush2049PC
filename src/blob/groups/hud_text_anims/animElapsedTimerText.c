/* animElapsedTimerText: the race clock's digits, a text blit's draw function (MAT3 hud.c). */
#include "game/game.h"

typedef struct { s32 x, y; } Pos;

VIEW(s32, gGameState);
#define gGameState gGameState_view

VIEW(Pos[], elapsedTimerPos);
#define elapsedTimerPos elapsedTimerPos_view

/* Draws the race clock (gModelTime) as m:ss.hh centred on elapsedTimerPos, each digit over a dim "8" (the unlit
 * segments), the separators half a digit and half a separator wide; nothing in attract mode, outside a race, or when
 * the HUD hides the clock. Returns 1. (The thousandths are computed but not drawn.) */
s32 animElapsedTimerText(s32 blit)
{
    s32 t;
    char digits[9];
    char str[2];
    s32 digitW;
    s32 colonW;
    s8 fixedWidth;
    s32 x;
    s32 y;
    s32 digitIdx;

    if ((gGameState & GS_DEMO) || !(gGameState & GS_RACING) || !gShowElapsed) {
        return 1;
    }
    t = gModelTime * 1000.0f;
    if (t < 0) t = 0;
    digits[0] = t / 600000 % 6 + '0';
    digits[1] = t / 60000 % 10 + '0';
    digits[2] = ':';
    digits[3] = t / 10000 % 6 + '0';
    digits[4] = t / 1000 % 10 + '0';
    digits[5] = '.';
    digits[6] = t / 100 % 10 + '0';
    digits[7] = t / 10 % 10 + '0';
    digits[8] = t % 10 + '0';
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
    str[1] = 0;
    y = elapsedTimerPos[gNumWindows - 1].y;
    x = elapsedTimerPos[gNumWindows - 1].x - (digitW * 5 + colonW * 2) / 2;
    for (digitIdx = 0; digitIdx < 8; digitIdx++) {
        if ((str[0] = digits[digitIdx]) != ':' && str[0] != '.') {
            str[0] = '8';
            TB_SetTextColor(kTB_BlackText);
            TB_DrawText(x, y, str);
            str[0] = digits[digitIdx];
        }
        TB_SetTextColor(kTB_WhiteText);
        TB_DrawText(x, y, str);
        if (digitIdx == 1 || digitIdx == 2 || digitIdx == 4 || digitIdx == 5) {
            x += (digitW + colonW) / 2;
        } else {
            x += digitW;
        }
    }
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
