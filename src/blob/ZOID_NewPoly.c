/* ZOID_NewPoly.c: add a flat polygon to the scene (MAT3 zoidpoly.c, ZOID_NewPoly()).
 *
 * The flat polygons (PolyInstanceList, 200 of them, MAT3 tPoly) are quads drawn after the scene's models: the cars'
 * shadows and arrows, the skid marks. A free one has no vertices; gNumPolyInsts is one past the last in use. */
#include "game/game.h"

/* A flat polygon (MAT3 tPoly, 88 bytes): what this file sets. */
typedef struct tPoly { s16 nverts; u16 flags, texidx; unsigned char rest[82]; } tPoly;
VIEW(tPoly[], PolyInstanceList);
#define PolyInstanceList PolyInstanceList_view

/* A new polygon of `nverts` vertices (up to 4) at xyz[nverts][3], coloured *color (bytes red, green, blue, alpha;
 * NULL leaves the colour) with `flags` (POLY_*); if `useTidx` it is textured with texture handle `texidx`
 * (POLY_TEXTURED). ZOID_UpdatePoly fills the vertices.
 * Returns the polygon, or NULL when all 200 are in use. */
tPoly *ZOID_NewPoly(int nverts, f32 *xyz, u16 texidx, u8 *color, u16 flags, int useTidx)
{
    int i;
    tPoly *poly;
    for (i = 0; i < gNumPolyInsts; i++) {
        if (!PolyInstanceList[i].nverts) break;
    }
    if (i >= 200) return 0;
    poly = &PolyInstanceList[i];
    if (i >= gNumPolyInsts) gNumPolyInsts = i + 1;
    if (gMaxPolyInsts < gNumPolyInsts) gMaxPolyInsts = gNumPolyInsts;
    poly->flags = flags;
    if (useTidx) {
        poly->flags |= POLY_TEXTURED;
        poly->texidx = texidx;
    }
    ZOID_UpdatePoly(poly, nverts, xyz, texidx, color, 0, useTidx);
    return poly;
}
