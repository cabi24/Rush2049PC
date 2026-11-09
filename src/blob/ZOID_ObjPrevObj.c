#include "game/game.h"

VIEW(s32, ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* PC: the scene node's host layout (docs/port/POINTERS.md 9): 0x68 bytes, pointer slots pointer-sized. The node whose
 * next sibling is arg0, or -1. */
typedef struct { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } PcNode;
s16 ZOID_ObjPrevObj(s16 arg0)
{
  s32 i;
  for (i = 0; i < gNumObjects; i++)
  {
    if (((PcNode *) &ObjInstanceList)[i].next == arg0)
    {
      return i;
    }
  }
  return -1;
}
