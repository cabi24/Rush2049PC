/* AreWeSolid.c: is a car solid, so that other cars collide with it? (MAT3 resurrect.c, AreWeSolid()) */
#include "game/game.h"

/* Whether car `slot` is a solid part of the race: not wrecked (crash_state CRASH_NONE), not crashed, not on a
 * respawn's flight, solid again after its last respawn and not hidden. The collisions between cars, the stuck-car
 * check and the wrong-way check skip a car that is not. Returns 1 or 0. */
s32 AreWeSolid(s16 slot)
{
  tModelData *m;
  m = &pc_models[slot];
  return (pc_cars[slot].crash_state) == CRASH_NONE
      && m->crashflag == 0
      && m->resurrect.moving_state == RES_IDLE
      && m->solid != 0
      && m->hide_car == 0;
}
