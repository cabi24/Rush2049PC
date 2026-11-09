/* NextResPathPt: the next point along the resurrection path or one of its branches (MAT3 pathcommon.c). */
#include "game/game.h"

/* Steps one point on from point `inIdx` of path `inPath` (an alternative path, or -1 the main resurrection path):
 * sets *outPath and *outIdx to the path and point after it. After a branch's last point comes the point it joins
 * (lastPath, lastIdx); after the main path's last point comes the point where it crosses the lap's first checkpoint
 * (gCheckpoints.loopPt). outPath may be NULL when inPath is -1. */
void NextResPathPt(s32 inPath, s32 inIdx, s32 *outPath, s32 *outIdx)
{
    if (inPath >= 0) {
        if (inIdx + 1 == gResPath.alt[inPath].numPts) {
            *outIdx = gResPath.alt[inPath].lastIdx;
            *outPath = gResPath.alt[inPath].lastPath;
            return;
        }
        *outIdx = inIdx + 1;
        *outPath = inPath;
        return;
    }
    if (gResPath.numPts == inIdx + 1) {
        *outIdx = gCheckpoints.points[gCheckpoints.loopPt].resPathPt;
    } else {
        *outIdx = inIdx + 1;
    }
    if (outPath != NULL) {
        *outPath = -1;
    }
}
