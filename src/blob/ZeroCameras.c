#include "game/game.h"

void ZeroCameras(void)
{
  s16 var_a0;
  int var_v0;
  int var_v1;
  s32 var_s0;
  var_s0 = 0;
  do
  {
    if (var_s0 == 0)
    {
      var_a0 = 1;
    }
    else
    {
      if (var_s0 == 1)
      {
        var_v0 = 2;
      }
      else
      {
        if (var_s0 == 2)
        {
          var_v1 = 4;
        }
        else
        {
          var_v1 = 8;
        }
        var_v0 = var_v1;
      }
      var_a0 = var_v0;
    }
    ZeroCamera(var_a0);
    var_s0 += 1;
  }
  while (var_s0 != 4);
  UpdateCameras();
}

