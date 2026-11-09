/* ZOID_RemovePoly.c: take a flat polygon out of the scene (MAT3 zoidpoly.c, ZOID_RemovePoly()). */
#include "game/game.h"

VIEW(s32, PolyInstanceList);
#define PolyInstanceList PolyInstanceList_view

/* A flat polygon (MAT3 tPoly), 88 bytes: 0x2C halfwords. */
typedef struct tPoly { s16 nverts; u8 pad[0x56]; } tPoly;

/* Free the polygon `poly` points at (its first halfword is nverts: 0 frees it). If it was the last one in use,
 * gNumPolyInsts drops back past it and any free ones before it. */
void ZOID_RemovePoly(s16 *poly) {
  *poly = 0;
  if (poly + 0x2C == (s16 *) (((tPoly *)&PolyInstanceList) + gNumPolyInsts)) {
    do {
      --gNumPolyInsts;
      poly -= 0x2C;
      if (*poly) break;
    } while (gNumPolyInsts > 0);
  }
}
