/* ZOID_ClearPolyFlags.c: clear flags of a flat polygon (MAT3 zoidpoly.c, ZOID_ClearPolyFlags()). */
#include "game/game.h"

/* The start of a flat polygon (MAT3 tPoly). */
typedef struct Header { u16 nverts; u16 flags; } Header;

/* Clear the flags of `mask` in polygon `poly`. */
void ZOID_ClearPolyFlags(Header *poly, u16 mask) {
    poly->flags &= ~mask;
}
