/* ZOID_ShowPoly.c: show a hidden flat polygon again (MAT3 zoidpoly.c, ZOID_ShowPoly()). */
#include "game/game.h"

/* Show polygon `poly` again (clears POLY_HIDDEN). */
void ZOID_ShowPoly(PolyInstance *poly) {
    poly->flags &= ~POLY_HIDDEN;
}
