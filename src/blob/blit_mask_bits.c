#include "game/game.h"

s32 blit_mask_bits(s32 arg0)
{
  if (arg0 < 0)
  {
    arg0 = -arg0;
  }
  if (arg0 >= 0x201)
  {
    return 10;
  }
  if (arg0 >= 0x101)
  {
    return 9;
  }
  if (arg0 >= 0x81)
  {
    return 8;
  }
  if (arg0 >= 0x41)
  {
    return 7;
  }
  if (arg0 >= 0x21)
  {
    return 6;
  }
  if (arg0 >= 0x11)
  {
    return 5;
  }
  if (arg0 >= 9)
  {
    return 4;
  }
  if (arg0 >= 5)
  {
    return 3;
  }
  if (arg0 >= 3)
  {
    return 2;
  }
  if (arg0 >= 2)
  {
    return 1;
  }
  return 0;
}

