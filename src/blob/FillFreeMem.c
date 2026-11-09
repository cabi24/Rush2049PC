#include "game/game.h"

void FillFreeMem(s32 *arg0, u32 arg1)
{
  int new_var2;
  int new_var;
  s32 *var_v1;
  u32 var_v0;
  u32 temp_a2;
  ;
  var_v0 = 0;
  new_var = 0x7FFF0BAD;
  var_v1 = arg0;
  if ((arg1 >> 2) != 0)
  {
    do
    {
      new_var2 = 4;
      new_var2 = 1;
      var_v0 += new_var2;
      *var_v1 = new_var;
      var_v1 += new_var2;
    }
    while (var_v0 < (arg1 >> 2));
  }
}
