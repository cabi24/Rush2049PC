/* ZOID_ClearPolyFlags.c: clear flags of a flat polygon (MAT3 zoidpoly.c, ZOID_ClearPolyFlags()). */
#include "game/game.h"

/* The start of a flat polygon (MAT3 tPoly). */
/* Clear the flags of `mask` in polygon `poly`. */
void ZOID_ClearPolyFlags(tPoly *poly, u16 mask) {
    poly->flags &= ~mask;
}
