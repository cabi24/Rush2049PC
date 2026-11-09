/* ZOID_ObjParent.c: the node a scene node is the first child of (MAT3 zoid.c, ZOID_ObjParent()). */
#include "game/game.h"

/* The node whose first child is `node`, or -1 (also when `node` is a later sibling: ZOID_TrueObjParent finds
 * those). */
s16 ZOID_ObjParent(s16 node)
{
  s32 i;
  for (i = 0; i < gNumObjects; i++)
  {
    if ((ObjInstanceList + i)->child == node)
    {
      return i;
    }
  }
  return -1;
}
