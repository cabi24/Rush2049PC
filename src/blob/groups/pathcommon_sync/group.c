#include "game/game.h"

typedef struct {
    s16 x;
    s16 y;
    s16 z;
} RoutePoint;

typedef struct {
    u8 type;
    char pad1[9];
    u16 num;
    RoutePoint *pts;
} Route;

/* PC: gResPath and gDronePaths are native structs and the alternative paths a native array (ReadPathData), so these views take host offsets and strides */
typedef struct {
    char pad0[16];
    u8 count;
    char pad9[3];
    Route *routes;
} RouteSet;

typedef struct {
    s16 unk0;
    s16 first;
    s16 last;
    s16 unk6;
    s16 count;
    s16 unkA;
    f32 x;
    f32 y;
    f32 z;
    f32 dx;
    char pad1C[4];
    f32 dz;
    s32 range;
    char pad28[6];
    s16 idx[17];
} Tracker;

typedef struct {
    char pad0[0x58];
    f32 len;
} TrackerLen;

VIEW(RouteSet, gResPath);
#define gResPath gResPath_view
VIEW(Tracker[], gCheckpoints);
#define gCheckpoints gCheckpoints_view

s32 FindAltPathIntersect(s16 who, s16 route) {
    s16 i;
    s16 prev;
    s16 side;
    s16 first;
    f32 d[2];
    f32 dir[2];
    Route *r;

    first = 1;
    if (route >= gResPath.count) {
        return -1;
    }
    r = (Route *)((u8 *)gResPath.routes + route * sizeof(Route));
    if (r->type == 2) {
        if (who == 0) {
            return 0;
        }
        return -1;
    }
    dir[0] = gCheckpoints[who].dx;
    dir[1] = gCheckpoints[who].dz;
    for (i = 0; i < r->num; i++) {
        d[0] = r->pts[i].x - gCheckpoints[who].x;
        d[1] = r->pts[i].z - gCheckpoints[who].z;
        if (d[0] * d[0] + d[1] * d[1] <= gCheckpoints[who].range) {
            if (d[0] * dir[0] + d[1] * dir[1] < 0.0f) {
                side = -1;
            } else {
                side = 1;
            }
            if (first) {
                prev = side;
                first = 0;
            } else if (prev != side) {
                return i;
            }
        }
    }
    return -1;
}

#define LEN(n) (((TrackerLen *)&gCheckpoints[n])->len)

static void FindPathDistances(void);

void SyncCheckpointsToPaths(s16 mode) {
    s32 unused1;
    s32 unused2;
    s16 pos[3];
    s32 i;
    s32 j;

    for (i = 0; i < gCheckpoints[0].count; i++) {
        pos[0] = gCheckpoints[i].x;
        pos[1] = gCheckpoints[i].y;
        pos[2] = gCheckpoints[i].z;
        if (mode < 0) {
            for (j = 0; j < 20; j++) {
                if (j == 0) {
                    gCheckpoints[i].idx[j] = FindResPathIntersect(i);
                } else if (j >= 5) {
                    gCheckpoints[i].idx[j] = FindAltPathIntersect(i, j - 5);
                } else {
                    gCheckpoints[i].idx[j] = FindDronePathIntersect(i, j - 1);
                }
            }
        } else if (mode == 0) {
            gCheckpoints[i].idx[mode] = FindResPathIntersect(i);
            for (j = 0; j < gResPath.count; j++) {
                gCheckpoints[i].idx[j + 5] = FindAltPathIntersect(i, j);
            }
        } else {
            gCheckpoints[i].idx[mode] = FindDronePathIntersect(i, mode - 1);
        }
    }
    FindPathDistances();
}

static unsigned int route_next_checkpoint_u(unsigned int cp) {
    if (cp == gCheckpoints[0].count - 1) {
        return gCheckpoints[0].first;
    }
    return cp + 1U;
}

static void FindPathDistances(void) {
    s32 k;
    s32 next;
    s32 done;

    gFirstLapDistance = 0.0f;
    gLapDistance = gFirstLapDistance;
    done = 0;
    for (k = 0; k < gCheckpoints[0].count; k++) {
        next = route_next_checkpoint_u(k);
        LEN(k) = GetPathDistance(gCheckpoints[k].idx[0], gCheckpoints[next].idx[0]);
        if (k > 0 && k == gCheckpoints[0].last) {
            done = 1;
        }
        if (!done) {
            gFirstLapDistance += LEN(k);
        }
        if (k >= gCheckpoints[0].first) {
            gLapDistance += LEN(k);
        }
    }
}
