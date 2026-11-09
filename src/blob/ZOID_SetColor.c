/* ZOID_SetColor.c: set a scene node's colour (MAT3 zoidtexture.c, ZOID_SetColor()). */
#include "game/game.h"

/* A colour: its four bytes (red, green, blue, alpha) or the word they make (MAT3 tRGBA). */
typedef union Color { unsigned char b[4]; unsigned int w; } Color;

/* Set node `node`'s primitive colour, which its display lists may draw with, to *col. */
void ZOID_SetColor(short node, Color *col)
{
    ObjInstanceList[node].color = col->w;
}
