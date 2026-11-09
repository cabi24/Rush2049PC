/* CalcResPathBounds: the bounding box of the track's resurrection path, which the HUD's track map scales to (MAT3
 * pathcommon.c). */
#include "game/game.h"

/* The bounds as arrays, indexed by axis (include/game declares them as WorldPoint, x y z). */
VIEW(s16[3], gPathMin);
#define gPathMin gPathMin_view
VIEW(s16[3], gPathMax);
#define gPathMax gPathMax_view

/* Sets gPathMin and gPathMax to the smallest and largest coordinates of the resurrection path's points: every point
 * of the main path, and of the alternative paths only those of a path cars may be put back on (ALT_PATH_RESURRECT).
 * The points of all the paths lie one after the other in gResPathPtList (ReadPathData), the main path's first. */
void CalcResPathBounds(void)
{
    int i, j, validPt;

    gPathMin[0] = gPathMin[1] = gPathMin[2] = 0x7FFF;
    gPathMax[0] = gPathMax[1] = gPathMax[2] = -0x7FFF;
    for (i = 0; i < gNumResPathPts; i++) {
        if (i < gResPath.numPts) {
            validPt = 1;
        } else {
            /* a point of an alternative path: find the path it belongs to */
            validPt = 0;
            for (j = 0; j < gResPath.numAlt; j++) {
                if (&gResPathPtList[i] >= gResPath.alt[j].pts &&
                    &gResPathPtList[i] < gResPath.alt[j].pts + gResPath.alt[j].numPts &&
                    gResPath.alt[j].resurrect == ALT_PATH_RESURRECT) {
                    validPt = 1;
                }
            }
        }
        if (validPt == 1) {
            for (j = 0; j < 3; j++) {
                gPathMin[j] = (gPathMin[j] < gResPathPtList[i][j]) ? gPathMin[j] : gResPathPtList[i][j];
                gPathMax[j] = (gPathMax[j] > gResPathPtList[i][j]) ? gPathMax[j] : gResPathPtList[i][j];
            }
        }
    }
}
