/* animCountdownTimer: the time left, a text blit's draw function (MAT3 hud.c); and the countdown's beep. */
#include "game/game.h"

#define SOUND_COUNTDOWN_BEEP 2

#define sc gScheduler_view
#define gFontSem text_lock_queue_view

typedef struct tSchedCtl { u8 pad000[0x27C]; s32 frameCount; } tSchedCtl;

VIEW(volatile tSchedCtl, gScheduler);

VIEW(OSMesgQueue, text_lock_queue);

/* Draws the time left (GetCountdownTimeRemaining, seconds) as m:ss in yellow with a black shadow, centred on
 * countdownTimerPos (a split-screen stunt race: on the map's place), without a leading zero; beeps at 30 seconds and
 * each of the last 5, when it also blinks. Nothing outside a race, when the HUD hides it, in a race, practice or
 * circuit, in a points battle or a stunt race of the timeless mode. Returns 1. */
s32 animCountdownTimer(void *blt)
{
    s32 t;
    u8 digits[5];
    u8 str[2];
    s32 digitW, colonW, x, y, digitIdx;
    static s32 sec;
    static s32 nextBeep = 30;

    if (!(gGameState & GS_RACING) || !gShowRemaining)
        goto skip;
    if (gRace == kBattle && gBattleMode == 0)
        goto skip;
    if (gRace == kStunt && gStuntMode == 1)
        goto skip;
    if (gRace == kOneRace || gRace == kPractice || gRace == kCircuit) {
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
            PlayGameSound_Countdown(SOUND_COUNTDOWN_BEEP, 0, 2, 0);
        }
        if (sec <= 5 && (sc.frameCount & 8))
            return 1;
    } else
        nextBeep = 30;
    x = countdownTimerPos[gNumWindows - 1][0];
    y = countdownTimerPos[gNumWindows - 1][1];
    HAL_SetTextDepth(0.0f);
    TB_SetJust(kTB_JustCenter, kTB_JustTop);
    if (gRace == kStunt && gNumWindows > 1) {
        x = hud_mapPos[gNumWindows - 1].x;
        y = hud_mapPos[gNumWindows - 1].y;
        TB_SetJust(kTB_JustCenter, kTB_JustCenter);
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
            TB_SetTextColor(kTB_BlackText);
            TB_DrawText(x + 1, y + 1, str);
            TB_SetTextColor(kTB_YellowText);
            TB_DrawText(x, y, str);
        }
        if (digitIdx == 1 || digitIdx == 2)
            x += (digitW + colonW) / 2;
        else
            x += digitW;
    }
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    HAL_SetTextDepth(-1.0f);
    return 1;
}

/* Plays sound effect fxid (AudPlayEffect) unless the game's sounds are off or fxid is -1; returns its handle, -1
 * when none. */
s32 PlayGameSound_Countdown(s32 fxid, s32 subId, s32 replace, u8 priority)
{
    if (!gGameSoundsEnabled) return -1;
    if (fxid == -1) return -1;
    return AudPlayEffect(fxid, subId, replace, priority);
}
