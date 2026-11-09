/* modelstop.c: stop the car simulation (MAT3 mdrive.c, modelstop()).
 *
 * The counterpart of modelgo: the race flow calls it at a pause and at the race's end. */
#include "game/game.h"

VIEW(s32, gGameState);
#define gGameState gGameState_view

VIEW(u8, gNumActiveCars);
#define gNumActiveCars gNumActiveCars_view

/* The model record (tModelData, docs/port/CARS.md 4): only its size, 0x808, matters here. */
typedef struct ModelRecord
{
  u8 pad000[0x7C6];
  u16 slot;                     /* 0x7C6 */
  u8 pad7C8[0x7E8 - 0x7C8];
  u8 laps;                      /* 0x7E8 */
} ModelRecord;

VIEW(volatile short, modelrun);
#define modelrun modelrun_view

/* Stop the model thread stepping the cars (modelrun), silence every active car's engine (StopCarSounds) and,
 * outside the attract mode, the Rumble Pak. */
void modelstop(void) {
    s32 i;
    ModelRecord *m;
    modelrun = 0;
    for (i = 0, m = PC_MODELS; i < (s8) gNumActiveCars; i++, m = (ModelRecord *) ((u8 *) m + 0x808)) {
        StopCarSounds(m);
    }
    if (!(gGameState & 8)) {   /* GS_DEMO */
        PS_RumbleStop();
    }
}
