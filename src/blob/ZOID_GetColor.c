/* ZOID_GetColor.c: a scene node's colour (MAT3 zoidtexture.c, ZOID_GetColor()). */
#include "game/game.h"

/* The address of node `node`'s primitive colour (four bytes: red, green, blue, alpha), for the caller to read or
 * change. */
void *ZOID_GetColor(s16 node) {
    return &(ObjInstanceList + node)->color;
}
