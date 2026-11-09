/* ZOID_SetObjLights.c: give a scene node its own lights (MAT3 zoidlight.c, ZOID_SetObjLights()). */
#include "game/game.h"


/* Light node `node` with its own `numLights` lights at `lights` instead of the scene's (-1: the scene's again). */
void ZOID_SetObjLights(s32 node, s32 numLights, uintptr_t lights)
{
    ObjectInstance *obj = &ObjInstanceList[node];
    obj->numLights = numLights;
    obj->lights = (void *)lights;
}
