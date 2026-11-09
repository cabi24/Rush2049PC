/* ZOID_RemovePoly.c: take a flat polygon out of the scene (MAT3 zoidpoly.c, ZOID_RemovePoly()). */
#include "game/game.h"

/* A flat polygon (MAT3 tPoly), 88 bytes: what this file reads. */
typedef struct tPoly { s16 nverts; u8 pad[0x56]; } tPoly;

/* Free the polygon `poly` (nverts 0 frees it). If it was the last one in use, gNumPolyInsts drops back past it and
 * any free ones before it. */
void ZOID_RemovePoly(tPoly *poly) {
  poly->nverts = 0;
  if (poly + 1 == ((tPoly *)&PolyInstanceList) + gNumPolyInsts) {
    do {
      --gNumPolyInsts;
      poly--;
      if (poly->nverts) break;
    } while (gNumPolyInsts > 0);
  }
}
