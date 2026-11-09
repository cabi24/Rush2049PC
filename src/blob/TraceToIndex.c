/* TraceToIndex: the point some way back along the resurrection path (MAT3 maxpath.c). */
#include "game/game.h"

/* Steps `toIdx` points back from point `inIdx` of the resurrection path (the main one when `inPath` is negative,
 * else alternative path `inPath`, whose start continues back on the path it leaves) and sets *outPath and *outIdx
 * to where that lands. On the main path, a point in the lap's loop (from the lap's first checkpoint,
 * gCheckpoints.loopPt, on) steps back round the loop; one in the run-up from the grid stops at point 0. */
void TraceToIndex(int inPath, int inIdx, int toIdx, int *outPath, int *outIdx) {
    if (inPath < 0) {
        *outPath = inPath;
        *outIdx = inIdx - toIdx;
        if (inIdx < gCheckpoints.points[gCheckpoints.loopPt].resPathPt) {
            if (*outIdx < 0)
                *outIdx = 0;
        } else {
            while (*outIdx < gCheckpoints.points[gCheckpoints.loopPt].resPathPt)
                *outIdx = *outIdx + gResPath.numPts - gCheckpoints.points[gCheckpoints.loopPt].resPathPt;
        }
    } else if (inIdx >= toIdx) {
        *outPath = inPath;
        *outIdx = inIdx - toIdx;
    } else {
        /* before the branch's start: go on back along the path it leaves */
        TraceToIndex(gResPath.alt[inPath].firstPath, gResPath.alt[inPath].firstIdx, toIdx - inIdx, outPath, outIdx);
    }
}
