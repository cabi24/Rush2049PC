/* ZOID_SetPolyZ.c: set a flat polygon's depth (MAT3 zoidpoly.c, ZOID_SetPolyZ()). */
#include "game/game.h"

/* Set polygon `poly`'s depth `z`, which orders the screen-space polygons. */
void ZOID_SetPolyZ(PolyInstance *poly, s16 z) {
    poly->z = z;
}
