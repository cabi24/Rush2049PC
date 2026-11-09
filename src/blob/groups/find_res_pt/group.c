/* TraceToMain: follows a branch of the resurrection path back to the main path (MAT3 maxpath.c). */
#include "game/game.h"

/* Follows the resurrection paths back from point `inIdx` of path `inPath` (-1 the main path), from each branch's
 * start to the point of the path it leaves, until the main path. Sets *outIdx to the main path's point and *numPts
 * to the points walked back to get there. Returns 0 if the walk loops (TraceCheck), with step `depth` of it this
 * call. */
s32 TraceToMain(s32 inPath, s32 inIdx, s32 *outIdx, s32 *numPts, s32 depth)
{
    if (!TraceCheck(inPath, depth)) {
        return 0;
    }
    if (depth == 0) {
        *numPts = 0;
    }
    if (inPath < 0) {
        *outIdx = inIdx;
        return 1;
    }
    *numPts += inIdx;
    return TraceToMain(gResPath.alt[inPath].firstPath, gResPath.alt[inPath].firstIdx,
                       outIdx, numPts, depth + 1);
}
