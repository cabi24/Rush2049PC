#include "game/game.h"

s32 AreWeSolid(s16 arg0)
{
  u8 *p;
  p = PC_MODEL(arg0);
  return (PC_CAR_B358(arg0)) == 0
      && *(s8 *) (p + 0x640) == 0
      && *(s16 *) (p + 0x6C4) == -1
      && *(s8 *) (p + 0x7EB) != 0
      && *(s8 *) (p + 0x7DF) == 0;
}
