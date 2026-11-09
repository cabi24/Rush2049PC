#include "game/game.h"

VIEW(s32, ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* PC: the scene node's host layout (docs/port/POINTERS.md 9): 0x68 bytes, pointer slots pointer-sized. 1 unless the
 * node's flag bit 8 + arg1 is set and zoid_subtree_visible finds nothing in its children. */
typedef struct { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } PcNode;
s32 zoid_node_visible(void *arg0, s32 arg1)
{
  PcNode *node = (PcNode *) arg0;
  if (!(node->flags & (0x100 << arg1)))
  {
    return 1;
  }
  if ((node->child >= 0) && (zoid_subtree_visible(&((PcNode *) &ObjInstanceList)[node->child], arg1) != 0))
  {
    return 1;
  }
  return 0;
}
