/* FindResPathIntersect: where the resurrection path crosses a checkpoint (MAT3 pathcommon.c). */
#include "game/game.h"

/* Returns the index of the point of the main resurrection path (gResPath) at which it crosses checkpoint `cp`: the
 * first point, walking the path once round from point 0, on the other side of the gate's plane from the point
 * before, both within the gate's radius (in x and z). If the path never crosses the gate, the point nearest its
 * centre. FindDronePathIntersect does the same for a drone path. */
s16 FindResPathIntersect(s16 cp) {
    f32 unused0;
    f32 prevDist;
    f32 delta[2];
    f32 normal[2];
    f32 minDist;
    f32 dist;
    s16 unused1;
    s16 prevSide;
    s16 closest;
    s16 side;
    s16 prev;
    s16 i;

    normal[0] = gCheckpoints.points[cp].normal[0];
    normal[1] = gCheckpoints.points[cp].normal[2];
    minDist = 1e20f;
    /* i the point, prev the one before it (-1 on the first pass: no pair yet); the walk ends on point 0 again */
    for (prev = -1, i = 0; prev < gResPath.numPts; i++, prev++) {
        if (i == gResPath.numPts) {
            i = 0;
        }
        delta[0] = gResPath.pts[i][0] - gCheckpoints.points[cp].pos[0];
        delta[1] = gResPath.pts[i][2] - gCheckpoints.points[cp].pos[2];
        dist = delta[0] * delta[0] + delta[1] * delta[1];
        if (delta[0] * normal[0] + delta[1] * normal[1] < 0.0f) {
            side = -1;
        } else {
            side = 1;
        }
        if (dist < minDist) {
            minDist = dist;
            closest = i;
        }
        if (prev >= 0 && dist <= gCheckpoints.points[cp].radius && prevDist <= gCheckpoints.points[cp].radius &&
            side != prevSide) {
            break;
        }
        prevDist = dist;
        prevSide = side;
    }
    if (prev == gResPath.numPts) {
        return closest;
    }
    return i;
}
