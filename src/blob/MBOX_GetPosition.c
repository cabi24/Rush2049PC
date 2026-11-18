/* MBOX_GetPosition.c: where a scene node is in the world (MAT3 mbox.c, MBOX_GetPosition()). */
#include "game/game.h"

/* The world position of node `node` into pos[3]: its own position plus each ancestor's, up the tree
 * (ZOID_TrueObjParent). Only the positions add up: the ancestors' rotations are not applied (as in MAT3). */
void MBOX_GetPosition(void *pos, int node)
{
  f32 *out = (f32 *) pos;
  ObjectInstance *nodes = ObjInstanceList;
  s32 parent;
  out[0] = nodes[node].mat->pos[0];
  out[1] = nodes[node].mat->pos[1];
  out[2] = nodes[node].mat->pos[2];
  parent = ZOID_TrueObjParent(node);
  while (parent != -1)
  {
    out[0] += nodes[parent].mat->pos[0];
    out[1] += nodes[parent].mat->pos[1];
    out[2] += nodes[parent].mat->pos[2];
    parent = ZOID_TrueObjParent(parent);
  }
}
