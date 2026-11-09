#include "game/game.h"

typedef struct PathPt {
    s16 x;
    s16 y;
    s16 z;
} PathPt;

typedef struct Obj80 {
    char pad00[0x0C];
    f32 x;          /* 0x0C */
    f32 y;          /* 0x10 */
    f32 z;          /* 0x14 */
    f32 dirx;       /* 0x18 */
    f32 pad1C;
    f32 dirz;       /* 0x20 */
    s32 radius;     /* 0x24 */
    char pad28[0x28];
} Obj80;

VIEW(Obj80[], gCheckpoints);
#define gCheckpoints gCheckpoints_view
/* The track's path (FindResPtRecurse.c's PathGraph): its points, and its alternative paths (nodes). */
typedef struct { u16 numPoints; PathPt *points; unsigned char numNodes; void *nodes; } PathGraph;
VIEW(PathGraph, gResPath);
#define gResPath gResPath_view

s16 FindResPathIntersect(s16 arg0) {
    f32 unused0;
    f32 prevDist;
    f32 d[2];
    f32 dir[2];
    f32 minDist;
    f32 dist;
    s16 unused1;
    s16 prevSide;
    s16 best;
    s16 side;
    s16 i;
    s16 j;

    dir[0] = gCheckpoints[arg0].dirx;
    dir[1] = gCheckpoints[arg0].dirz;
    minDist = 1e20f;
    for (i = -1, j = 0; i < gResPath.numPoints; j++, i++) {
        if (j == gResPath.numPoints) {
            j = 0;
        }
        d[0] = gResPath.points[j].x - gCheckpoints[arg0].x;
        d[1] = gResPath.points[j].z - gCheckpoints[arg0].z;
        dist = d[0] * d[0] + d[1] * d[1];
        if (d[0] * dir[0] + d[1] * dir[1] < 0.0f) {
            side = -1;
        } else {
            side = 1;
        }
        if (dist < minDist) {
            minDist = dist;
            best = j;
        }
        if (i >= 0 && dist <= gCheckpoints[arg0].radius && prevDist <= gCheckpoints[arg0].radius && side != prevSide) {
            break;
        }
        prevDist = dist;
        prevSide = side;
    }
    if (i == gResPath.numPoints) {
        return best;
    }
    return j;
}
