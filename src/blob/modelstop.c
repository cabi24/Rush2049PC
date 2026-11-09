/* modelstop.c: stop the car simulation (MAT3 mdrive.c, modelstop()).
 *
 * The counterpart of modelgo: the race flow calls it at a pause and at the race's end. */
#include "game/game.h"

/* The model record (tModelData, docs/port/CARS.md 4): no field is read here; the loop steps 0x808 bytes, its size. */
typedef struct tModelData tModelData;

/* Stop the model thread stepping the cars (modelrun), silence every active car's engine (StopCarSounds) and,
 * outside the attract mode, the Rumble Pak. */
void modelstop(void) {
    s32 i;
    tModelData *m;
    modelrun = 0;
    for (i = 0, m = PC_MODELS; i < (s8) gNumActiveCars; i++, m = (tModelData *) ((u8 *) m + 0x808)) {
        StopCarSounds(m);
    }
    if (!(gGameState & 8)) {   /* GS_DEMO */
        PS_RumbleStop();
    }
}
