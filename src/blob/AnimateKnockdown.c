#include "game/game.h"

/* PC: arg0 is a visual (struct pc_visual) whose data is the target, a level-object node in its N64 layout; both the
 * data and the target's state (+0x6C, the spin rate RotateUV reads) are 32-bit window addresses, and the
 * state goes to RotateUV zero-extended, as the pointer it is (docs/port/CARS.md, POINTERS.md 10). */
void AnimateKnockdown(void *arg0, s16 arg1)
{
  u8 *target;
  void *rate;
  s16 var_v1;
  if (arg1 == 0)
  {
    ReleaseEnviron(arg0, 1);
  }
  if (gPauseState == 0)
  {
    target = (u8 *) PC_N64P(((struct pc_visual *) arg0)->data);
    rate = (void *) PC_N64P(*((u32 *) (target + 0x6C)));
    var_v1 = *((s16 *) (target + 0x5A));
    if (var_v1 != 0)
    {
      *((s16 *) (target + 0x5A)) = (s16) (var_v1 - 1);
      RotateUV(rate, target + 0x14);
      var_v1 = *((s16 *) (target + 0x5A));
    }
    if (var_v1 == 0)
    {
      ReleaseEnviron(arg0, 1);
    }
  }
}
