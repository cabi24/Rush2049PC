#include "game/game.h"

s32 game_strcmp(u8 *arg0, u8 *arg1)
{
  int v;
  v = *arg0;
  if (v != 0 && v == *arg1)
  {
    do
    {
      v = arg0[1];
      arg0++;
      arg1++;
    } while (v != 0 && v == *arg1);
  }
  return v - *arg1;
}
