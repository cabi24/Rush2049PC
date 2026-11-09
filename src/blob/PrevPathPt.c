/* PrevPathPt: the point before another along a drone path (MAT3 pathcommon.c). */
#include "game/game.h"

/* Returns the point before point `idx` of drone path `path`: NextPathPt backwards. Before the point where the path
 * crosses the lap's first checkpoint (gCheckpoints.loopPt) comes the path's last point, and before point 0 too,
 * unless the lap starts from a later checkpoint than the first: then point 0 stays where it is. */
s16 PrevPathPt(s16 idx, s16 path)
{
    if (idx == 0 && gCheckpoints.loopPt > 0) {
        return idx;
    }
    if (idx == 0 || idx == gCheckpoints.points[gCheckpoints.loopPt].dronePathPt[path]) {
        return gDronePaths[path].numPts - 1;
    }
    return idx - 1;
}
