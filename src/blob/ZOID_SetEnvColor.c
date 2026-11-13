/* ZOID_SetEnvColor.c: set a scene node's environment colour (MAT3 zoidtexture.c, ZOID_SetEnvColor()). */
#include "game/game.h"

/* Set node `node`'s environment colour, the second colour its display lists may blend with, to *col. */
void ZOID_SetEnvColor(short node, tRGBA *col)
{
    ObjectInstance *obj;
    obj = &ObjInstanceList[node];
    obj->envcolor = col->rgba;
}
