#include "game/game.h"

s32 HAL_IntLog2(s32 arg0)
{
  unsigned int temp_t9;
  s32 var_v1;
  var_v1 = 0x1F;
  loop_1:
  if ((1 << var_v1) & arg0)
  {
    return var_v1;
  }

  var_v1 = (var_v1 - 1) & 0xFF;
  temp_t9 = var_v1;
  var_v1 = temp_t9;
  if (temp_t9 == 0)
  {
    return 0;
  }
  goto loop_1;
}
