/* animLapCount: the lap counter, a text blit's draw function (MAT3 hud.c). */
#include "game/game.h"

#define GAME_STATE_ATTRACT 0x8       /* gGameState: attract mode (the demo) */
#define GAME_STATE_RESULTS 0x400000  /* gGameState: the race is on (and its end) */
#define kPractice 1   /* gRace (MAT3 enum kNumRaceTypes) */
#define kObstacle 5
#define LAP_FORMAT 206   /* gText.single: the "lap n of m" format */

typedef struct Pos {
    s32 x, y;
} Pos;

VIEW(Pos[][4], lapCountPos);
#define lapCountPos lapCountPos_view

/* Draws each window's lap ("lap n of m", centred on lapCountPos, with a black shadow) while it races, in the HUD font
 * for the number of windows, at no depth; nothing in attract mode, outside a race, in practice or the obstacle course,
 * or when the HUD hides the place. Returns 1. */
s32 animLapCount(void *item)
{
    s32 i;
    s32 x;
    s32 y;

    if ((gGameState & GAME_STATE_ATTRACT) || !(gGameState & GAME_STATE_RESULTS) || gRace == kPractice || gRace == kObstacle || !gShowPlace) {
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    gJust.v = kTB_JustTop;
    gJust.h = kTB_JustCenter;
    if (gNumWindows == 1) {
        HAL_SetFont(11);
    } else {
        HAL_SetFont(12);
    }
    for (i = 0; i < gNumWindows; i++) {
        if (gNumLaps == PC_MODEL_LAP(i)) {
            continue;
        }
        if (PC_CAR_FINISHED(i) == 1) {
            continue;
        }
        x = lapCountPos[gNumWindows - 1][i].x;
        y = lapCountPos[gNumWindows - 1][i].y;
        TB_SetTextColor(kTB_BlackText);
        TB_Printf(x + 1, y + 1, gText.single[LAP_FORMAT], PC_MODEL_LAP(i) + 1, gNumLaps);
        TB_SetTextColor(kTB_WhiteText);
        TB_Printf(x, y, gText.single[LAP_FORMAT], PC_MODEL_LAP(i) + 1, gNumLaps);
    }
    gJust.v = kTB_JustTop;
    gJust.h = kTB_JustLeft;
    HAL_SetTextDepth(-1.0f);
    return 1;
}
