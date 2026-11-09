#include "game/game.h"

typedef struct PathPoint {
    s16 pos[3];
} PathPoint;

typedef struct {
    u16 count;
    PathPoint *points;
} PathGraph;

typedef struct {
    s16 unk0;
    s16 unk2;
    char pad4[0x2A];
    s16 unk2E;
    char pad30[0x20];
} Track; /* 0x50 */

VIEW(PathGraph, gResPath);
#define gResPath gResPath_view
VIEW(Track[], gCheckpoints);
#define gCheckpoints gCheckpoints_view

extern f32 sqrtf(f32);
#pragma intrinsic(sqrtf)

f32 GetPathDistance(s32 from, s32 to) {
    s32 i;
    s32 next;
    s32 end;
    f32 dist;
    f32 d[3];

    if (to < from && to < gCheckpoints[gCheckpoints[0].unk2].unk2E) {
        return -1.0f;
    }
    if (to < from) {
        end = gResPath.count;
    } else {
        end = to;
    }
    dist = 0.0f;
    for (i = from; i < end; i++) {
        NextResPathPt(-1, i, 0, &next);
        d[0] = gResPath.points[next].pos[0] - gResPath.points[i].pos[0];
        d[1] = gResPath.points[next].pos[1] - gResPath.points[i].pos[1];
        d[2] = gResPath.points[next].pos[2] - gResPath.points[i].pos[2];
        d[1] = 0.0f;
        dist += sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    }
    if (end != to) {
        for (i = gCheckpoints[gCheckpoints[0].unk2].unk2E; i < to; i++) {
            NextResPathPt(-1, i, 0, &next);
            d[0] = gResPath.points[next].pos[0] - gResPath.points[i].pos[0];
            d[1] = gResPath.points[next].pos[1] - gResPath.points[i].pos[1];
            d[2] = gResPath.points[next].pos[2] - gResPath.points[i].pos[2];
            d[1] = 0.0f;
            dist += sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        }
    }
    return dist;
}
