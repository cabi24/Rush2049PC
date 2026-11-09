/* CheckCrash.c: a car's crash state, once a frame (MAT3 resurrect.c, CheckCrash()).
 *
 * The model thread's crash flag starts a wreck; the wreck stays on show for a while, then the car is marked for its
 * respawn, which UpdateModel carries out. */
#include "game/game.h"

/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the fields CheckCrash reads and sets. */
typedef struct tCarData
{
  u8 pad000[0xED];
  s8 restart;                                   /* +0x0ED -1 crashed, 1 restarted after a respawn */
  u8 pad0EE[0x10C - 0xEE];
  f32 crashtime;                                /* +0x10C the model time of the crash */
  struct pc_visual visuals[21];                 /* +0x110 */
  u8 pad308[0x358 - 0x308];
  u8 crash_state;                               /* +0x358 CRASH_NONE, CRASH_WRECKED, CRASH_RESPAWN */
  u8 dead;                                      /* +0x359 CAR_ABORTED, CAR_ALIVE, CAR_DYING, CAR_DEAD */
} tCarData;

/* The model record (tModelData), the fields CheckCrash reads. */
typedef struct tModelData
{
  _tCar *car; Tune44 *tune;                     /* +0x000 the physics descriptor and its tune */
  u8 pad008[0x3A0 - 0x008]; streetyp *lasttp[4]; /* +0x3A0 each tire's last road node */
  u8 pad3B0[0x40C - 0x3B0]; const s16 *torquecurve;   /* +0x40C the torque curve */
  u8 pad410[0x640 - 0x410]; s8 crashflag;       /* +0x640 the simulation crashed the car */
  u8 pad641[0x648 - 0x641]; surftyp *movingSurface;   /* +0x648 the moving surface under the car */
  u8 pad64C[0x714 - 0x64C]; f32 modelTime;      /* +0x714 fastin.modelTime: the model thread's race clock, seconds */
} tModelData;

/* Step car `slot`'s crash state. CRASH_NONE: a car still in the race (dead CAR_ALIVE or CAR_ABORTED) whose model
 * has crashed keeps the crash point as its respawn point (SetResurrectionData, INIT_MODE_CRASH) and the crash time,
 * and becomes CRASH_WRECKED. CRASH_WRECKED: after 2.25 seconds in a battle, 3.5 elsewhere, CRASH_RESPAWN.
 * CRASH_RESPAWN (and 3, which nothing sets) waits for the model thread. */
void CheckCrash(s16 slot)
{
  tCarData *car;
  tModelData *m;
  car = PC_CAR(slot);
  switch ((s8) car->crash_state)
  {
    case CRASH_NONE:
      if (((s8) car->dead) <= CAR_ALIVE)
      {
        m = PC_MODEL(slot);
        if (m->crashflag != 0)
        {
          car->restart = -1;
          SetResurrectionData(INIT_MODE_CRASH, m, 0);
          car->crash_state = CRASH_WRECKED;
          car->crashtime = (f32) m->modelTime;
        }
      }
      return;

    case CRASH_RESPAWN:
    case 3:
      return;

    case CRASH_WRECKED:
      if (gRace == kBattle)
      {
        if (((PC_MODEL_MODEL_TIME(slot)) - car->crashtime) > 2.25f)
        {
          car->crash_state = CRASH_RESPAWN;
          return;
        }
      }
      else if (((PC_MODEL_MODEL_TIME(slot)) - car->crashtime) > 3.5f)
      {
        car->crash_state = CRASH_RESPAWN;
      }
      break;
  }
}
