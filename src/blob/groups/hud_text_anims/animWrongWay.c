/* The HUD's "wrong way" warning (MAT3 hud.c animWrongWay): an animation function of a blit. */
#include "game/game.h"

#define TEXT_WRONG_WAY 26         /* gText.single[]: "wrong way" */

/* For each window's car, during the race (not obstacle or practice, gRace 5 and 1): once it has faced backwards
 * along the track (heading beyond a quarter turn) for two seconds, a flashing "wrong way" at the window's place;
 * the time restarts while it faces the right way or has finished. Returns 1: the blit stays. */
s32 animWrongWay(void *item)
{
    s32 x;
    s32 y;
    s32 slot;
    static s8 wasStrayed[4];

    if (gRace == kObstacle) {
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    TB_SetJust(1, 1);
    for (slot = 0; slot < gNumWindows; slot++) {
        if ((-1.5707964f <= PC_CAR_HEADING(slot) && PC_CAR_HEADING(slot) <= 1.5707964f) ||
            !(gGameState & GS_RACING) || PC_CAR_FINISHED(slot) == 1 || !gShowWrongWay || gRace == kPractice) {
            wrongWayTime[slot] = gSeconds;
            wasStrayed[slot] = 0;
            continue;
        }
        wasStrayed[slot] = 1;
        if (gSeconds - wrongWayTime[slot] < 2.0f) {
            continue;
        }
        if (gNumWindows == 1) {
            HAL_SetFont(10);
        } else {
            HAL_SetFont(11);
        }
        HAL_SetTextTranslucency(255.0f * TB_GetFlashTextCycle());
        x = wrongWayPos[gNumWindows - 1][slot].x;
        y = wrongWayPos[gNumWindows - 1][slot].y;
        TB_SetTextColor(0);
        TB_DrawText(x + 1, y + 1, gText.single[TEXT_WRONG_WAY]);
        TB_SetTextColor(14);
        TB_DrawText(x, y, gText.single[TEXT_WRONG_WAY]);
    }
    TB_SetJust(0, 3);
    HAL_SetTextTranslucency(-1.0f);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
