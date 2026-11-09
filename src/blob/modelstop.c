/* modelstop.c: stop the car simulation (MAT3 mdrive.c, modelstop()).
 *
 * The counterpart of modelgo: the race flow calls it at a pause and at the race's end. */
#include "game/game.h"

/* Stop the model thread stepping the cars (modelrun), silence every active car's engine (StopCarSounds) and,
 * outside the attract mode, the Rumble Pak. */
void modelstop(void) {
    s32 i;
    modelrun = 0;
    for (i = 0; i < (s8) gNumActiveCars; i++) {
        StopCarSounds(PC_MODEL(i));
    }
    if (!(gGameState & GS_DEMO)) {
        PS_RumbleStop();
    }
}
