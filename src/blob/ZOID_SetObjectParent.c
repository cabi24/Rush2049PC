/* ZOID_SetObjectParent.c: move a scene node under another parent (MAT3 zoid.c, ZOID_SetObjectParent()). */
#include "game/game.h"

VIEW(s16, ObjHead);
#define ObjHead ObjHead_view

/* Unlink node `node` from where it is (its parent's first-child link, the sibling before it, or the root list
 * from ObjHead) and link it in as the last child of `newparent` (ZOID_InsertObject; -1: a root). Nothing happens
 * when it is newparent's first child already; its own children go with it. */
void ZOID_SetObjectParent(s16 node, s16 newparent)
{
  ObjectInstance *nodes = (ObjectInstance *) ObjInstanceList;
  s16 parent;
  s16 prev;
  parent = ZOID_ObjParent(node);
  if ((parent == (-1)) || (parent != newparent))
  {
    if (parent >= 0)
    {
      nodes[parent].child = nodes[node].next;
    }
    else if (node == ObjHead)
    {
      ObjHead = nodes[node].next;
    }
    else
    {
      prev = ZOID_ObjPrevObj(node);
      if (prev >= 0)
      {
        nodes[prev].next = nodes[node].next;
      }
    }
    nodes[node].next = -1;
    ZOID_InsertObject(node, newparent);
  }
}
