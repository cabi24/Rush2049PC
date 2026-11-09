#include "game/game.h"

VIEW(s32[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

VIEW(s16, ObjHead);
#define ObjHead ObjHead_view

/* PC: the scene node's host layout (docs/port/POINTERS.md 9): 0x68 bytes, pointer slots pointer-sized. Unlinks node
 * arg0 from its parent's child list, its older sibling or the root list (ObjHead), then links it under arg1
 * (ZOID_InsertObject); nothing when it is arg1's child already. */
typedef struct { u32 flags, w4; void *obj; f32 scale, w; u16 model; s16 child, next, mode; void *light, *tex[4], *rect, *clip, *pal; u32 prim, env; } PcNode;
void ZOID_SetObjectParent(s16 arg0, s16 arg1)
{
  PcNode *nodes = (PcNode *) &ObjInstanceList;
  s16 parent;
  s16 prev;
  parent = ZOID_ObjParent(arg0);
  if ((parent == (-1)) || (parent != arg1))
  {
    if (parent >= 0)
    {
      nodes[parent].child = nodes[arg0].next;
    }
    else if (arg0 == ObjHead)
    {
      ObjHead = nodes[arg0].next;
    }
    else
    {
      prev = ZOID_ObjPrevObj(arg0);
      if (prev >= 0)
      {
        nodes[prev].next = nodes[arg0].next;
      }
    }
    nodes[arg0].next = -1;
    ZOID_InsertObject(arg0, arg1);
  }
}
