#include "game/game.h"

VIEW(s32, gGameState);
#define gGameState gGameState_view

VIEW(u8, gNumActiveCars);
#define gNumActiveCars gNumActiveCars_view

typedef struct ModelRecord 
{
  u8 pad000[0x7C6];
  u16 unk7C6;
  u8 pad7C8[0x7E8 - 0x7C8];
  u8 unk7E8;
} ModelRecord;

VIEW(volatile short, modelrun);
#define modelrun modelrun_view

void modelstop(void) {
    s32 i;
    ModelRecord *r;
    modelrun = 0;
    for (i = 0, r = PC_MODELS; i < (s8) gNumActiveCars; i++, r = (ModelRecord *) ((u8 *) r + 0x808)) {
        StopCarSounds(r);
    }
    if (!(gGameState & 8)) {
        PS_RumbleStop();
    }
}
