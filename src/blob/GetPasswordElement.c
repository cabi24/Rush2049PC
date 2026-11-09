#include "game/game.h"

s32 GetPasswordElement(s32 *arg0, u32 arg1)
{
  u16 i;
  if (arg1 > 0x20)
  {
    return 0;
  }
  if (gDataBits < gNumBits + arg1)
  {
    return 0;
  }
  *arg0 = 0;
  for (i = 0; i < arg1; i++, gNumBits++)
  {
    *arg0 |= ((password_gData[gNumBits >> 3] >> (gNumBits & 7)) & 1) << i;
  }
  return 1;
}
