/* FindDronePathIntersect: where a drone path crosses a checkpoint (MAT3 pathcommon.c). */
#include "game/game.h"

/* Returns the index of the point of drone path `path` at which the path crosses checkpoint `cp`: the first point,
 * walking the path once round from point 0, that is on the other side of the gate's plane from the point before,
 * both within the gate's radius (in x and z). If the path never crosses the gate, the point nearest its centre. */
s16 FindDronePathIntersect(s16 cp, s16 path)
{
    f32 minDist;
    f32 prevDist;
    f32 delta[2];
    f32 normal[2];
    f32 dist;
    f32 dot;
    s16 prev;
    s16 prevSide;
    s16 closest;
    s16 i;
    s16 side;

    normal[0] = gCheckpoints.points[cp].normal[0];
    normal[1] = gCheckpoints.points[cp].normal[2];
    minDist = 1e20f;
    /* i the point, prev the one before it (-1 on the first pass: no pair yet); the walk ends on point 0 again */
    for (prev = -1, i = 0; prev < gDronePaths[path].numPts; i++, prev++) {
        if (i == gDronePaths[path].numPts) {
            i = 0;
        }
        delta[0] = gDronePaths[path].pts[i].pos[0] - gCheckpoints.points[cp].pos[0];
        delta[1] = gDronePaths[path].pts[i].pos[2] - gCheckpoints.points[cp].pos[2];
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
            side != (s32) prevSide) {
            break;
        }
        prevDist = dist;
        prevSide = side;
    }
    if (prev == gDronePaths[path].numPts) {
        return closest;
    }
    return i;
}
