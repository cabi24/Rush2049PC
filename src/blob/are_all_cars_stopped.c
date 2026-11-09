#include "game/game.h"

s32 are_all_cars_stopped(void)
{
    s16 i;
    s32 n;
    s32 result;
    s16 idx;

    if (gDeathMode != 0 && gNumPlayers >= 2 && gAPlayerReallyFinished != 0) {
        if (gDeathWaitEnd > 0.0f) {
            gDeathWaitEnd += gFrameSeconds;
            if (gDeathWaitEnd > 4.0f) {
                return 1;
            }
        } else {
            for (i = 0; i < gNumPlayers; i++) {
                if (gNumLaps == PC_MODEL_LAP(i)) {
                    gDeathWaitEnd = gFrameSeconds;
                }
            }
        }
        return 0;
    }
    result = 1;
    if (gRace == 2) {
        n = 1;
    } else {
        n = gNumActiveCars;
    }
    for (i = 0; i < n; i++) {
        idx = PC_MODEL_SLOT(i);
        if (PC_MODEL_DRONE_TYPE(idx) == 2) {
            if (gDeathMode == 0 || gNumLaps == PC_MODEL_LAP(idx)) {
                if (PC_MODEL_SPEED(idx) > 5.0f) {
                    result = 0;
                    break;
                }
            }
        }
    }
    if (result != 0 && gRace == 4) {
        return (s16)gStuntsDone;
    }
    return result;
}
