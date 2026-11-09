/* AbortCar.c: take a car out of play to respawn it (MAT3 resurrect.c, AbortCar()).
 *
 * Called when a car falls out of the world, lands in water or lava, or the drone code finds it stuck
 * (DetectDroneAbort, TireSurf, the battle's damage); the respawn itself happens on the model thread
 * (UpdateResurrectingCar, Resurrect). */
#include "game/game.h"

/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the fields AbortCar sets. */
typedef struct tCarData
{
  u8 pad000[0x110]; struct pc_visual visuals[21]; u8 pad308[0x358 - 0x308];
  u8 crash_state;                               /* +0x358 CRASH_NONE, CRASH_WRECKED, CRASH_RESPAWN */
  u8 dead;                                      /* +0x359 CAR_ABORTED, CAR_ALIVE, CAR_DYING, CAR_DEAD */
} tCarData;

/* The model record (tModelData): only its size, 0x808, matters here. */
typedef struct tModelData tModelData;

/* Abort car `slot`: from five seconds into the race on, note the crash for the stunt score (a stunt race) or reset
 * its battle state (a battle), save where and when it crashed as its respawn point (SetResurrectionData,
 * INIT_MODE_CRASH) and ask the model thread to respawn it (crash_state CRASH_RESPAWN). `death` is set when the car
 * died rather than got stuck: in death mode it then stays in the race as CAR_ALIVE until its respawn, any other
 * abort marks it CAR_ABORTED. */
void AbortCar(s16 slot, s32 death)
{
  tCarData *car;
  tModelData *models;
  int aborted;
  s32 abort_flag;   /* SetResurrectionData's third argument, which it ignores */
  if (!(gModelTime < 5.0f))
  {
    ;
    aborted = -1U;    /* CAR_ABORTED */
    models = PC_MODELS;
    if (gRace == 4)   /* kStunt */
    {
      CrashedStunt((s32) slot);
    }
    if (gRace == 6)   /* kBattle */
    {
      InitBattlePlayer(slot);
    }
    SetResurrectionData(INIT_MODE_CRASH, (tModelData *) ((slot * 0x808) + ((u8 *) models)), abort_flag = 1);
    car = PC_CAR(slot);
    car->crash_state = CRASH_RESPAWN;
    if ((((s8) gDeathMode) != 0) && (death != 0))
    {
      car->dead = CAR_ALIVE;
      return;
    }
    car->dead = aborted;
  }
}
