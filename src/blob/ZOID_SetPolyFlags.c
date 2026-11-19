/* ZOID_SetPolyFlags.c: set flags of a flat polygon (MAT3 zoidpoly.c, ZOID_SetPolyFlags()). */
#include "game/game.h"

/* The start of a flat polygon (MAT3 tPoly). */
/* Set the flags of `mask` in polygon `poly`. */
void ZOID_SetPolyFlags(tPoly *poly, u16 mask) {
    poly->flags |= mask;
}
