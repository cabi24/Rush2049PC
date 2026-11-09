/* ZOID_HidePoly.c: hide a flat polygon (MAT3 zoidpoly.c, ZOID_HidePoly()). */
#include "game/game.h"

/* Hide polygon `poly` (POLY_HIDDEN) without freeing it. */
void ZOID_HidePoly(PolyInstance *poly) {
    poly->flags |= POLY_HIDDEN;
}
