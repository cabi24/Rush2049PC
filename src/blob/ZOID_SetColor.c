/* ZOID_SetColor.c: set a scene node's colour (MAT3 zoidtexture.c, ZOID_SetColor()). */
#include "game/game.h"

/* Set node `node`'s primitive colour, which its display lists may draw with, to *col. */
void ZOID_SetColor(short node, tRGBA *col)
{
    ObjInstanceList[node].color = col->rgba;
}
