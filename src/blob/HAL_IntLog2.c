/* HAL_IntLog2 (MAT3 HAL_Support.c): an integer's base-2 logarithm, for the sound system's sizes. */
#include "game/game.h"

/* The highest set bit of value among bits 31 down to 1 (floor(log2(value)) for value > 1); 0 when none of those
 * is set (value 0 or 1). */
s32 HAL_IntLog2(s32 value)
{
  s32 bit;
  bit = 31;
  for (;;)
  {
    if ((1 << bit) & value)
    {
      return bit;
    }
    bit = (bit - 1) & 0xFF;
    if (bit == 0)
    {
      return 0;
    }
  }
}
