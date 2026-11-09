/* modelgo.c: start the car simulation (MAT3 mdrive.c, modelgo()).
 *
 * The race flow calls it when the cars may move (the countdown, the attract mode, after a pause). */
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

/* In the attract mode or a race, start every active car's engine sounds (StartCarSounds); outside the attract mode
 * let the Rumble Pak run (gRumble, MAT3 PS_RumbleGo); then let the model thread step the cars (modelrun). */
void modelgo(void)
{
  s8 *m;
  s32 i;

  if ((gGameState & 8) || (gGameState & 0x7C0000))   /* GS_DEMO, GS_RACE_MASK */
  {
    for (i = 0, m = (s8 *) PC_MODELS; i < (s8) gNumActiveCars; i++, m += 0x808)
    {
      StartCarSounds((ModelRecord *) m);
    }
  }
  if (!(gGameState & 8))   /* GS_DEMO */
  {
    gRumble = 1;
  }
  modelrun = 1;
}
