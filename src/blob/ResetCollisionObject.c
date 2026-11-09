#include "game/game.h"

VIEW(intptr_t, stree_surf);
#define stree_surf stree_surf_view

VIEW(intptr_t, stree_svert);
#define stree_svert stree_svert_view

VIEW(intptr_t, stree_save);
#define stree_save stree_save_view

void *memcpy(void *, const void *, u32);
/* PC: the N64 version assigns new_var in one memcpy argument and reads it in the next, which only IDO's
 * left-to-right evaluation makes defined (x86-64 gcc reads it first). The same copies, in order. */
void ResetCollisionObject(s32 arg0)
{
  intptr_t var_s0;
  s32 var_s1;
  var_s0 = stree_save;
  for (var_s1 = 0; var_s1 < ((s32) stree_savenum); var_s1++, var_s0 += 0x20)
  {
    if (arg0 == (*((u16 *) var_s0)))
    {
      memcpy((void *) (stree_surf + ((*((u16 *) (var_s0 + 2))) * 0x18) + 4), (void *) (var_s0 + 4), 0x12);
      memcpy((void *) (((*((u16 *) (var_s0 + 0x16))) * 8) + stree_svert), (void *) (var_s0 + 0x18), 8);
    }
  }
}
