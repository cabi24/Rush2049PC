/* modelgo.c: start the car simulation (MAT3 mdrive.c, modelgo()).
 *
 * The race flow calls it when the cars may move (the countdown, the attract mode, after a pause). */
#include "game/game.h"

/* The model record (tModelData, docs/port/CARS.md 4): no field is read here; the loop steps 0x808 bytes, its size. */
typedef struct tModelData tModelData;

/* In the attract mode or a race, start every active car's engine sounds (StartCarSounds); outside the attract mode
 * let the Rumble Pak run (gRumble, MAT3 PS_RumbleGo); then let the model thread step the cars (modelrun). */
void modelgo(void)
{
  s8 *m;
  s32 i;

  if ((gGameState & GS_DEMO) || (gGameState & GS_RACE_MASK))
  {
    for (i = 0, m = (s8 *) PC_MODELS; i < (s8) gNumActiveCars; i++, m += 0x808)
    {
      StartCarSounds((tModelData *) m);
    }
  }
  if (!(gGameState & GS_DEMO))
  {
    gRumble = 1;
  }
  modelrun = 1;
}
