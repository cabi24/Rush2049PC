/* ZOID_RemovePoly.c: take a flat polygon out of the scene (MAT3 zoidpoly.c, ZOID_RemovePoly()). */
#include "game/game.h"

/* A flat polygon (MAT3 tPoly), 88 bytes: what this file reads. */
typedef struct tPoly { s16 nverts; u8 pad[0x56]; } tPoly;

/* Free the polygon `poly` (nverts 0 frees it). If it was the last one in use, gNumPolyInsts drops back past it and
 * any free ones before it. (The count is tested before the polygon below it: the N64's order read the polygon before
 * the list once the count reached 0, and stopped either way.) */
void ZOID_RemovePoly(tPoly *poly) {
  poly->nverts = 0;
  if (poly + 1 == ((tPoly *)&PolyInstanceList) + gNumPolyInsts) {
    do {
      --gNumPolyInsts;
      poly--;
    } while (gNumPolyInsts > 0 && !poly->nverts);
  }
}
