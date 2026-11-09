/* TracePathPtForward: the point some way further along a path (MAT3 maxpath.c). */
#include "game/game.h"

/* Steps `numPts` points on from point `inIdx` of a path and sets *outPath and *outIdx to where that lands.
 * `res` 0: drone path `inPath`; else the resurrection path: the main one when `inPath` is negative, else
 * alternative path `inPath`, whose end continues on the path it joins. Past the end of the main path or a drone
 * path the count goes on round the lap, from the point where the path crosses the lap's first checkpoint
 * (gCheckpoints.loopPt). */
void TracePathPtForward(int res, int inPath, int inIdx, int numPts, int *outPath, int *outIdx) {
    if (res == 0 || inPath < 0) {
        *outPath = inPath;
        *outIdx = inIdx + numPts;
        if (res) {
            while (*outIdx >= gResPath.numPts)
                *outIdx = *outIdx + gCheckpoints.points[gCheckpoints.loopPt].resPathPt - gResPath.numPts;
        } else {
            while (*outIdx >= gDronePaths[inPath].numPts)
                *outIdx = gCheckpoints.points[gCheckpoints.loopPt].dronePathPt[inPath] + *outIdx
                    - gDronePaths[inPath].numPts;
        }
    } else if (inIdx + numPts < gResPath.alt[inPath].numPts) {
        *outPath = inPath;
        *outIdx = inIdx + numPts;
    } else {
        /* off the branch's end: go on along the path it joins */
        TracePathPtForward(res, gResPath.alt[inPath].lastPath, gResPath.alt[inPath].lastIdx,
                           numPts - gResPath.alt[inPath].numPts + inIdx, outPath, outIdx);
    }
}
