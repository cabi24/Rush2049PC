/* ZOID_SetTexture.c: give a scene node a texture of its own (MAT3 zoidtexture.c, ZOID_SetTexture()). */
#include "game/game.h"

/* The scene node (ObjInstanceList's record, types.h ObjectInstance) with MAT3's field names. */
typedef struct ObjInstance {
    u32 flags, culling;
    void *mat;
    f32 scale, w;
    u16 index;
    s16 child, next, numLights;
    void *lights;
    void *texture[4];           /* +0x28 a texture per level of detail in place of the model's, NULL none */
    void *tile, *tileSize, *palette;
    u32 color, envcolor;
} ObjInstance;

VIEW(ObjInstance[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* Draw node `node` with texture `tex` (a texture definition) in place of its model's: at level of detail `lod`
 * (0-3), or at all four when `lod` is negative. NULL gives the model's own back. */
void ZOID_SetTexture(s16 node, void *tex, s32 lod)
{
  ObjInstance *obj = ObjInstanceList + node;
  if (lod < 0)
  {
    obj->texture[0] = tex;
    obj->texture[1] = tex;
    obj->texture[2] = tex;
    obj->texture[3] = tex;
    return;
  }
  obj->texture[lod] = tex;
}
