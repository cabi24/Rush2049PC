#include "game/game.h"

VIEW(f32[13], gCarMass);
#define gCarMass gCarMass_view[0]

VIEW(f32, carDurability);
#define carDurability carDurability_view

VIEW(s32, gTune);
#define gTune gTune_view

f32 CalcMass(void *arg0)
{
  f32 d = 0.0f;
  if (mass_unit) {}
  d = (&carDurability)[((s8 *) arg0)[0xE]] - mass_durability_base;
  return ((&gCarMass)[((u8 *) arg0)[8]] + d * mass_unit + ((f32) gMassiveMass) * mass_unit) * ((Tune44 *) &gTune)[((s8 *) arg0)[0xB]].values[1];
}
