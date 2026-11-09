#include "game/game.h"

s16 TB_CenterText(void *arg0, s16 arg1)
{
  unsigned int new_var;
  new_var = HAL_GetTextWidth(arg0, -1);
  return (s16) (arg1 - (new_var >> 1));
}
