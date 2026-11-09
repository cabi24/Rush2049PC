/* MBOX_GetPosition.c: where a scene node is in the world (MAT3 mbox.c, MBOX_GetPosition()). */
#include "game/game.h"

/* A matrix (MAT3 tUVMatrix, as its mat3 view): the rotation's three rows, then the position. */
typedef struct tUVMatrix { f32 uvs[3][3]; f32 pos[3]; } tUVMatrix;

/* The scene node as this file reads it: ObjInstanceList's layout (types.h ObjectInstance), its matrix typed. A
 * node's matrix is a rotation and a position relative to its parent. */
typedef struct ObjInstance {
    u32 flags, culling;
    tUVMatrix *mat;
    f32 scale, w;
    u16 index;
    s16 child, next, numLights;
    void *lights, *texture[4], *tile, *tileSize, *palette;
    u32 color, envcolor;
} ObjInstance;

/* The world position of node `node` into pos[3]: its own position plus each ancestor's, up the tree
 * (ZOID_TrueObjParent). Only the positions add up: the ancestors' rotations are not applied (as in MAT3). */
void MBOX_GetPosition(void *pos, int node)
{
  f32 *out = (f32 *) pos;
  ObjInstance *nodes = ObjInstanceList;
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
