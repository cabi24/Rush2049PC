/* update_reckon_base.c: a car's dead-reckoning base (MAT3 rekon.c, update_reckon_base()).
 *
 * The base is the acceleration, velocity and position the next snapshot (dead_reckon_posuv) starts from. */
#include "game/game.h"

/* Set car `slot`'s reckoning base from its current acceleration, velocity (scaled by its time_fudge) and position.
 * `unused`: MAT3 has one parameter; the N64 function takes a second one it never reads. */
void update_reckon_base(s32 slot, M2C_UNK unused)
{
  f32 time_fudge;
  tModelData *m;
  m = &pc_models[slot];
  time_fudge = m->time_fudge;
  m->reckon.base_rwa[0] = m->rwa[0];
  m->reckon.base_rwa[1] = m->rwa[1];
  m->reckon.base_rwa[2] = m->rwa[2];
  m->reckon.base_rwv[0] = m->rwv[0] * time_fudge;
  m->reckon.base_rwv[1] = m->rwv[1] * time_fudge;
  m->reckon.base_rwv[2] = m->rwv[2] * time_fudge;
  m->reckon.base_rwr[0] = m->rwr[0];
  m->reckon.base_rwr[1] = m->rwr[1];
  m->reckon.base_rwr[2] = m->rwr[2];
}
