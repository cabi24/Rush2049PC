/* update_reckon_base.c: a car's dead-reckoning base (MAT3 rekon.c, update_reckon_base()).
 *
 * The base is the acceleration, velocity and position the next snapshot (dead_reckon_posuv) starts from. */
#include "game/game.h"

/* The model record (tModelData, docs/port/CARS.md 4), the fields used here. */
typedef struct tModelData {
  _tCar *car;
  Tune44 *tune;
  u8 pad008[0x20C];
  f32 rwa[3];                   /* 0x214 the acceleration in the world */
  f32 rwv[3];                   /* 0x220 the velocity in the world, feet a second */
  f32 rwr[3];                   /* 0x22C the position in the world, feet */
  u8 pad238[0x3A0 - 0x238];
  streetyp *lasttp[4];
  u8 pad3B0[0x40C - 0x3B0];
  const s16 *torquecurve;
  u8 pad410[0x648 - 0x410];
  surftyp *movingSurface;
  u8 pad64C[0x734 - 0x64C];
  f32 reckon_base_rwa[3];       /* 0x734 reckon.base_rwa */
  f32 reckon_base_rwv[3];       /* 0x740 reckon.base_rwv */
  f32 reckon_base_rwr[3];       /* 0x74C reckon.base_rwr */
  u8 pad758[0x7F4 - 0x758];
  f32 time_fudge;               /* 0x7F4 a scale on the time step (1 normally) */
} tModelData;

/* Set car `slot`'s reckoning base from its current acceleration, velocity (scaled by its time_fudge) and position.
 * `unused`: MAT3 has one parameter; the N64 function takes a second one it never reads. */
void update_reckon_base(s32 slot, M2C_UNK unused)
{
  f32 time_fudge;
  tModelData *m;
  m = (tModelData *) PC_MODEL(slot);
  time_fudge = m->time_fudge;
  m->reckon_base_rwa[0] = m->rwa[0];
  m->reckon_base_rwa[1] = m->rwa[1];
  m->reckon_base_rwa[2] = m->rwa[2];
  m->reckon_base_rwv[0] = m->rwv[0] * time_fudge;
  m->reckon_base_rwv[1] = m->rwv[1] * time_fudge;
  m->reckon_base_rwv[2] = m->rwv[2] * time_fudge;
  m->reckon_base_rwr[0] = m->rwr[0];
  m->reckon_base_rwr[1] = m->rwr[1];
  m->reckon_base_rwr[2] = m->rwr[2];
}
