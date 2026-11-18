/* CheckCrash.c: a car's crash state, once a frame (MAT3 resurrect.c, CheckCrash()).
 *
 * The model thread's crash flag starts a wreck; the wreck stays on show for a while, then the car is marked for its
 * respawn, which UpdateModel carries out. */
#include "game/game.h"

/* Step car `slot`'s crash state. CRASH_NONE: a car still in the race (dead CAR_ALIVE or CAR_ABORTED) whose model
 * has crashed keeps the crash point as its respawn point (SetResurrectionData, INIT_MODE_CRASH) and the crash time,
 * and becomes CRASH_WRECKED. CRASH_WRECKED: after 2.25 seconds in a battle, 3.5 elsewhere, CRASH_RESPAWN.
 * CRASH_RESPAWN (and 3, which nothing sets) waits for the model thread. */
void CheckCrash(s16 slot)
{
  tCarData *car;
  tModelData *m;
  car = &pc_cars[slot];
  switch ((s8) car->crash_state)
  {
    case CRASH_NONE:
      if (((s8) car->dead) <= CAR_ALIVE)
      {
        m = &pc_models[slot];
        if (m->crashflag != 0)
        {
          car->restart = -1;
          SetResurrectionData(INIT_MODE_CRASH, m, 0);
          car->crash_state = CRASH_WRECKED;
          car->crashtime = (f32) m->fastin.modelTime;
        }
      }
      return;

    case CRASH_RESPAWN:
    case 3:
      return;

    case CRASH_WRECKED:
      if (gRace == kBattle)
      {
        if (((pc_models[slot].fastin.modelTime) - car->crashtime) > 2.25f)
        {
          car->crash_state = CRASH_RESPAWN;
          return;
        }
      }
      else if (((pc_models[slot].fastin.modelTime) - car->crashtime) > 3.5f)
      {
        car->crash_state = CRASH_RESPAWN;
      }
      break;
  }
}
