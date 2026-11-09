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

void modelgo(void)
{
  s8 *rec;
  s32 i;

  if ((gGameState & 8) || (gGameState & 0x7C0000))
  {
    for (i = 0, rec = (s8 *) PC_MODELS; i < (s8) gNumActiveCars; i++, rec += 0x808)
    {
      StartCarSounds((ModelRecord *) rec);
    }
  }
  if (!(gGameState & 8))
  {
    gRumble = 1;
  }
  modelrun = 1;
}

