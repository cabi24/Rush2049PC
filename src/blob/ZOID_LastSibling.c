#include "game/game.h"

VIEW(volatile unsigned char, ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* PC: the scene node's host layout (docs/port/POINTERS.md 9): 0x68 bytes, pointer slots pointer-sized. The last node
 * of arg0's sibling chain (a ring stops at arg0). */
typedef struct { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } PcNode;
s16 ZOID_LastSibling(s16 arg0)
{
  s16 last;
  s16 i = arg0;
  do
  {
    last = i;
    i = ((PcNode *) &ObjInstanceList)[i].next;
  } while ((i >= 0) && (i != arg0));
  return last;
}
