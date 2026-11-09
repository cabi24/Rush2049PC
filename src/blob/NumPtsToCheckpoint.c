/* NumPtsToCheckpoint: how many path points lie between a point and a checkpoint (MAT3 maxpath.c). */
#include "game/game.h"

/* Returns the number of points from point `inIdx` of a path on to where the path crosses checkpoint `cp`.
 * `res` 0: the path is drone path `inPath`; else the resurrection path: the main one when `inPath` is negative,
 * else alternative path `inPath`, followed on through the paths it joins until one crosses the checkpoint. Past a
 * path's end the count goes on round the lap: from the point where the path crosses the lap's first checkpoint
 * (gCheckpoints.loopPt). */
int NumPtsToCheckpoint(int res, int inPath, int inIdx, int cp) {
    if (res == 0) {
        if (gCheckpoints.points[cp].dronePathPt[inPath] < inIdx) {
            return gDronePaths[inPath].numPts - inIdx + gCheckpoints.points[cp].dronePathPt[inPath]
                - ((cp >= gCheckpoints.loopPt) ? gCheckpoints.points[gCheckpoints.loopPt].dronePathPt[inPath] : 0);
        }
        return gCheckpoints.points[cp].dronePathPt[inPath] - inIdx;
    }
    if (inPath < 0) {
        if (gCheckpoints.points[cp].resPathPt < inIdx) {
            return gResPath.numPts - inIdx + gCheckpoints.points[cp].resPathPt
                - ((cp >= gCheckpoints.loopPt) ? gCheckpoints.points[gCheckpoints.loopPt].resPathPt : 0);
        }
        return gCheckpoints.points[cp].resPathPt - inIdx;
    }
    if (gCheckpoints.points[cp].altPathPt[inPath] >= 0 && gCheckpoints.points[cp].altPathPt[inPath] >= inIdx)
        return gCheckpoints.points[cp].altPathPt[inPath] - inIdx;
    /* the branch doesn't cross it: the rest of the branch, then on from where it joins */
    return gResPath.alt[inPath].numPts
        + NumPtsToCheckpoint(res, gResPath.alt[inPath].lastPath, gResPath.alt[inPath].lastIdx, cp)
        - inIdx;
}
