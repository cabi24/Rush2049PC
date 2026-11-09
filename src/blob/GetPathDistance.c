/* GetPathDistance: the length of a stretch of the resurrection path (MAT3 pathcommon.c). */
#include "game/game.h"

extern f32 sqrtf(f32);

/* Returns the length, in feet and on the ground (x and z only), of the main resurrection path from point `from` to
 * point `to`. When `to` comes before `from`, the stretch runs on past the path's last point and round from the
 * point where a lap starts (the resurrection path point of checkpoint gCheckpoints.loopPt); a `to` before that point
 * can't be reached so, and gives -1. */
f32 GetPathDistance(s32 from, s32 to) {
    s32 i;
    s32 next;
    s32 end;
    f32 dist;
    f32 d[3];

    if (to < from && to < gCheckpoints.points[gCheckpoints.loopPt].resPathPt) {
        return -1.0f;
    }
    if (to < from) {
        end = gResPath.numPts;
    } else {
        end = to;
    }
    dist = 0.0f;
    for (i = from; i < end; i++) {
        NextResPathPt(-1, i, 0, &next);
        d[0] = gResPath.pts[next][0] - gResPath.pts[i][0];
        d[1] = gResPath.pts[next][1] - gResPath.pts[i][1];
        d[2] = gResPath.pts[next][2] - gResPath.pts[i][2];
        d[1] = 0.0f;
        dist += sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    }
    if (end != to) {
        /* the rest of the stretch, from where the lap starts again */
        for (i = gCheckpoints.points[gCheckpoints.loopPt].resPathPt; i < to; i++) {
            NextResPathPt(-1, i, 0, &next);
            d[0] = gResPath.pts[next][0] - gResPath.pts[i][0];
            d[1] = gResPath.pts[next][1] - gResPath.pts[i][1];
            d[2] = gResPath.pts[next][2] - gResPath.pts[i][2];
            d[1] = 0.0f;
            dist += sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        }
    }
    return dist;
}
