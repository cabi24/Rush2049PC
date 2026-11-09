/* NextPathPt: the next point along a drone path (MAT3 pathcommon.c). */
#include "game/game.h"

/* Returns the point after point `idx` of drone path `path`. After the path's last point comes the point where it
 * crosses the checkpoint a lap starts from (gCheckpoints.loopPt), so the path loops without its run-up from the
 * starting grid. */
s16 NextPathPt(s16 idx, s16 path)
{
    if (idx + 1 == gDronePaths[path].numPts) {
        return gCheckpoints.points[gCheckpoints.loopPt].dronePathPt[path];
    }
    return idx + 1;
}
