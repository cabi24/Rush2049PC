/* ZOID_Reset.c: empty the scene (MAT3 zoid.c, ZOID_Reset()).
 *
 * Called when a race or a screen starts building its scene: no nodes, cull vertices, blits, matrices or flat
 * polygons, the one full-screen window, the default lights. */
#include "game/game.h"

VIEW(intptr_t, gLights);
#define gLights gLights_view

VIEW(s16, ObjHead);
#define ObjHead ObjHead_view

/* Forget every scene node, cull vertex, blit, matrix and polygon, set up the single full-screen window again
 * (ZOID_ResetWindows) and go back to the default lights. Nothing is freed: the pools are reused from the start. */
void ZOID_Reset(void) {
    gNumObjects = 0;
    cull_vertex_count = 0;
    gNumBlitInsts = 0;
    gNumMatrices = 0;
    gNumPolyInsts = 0;
    ObjHead = -1;
    ZOID_ResetWindows();
    gGlobalCameraFacingScale = 1.0f;
    gNumLights = 0;
    gLights = (intptr_t) &default_lights;
}
