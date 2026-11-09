#include "game/game.h"

typedef struct { s32 x, y; } Pos;

VIEW(s32, gGameState);
#define gGameState gGameState_view

VIEW(Pos[], elapsedTimerPos);
#define elapsedTimerPos elapsedTimerPos_view

s32 animElapsedTimerText(s32 arg0)
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

    if ((gGameState & 8) || !(gGameState & 0x400000) || !gShowElapsed) {
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
    TB_SetJust(1, 3);
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
            TB_SetTextColor(0);
            TB_DrawText(x, y, str);
            str[0] = digits[digitIdx];
        }
        TB_SetTextColor(1);
        TB_DrawText(x, y, str);
        if (digitIdx == 1 || digitIdx == 2 || digitIdx == 4 || digitIdx == 5) {
            x += (digitW + colonW) / 2;
        } else {
            x += digitW;
        }
    }
    TB_SetJust(0, 3);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
