#include "game/game.h"

/* PC: arg0 is the car's model (D_8014A250), which keeps its N64 layout: its car descriptor pointer at +0 is a u32
 * window address (docs/port/CARS.md 2.2), not the 8-byte pointer `*arg0` reads. */
void calcaa(void **arg0, f32 *arg1, f32 *arg2)
{
  f32 *car = (f32 *)PC_N64P(*(u32 *)arg0);
  arg2[0] = arg1[0] * car[3];
  arg2[1] = arg1[1] * car[4];
  arg2[2] = arg1[2] * car[5];
}

