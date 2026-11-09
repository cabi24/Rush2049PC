/* are_all_cars_stopped.c: has the race ended for every human? (MAT3 mdrive.c, are_all_cars_stopped())
 *
 * The race flow waits on this after the finish before it goes to the results. */
#include "game/game.h"

/* 1 once every human car (only the player's own in a ghost race) that matters has slowed below 5 feet a second,
 * else 0. In death mode only cars that have run all the laps matter; with two or more players, once a player has
 * really finished, it is instead 1 four seconds (of frame time, gFrameSeconds) after the first player to complete
 * the laps was seen. In a stunt race the answer, when 1, is gStuntsDone: every stunt scored. */
s32 are_all_cars_stopped(void)
{
    s16 i;
    s32 count;
    s32 result;
    s16 slot;

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
    if (gRace == kGhost) {
        count = 1;
    } else {
        count = gNumActiveCars;
    }
    for (i = 0; i < count; i++) {
        slot = PC_MODEL_SLOT(i);
        if (PC_MODEL_DRONE_TYPE(slot) == 2) {   /* DRONE_TYPE_HUMAN */
            if (gDeathMode == 0 || gNumLaps == PC_MODEL_LAP(slot)) {
                if (PC_MODEL_SPEED(slot) > 5.0f) {
                    result = 0;
                    break;
                }
            }
        }
    }
    if (result != 0 && gRace == kStunt) {
        return (s16)gStuntsDone;
    }
    return result;
}
