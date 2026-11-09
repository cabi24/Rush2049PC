#include "game/game.h"


#define sc gScheduler_view
#define mapPos hud_mapPos_view
#define gFontSem text_lock_queue_view

typedef struct tSchedCtl { u8 pad000[0x27C]; s32 frameCount; } tSchedCtl;

VIEW(volatile tSchedCtl, gScheduler);

VIEW(s32[4][2], countdownTimerPos);
#define countdownTimerPos countdownTimerPos_view
VIEW(s32[4][2], hud_mapPos);

VIEW(OSMesgQueue, text_lock_queue);

s32 animCountdownTimer(void *blt)
{
    s32 t;
    u8 digits[5];
    u8 str[2];
    s32 digitW, colonW, x, y, digitIdx;
    static s32 sec;
    static s32 nextBeep = 30;

    if (!(gGameState & 0x400000) || !gShowRemaining)
        goto skip;
    if (gRace == 6 && gBattleMode == 0)
        goto skip;
    if (gRace == 4 && gStuntMode == 1)
        goto skip;
    if (gRace == 0 || gRace == 1 || gRace == 3) {
skip:
        return 1;
    }
    t = GetCountdownTimeRemaining();
    if (t < 0)
        t = 0;
    sec = t;
    if (sec <= 30) {
        if (sec <= nextBeep) {
            if (nextBeep > 5)
                nextBeep = 5;
            else
                nextBeep--;
            PlayGameSound_Countdown(2, 0, 2, 0);
        }
        if (sec <= 5 && (sc.frameCount & 8))
            return 1;
    } else
        nextBeep = 30;
    x = countdownTimerPos[gNumWindows - 1][0];
    y = countdownTimerPos[gNumWindows - 1][1];
    HAL_SetTextDepth(0.0f);
    TB_SetJust(1, 3);
    if (gRace == 4 && gNumWindows > 1) {
        x = mapPos[gNumWindows - 1][0];
        y = mapPos[gNumWindows - 1][1];
        TB_SetJust(1, 1);
    }
    digits[0] = t / 600 + '0';
    digits[1] = (t / 60) % 10 + '0';
    digits[2] = ':';
    digits[3] = (t / 10) % 6 + '0';
    digits[4] = t % 10 + '0';
    if (gNumWindows == 1)
        HAL_SetFont(9);
    else if (gNumWindows <= 3)
        HAL_SetFont(8);
    else
        HAL_SetFont(5);
    digitW = HAL_GetCharWidth('8') + 1;
    colonW = HAL_GetCharWidth(':') + 1;
    x -= (3 * digitW + colonW) / 2;
    for (digitIdx = 0; digitIdx < 5; digitIdx++) {
        str[0] = digits[digitIdx];
        str[1] = 0;
        if (digitIdx > 0 || str[0] != '0') {
            TB_SetTextColor(0);
            TB_DrawText(x + 1, y + 1, str);
            TB_SetTextColor(14);
            TB_DrawText(x, y, str);
        }
        if (digitIdx == 1 || digitIdx == 2)
            x += (digitW + colonW) / 2;
        else
            x += digitW;
    }
    TB_SetJust(0, 3);
    HAL_SetTextDepth(-1.0f);
    return 1;
}

 

s32 PlayGameSound_Countdown(s32 fxid, s32 subId, s32 replace, u8 priority)
{
    if (!gGameSoundsEnabled) return -1;
    if (fxid == -1) return -1;
    return AudPlayEffect(fxid, subId, replace, priority);
}
