#include "game/game.h"

void FVecInterp(f32 *arg0, f32 *arg1, f32 arg2, f32 *arg3)
{
  s16 i;
  f32 c;
  i = 0;
  do
  {
    i += 1;
    c = 1.0f - arg2;
    *arg3 = arg0[0] * c + arg1[0] * arg2;
    arg3++;
    arg0++;
    arg1++;
  }
  while (i < 3);
}
