/* FindClosestPathPt: the point of a drone path nearest a position (MAT3 maxpath.c). */
#include "game/game.h"

/* Returns the index of the point of drone path `path` (gDronePaths) nearest to `pos` (x, y, z), by straight-line
 * distance. The respawn uses it to put a drone back on its path (resurrect.c). The result is unset if the path has
 * no points. */
s16 FindClosestPathPt(s16 *pos, s16 path)
{
    DronePathPt *pt;
    s32 i;
    s32 pad0[4];
    s16 closest;
    f32 dx, dy, dz, dist;
    f32 min;

    min = 1e20f;
    i = 0;
    if (gDronePaths[path].numPts > 0) {
        pt = gDronePaths[path].pts;
        do {
            dx = pt->pos[0] - pos[0];
            dy = pt->pos[1] - pos[1];
            dz = pt->pos[2] - pos[2];
            dist = dx * dx + dy * dy + dz * dz;
            if (dist < min) {
                min = dist;
                closest = i;
            }
            i++;
            pt++;
        } while (i < gDronePaths[path].numPts);
    }
    return closest;
}
