/* ZOID_ObjPrevObj.c: the sibling before a scene node (MAT3 zoid.c, ZOID_ObjPrevObj()). */
#include "game/game.h"

/* The node whose next sibling is `node`, or -1. */
s16 ZOID_ObjPrevObj(s16 node)
{
  s32 i;
  for (i = 0; i < gNumObjects; i++)
  {
    if ((ObjInstanceList + i)->next == node)
    {
      return i;
    }
  }
  return -1;
}
