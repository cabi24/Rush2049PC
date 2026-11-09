/* ZOID_SetBlitData: a zoid blit list entry's visibility, alpha and depth (MAT3 zoidblit.c). */
#include "game/game.h"

/* Shows (hide 0) or hides entry index, sets its alpha unless negative and its depth unless 0xFFFF. A free entry
 * (BLITINST_FREE) is left alone. */
void ZOID_SetBlitData(s32 index, s32 hide, s32 alpha, unsigned short z)
{
  tBlitInstance *inst;
  u16 depth;
  inst = &gBlitInstanceList[index];
  depth = z & 0xFFFF;
  if (((s8) inst->hide) != BLITINST_FREE)
  {
    inst->hide = hide != 0;
    if (alpha >= 0)
    {
      inst->alpha = (u8) alpha;
    }
    if (z != 0xFFFF)
    {
      inst->z = depth;
    }
  }
}
