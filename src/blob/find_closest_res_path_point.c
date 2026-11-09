/* find_closest_res_path_point: the point of the resurrection path nearest a position. Nothing calls it; MAT3 has no
 * function of this shape (FindClosestPathPt is its drone path counterpart). */
#include "game/game.h"

/* Returns the index of the point of the main resurrection path (gResPath) nearest to `pos` (x, y, z), counting only
 * points nearer than res_path_search_start_dist (a squared distance); the result is unset if none is. */
s16 find_closest_res_path_point(s16 *pos) {
    float best = res_path_search_start_dist;
    s32 pad[5];
    s16 closest;
    s32 i;
    for (i = 0; i < gResPath.numPts; i++) {
        float dx = gResPath.pts[i][0] - pos[0];
        float dy = gResPath.pts[i][1] - pos[1];
        float dz = gResPath.pts[i][2] - pos[2];
        float d = dx * dx + dy * dy + dz * dz;
        if (d < best) {
            best = d;
            closest = i;
        }
    }
    return closest;
}
