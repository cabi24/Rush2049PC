/* ZOID_UpdateObject.c: move a scene node (MAT3 zoid.c, ZOID_UpdateObject()). */
#include "game/game.h"

/* A matrix (MAT3 tUVMatrix, as its mat3 view): the rotation's three rows, then the position. */
typedef struct tUVMatrix { f32 uvs[3][3]; f32 pos[3]; } tUVMatrix;

/* The scene node as this file reads it: ObjInstanceList's layout (types.h ObjectInstance), its matrix typed. */
typedef struct ObjInstance {
    u32 flags, culling;
    tUVMatrix *mat;
    f32 scale, w;
    u16 index;
    s16 child, next, numLights;
    void *lights, *texture[4], *tile, *tileSize, *palette;
    u32 color, envcolor;
} ObjInstance;

VIEW(ObjInstance[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* Set node `node`'s position to pos[3] and its rotation to the 3x3 uvs, each only if given (not NULL). */
void ZOID_UpdateObject(s16 node, void *pos, void *uvs)
{
  tUVMatrix *mat = (ObjInstanceList + node)->mat;
  if (pos != ((void *) 0))
  {
    mat->pos[0] = ((f32 *) pos)[0];
    mat->pos[1] = ((f32 *) pos)[1];
    mat->pos[2] = ((f32 *) pos)[2];
  }
  if (uvs != ((void *) 0))
  {
    CopyUV(uvs, mat);
  }
}
