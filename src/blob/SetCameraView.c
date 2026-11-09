#include "game/game.h"

VIEW(s32[], cur_acc);
#define cur_acc cur_acc_view

void SetCameraView(s16 arg0)
{
  void *temp_v0;
  void *new_var;
  s8 *new_var2;
  new_var2 = &cur_acc;
  temp_v0 = (arg0 * 0xC) + new_var2;
  new_var2 = 8 + ((s8 *) (new_var = temp_v0));
  *((f32 *) (((s8 *) temp_v0) + 0)) = 0.0f;
  *((f32 *) (((s8 *) temp_v0) + 4)) = 0.0f;
  *((f32 *) new_var2) = 0.0f;
}
