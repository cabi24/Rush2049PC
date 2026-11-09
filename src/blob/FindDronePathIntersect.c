#include "game/game.h"

typedef struct PathPoint {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
    /* 0x6 */ s16 flag;
} PathPoint;

typedef struct PathDesc {
    /* 0x0 */ u16 count;
    /* 0x2 */ u16 unk2;
    /* 0x4 */ PathPoint *points;
} PathDesc;

typedef struct Gate {
    /* 0x00 */ u8 pad0[12];
    /* 0x0C */ f32 pos[3];
    /* 0x18 */ f32 dir[3];
    /* 0x24 */ s32 radius;
    /* 0x28 */ u8 pad28[40];
} Gate; /* 0x50 */

VIEW(Gate[], gCheckpoints);
#define gCheckpoints gCheckpoints_view
VIEW(PathDesc[], gDronePaths);
#define gDronePaths gDronePaths_view

s16 FindDronePathIntersect(s16 gate, s16 path)
{
    f32 best;
    f32 lastDist;
    f32 delta[2];
    f32 dir[2];
    f32 dist;
    f32 dot;
    s16 n;
    s16 lastSign;
    s16 closest;
    s16 i;
    s16 sign;

    dir[0] = gCheckpoints[gate].dir[0];
    dir[1] = gCheckpoints[gate].dir[2];
    best = 1e20f;
    for (n = -1, i = 0; n < gDronePaths[path].count; i++, n++) {
        if (i == gDronePaths[path].count) {
            i = 0;
        }
        delta[0] = gDronePaths[path].points[i].x - gCheckpoints[gate].pos[0];
        delta[1] = gDronePaths[path].points[i].z - gCheckpoints[gate].pos[2];
        dist = delta[0] * delta[0] + delta[1] * delta[1];
        if (delta[0] * dir[0] + delta[1] * dir[1] < 0.0f) {
            sign = -1;
        } else {
            sign = 1;
        }
        if (dist < best) {
            best = dist;
            closest = i;
        }
        if (n >= 0 && dist <= gCheckpoints[gate].radius && lastDist <= gCheckpoints[gate].radius && sign != (s32) lastSign) {
            break;
        }
        lastDist = dist;
        lastSign = sign;
    }
    if (n == gDronePaths[path].count) {
        return closest;
    }
    return i;
}
