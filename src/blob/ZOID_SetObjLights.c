/* ZOID_SetObjLights.c: give a scene node its own lights (MAT3 zoidlight.c, ZOID_SetObjLights()). */
#include "game/game.h"

/* The scene node (ObjInstanceList's record, types.h ObjectInstance) with MAT3's field names. */
typedef struct ObjInstance {
    u32 flags, culling;
    void *mat;
    f32 scale, w;
    u16 index;
    s16 child, next;
    s16 numLights;              /* +0x1E its own lights' count, -1 the scene's */
    void *lights;               /* +0x20 its own lights */
    void *texture[4], *tile, *tileSize, *palette;
    u32 color, envcolor;
} ObjInstance;

VIEW(ObjInstance[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

/* Light node `node` with its own `numLights` lights at `lights` instead of the scene's (-1: the scene's again). */
void ZOID_SetObjLights(s32 node, s32 numLights, void *lights)
{
    ObjInstance *obj = &ObjInstanceList[node];
    obj->numLights = numLights;
    obj->lights = lights;
}
