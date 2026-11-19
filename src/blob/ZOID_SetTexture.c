/* ZOID_SetTexture.c: give a scene node a texture of its own (MAT3 zoidtexture.c, ZOID_SetTexture()). */
#include "game/game.h"

/* Draw node `node` with texture `tex` (a texture definition) in place of its model's: at level of detail `lod`
 * (0-3), or at all four when `lod` is negative. NULL gives the model's own back. */
void ZOID_SetTexture(s16 node, void *tex, s32 lod)
{
  ObjectInstance *obj = ObjInstanceList + node;
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
