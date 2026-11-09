/* ZOID_SetEnvColor.c: set a scene node's environment colour (MAT3 zoidtexture.c, ZOID_SetEnvColor()). */
#include "game/game.h"

/* A colour: its four bytes (red, green, blue, alpha) or the word they make (MAT3 tRGBA). */
typedef union Color { unsigned char b[4]; unsigned int w; } Color;

/* Set node `node`'s environment colour, the second colour its display lists may blend with, to *col. */
void ZOID_SetEnvColor(short node, Color *col)
{
    ObjectInstance *obj;
    obj = &ObjInstanceList[node];
    obj->envcolor = col->w;
}
