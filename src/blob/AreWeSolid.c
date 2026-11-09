/* AreWeSolid.c: is a car solid, so that other cars collide with it? (MAT3 resurrect.c, AreWeSolid()) */
#include "game/game.h"

/* The model record (tModelData, docs/port/CARS.md 4): the fields AreWeSolid reads. */
typedef struct tModelData {
  u8 pad000[0x640]; s8 crashflag;                       /* +0x640 crashed */
  u8 pad641[0x6C4 - 0x641]; s16 resurrect_moving_state; /* +0x6C4 resurrect.moving_state: RES_IDLE, RES_START ... */
  u8 pad6C6[0x7DF - 0x6C6]; s8 hide_car;                /* +0x7DF hidden during the respawn's flight */
  u8 pad7E0[0x7EB - 0x7E0]; s8 solid;                   /* +0x7EB solid again after a respawn */
} tModelData;

/* Whether car `slot` is a solid part of the race: not wrecked (crash_state CRASH_NONE), not crashed, not on a
 * respawn's flight, solid again after its last respawn and not hidden. The collisions between cars, the stuck-car
 * check and the wrong-way check skip a car that is not. Returns 1 or 0. */
s32 AreWeSolid(s16 slot)
{
  tModelData *m;
  m = PC_MODEL(slot);
  return (PC_CAR_B358(slot)) == CRASH_NONE
      && m->crashflag == 0
      && m->resurrect_moving_state == RES_IDLE
      && m->solid != 0
      && m->hide_car == 0;
}
