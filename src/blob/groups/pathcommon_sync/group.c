/* Where the paths cross the checkpoints, and the lap's length (MAT3 pathcommon.c: FindAltPathIntersect,
 * SyncCheckpointsToPaths, FindPathDistances). */
#include "game/game.h"

/* Returns the index of the point of alternative path `path` at which it crosses checkpoint `cp`: the first point,
 * of those within the gate's radius (in x and z), on the other side of the gate's plane from the first of them; -1
 * if it doesn't cross (or there is no such path). A branch joined to nothing (ALT_PATH_ALONE) counts as crossing
 * checkpoint 0 at its first point and no other. */
s32 FindAltPathIntersect(s16 cp, s16 path) {
    s16 i;
    s16 sign;
    s16 newSign;
    s16 first;
    f32 d[2];
    f32 normal[2];
    AltPath *alt;

    first = 1;
    if (path >= gResPath.numAlt) {
        return -1;
    }
    alt = &gResPath.alt[path];
    if (alt->resurrect == ALT_PATH_ALONE) {
        if (cp == 0) {
            return 0;
        }
        return -1;
    }
    normal[0] = gCheckpoints.points[cp].normal[0];
    normal[1] = gCheckpoints.points[cp].normal[2];
    for (i = 0; i < alt->numPts; i++) {
        d[0] = alt->pts[i][0] - gCheckpoints.points[cp].pos[0];
        d[1] = alt->pts[i][2] - gCheckpoints.points[cp].pos[2];
        if (d[0] * d[0] + d[1] * d[1] <= gCheckpoints.points[cp].radius) {
            if (d[0] * normal[0] + d[1] * normal[1] < 0.0f) {
                newSign = -1;
            } else {
                newSign = 1;
            }
            if (first) {
                sign = newSign;
                first = 0;
            } else if (sign != newSign) {
                return i;
            }
        }
    }
    return -1;
}

static void FindPathDistances(void);

/* Finds where the paths cross every checkpoint (tCheckpoint.pathPts), then the stretches' and the laps' lengths.
 * `path` -1: every path; 0: the resurrection path and its branches; 1 to 4: drone path `path` - 1. */
void SyncCheckpointsToPaths(s16 path) {
    s32 unused1;
    s32 unused2;
    s16 pos[3];
    s32 i;
    s32 p;

    for (i = 0; i < gCheckpoints.numPoints; i++) {
        pos[0] = gCheckpoints.points[i].pos[0];
        pos[1] = gCheckpoints.points[i].pos[1];
        pos[2] = gCheckpoints.points[i].pos[2];
        if (path < 0) {
            for (p = 0; p < 20; p++) {
                if (p == 0) {
                    gCheckpoints.points[i].pathPts[p] = FindResPathIntersect(i);
                } else if (p >= 5) {
                    gCheckpoints.points[i].pathPts[p] = FindAltPathIntersect(i, p - 5);
                } else {
                    gCheckpoints.points[i].pathPts[p] = FindDronePathIntersect(i, p - 1);
                }
            }
        } else if (path == 0) {
            gCheckpoints.points[i].pathPts[path] = FindResPathIntersect(i);
            for (p = 0; p < gResPath.numAlt; p++) {
                gCheckpoints.points[i].pathPts[p + 5] = FindAltPathIntersect(i, p);
            }
        } else {
            gCheckpoints.points[i].pathPts[path] = FindDronePathIntersect(i, path - 1);
        }
    }
    FindPathDistances();
}

/* GetNextCheckpoint, unsigned (the N64 code has its own copy here). */
static unsigned int next_checkpoint(unsigned int cp) {
    if (cp == gCheckpoints.numPoints - 1) {
        return gCheckpoints.loopPt;
    }
    return cp + 1U;
}

/* Measures the resurrection path from each checkpoint to the next (tCheckpoint.distToNext), and from them the
 * first lap's length (gFirstLapDistance: the stretches from checkpoint 0 to the finish) and a later lap's
 * (gLapDistance: from the lap's first checkpoint on). */
static void FindPathDistances(void) {
    s32 i;
    s32 next;
    s32 finishReached;

    gFirstLapDistance = 0.0f;
    gLapDistance = gFirstLapDistance;
    finishReached = 0;
    for (i = 0; i < gCheckpoints.numPoints; i++) {
        next = next_checkpoint(i);
        gCheckpoints.points[i].distToNext = GetPathDistance(gCheckpoints.points[i].resPathPt,
                                                            gCheckpoints.points[next].resPathPt);
        if (i > 0 && i == gCheckpoints.finishPt) {
            finishReached = 1;
        }
        if (!finishReached) {
            gFirstLapDistance += gCheckpoints.points[i].distToNext;
        }
        if (i >= gCheckpoints.loopPt) {
            gLapDistance += gCheckpoints.points[i].distToNext;
        }
    }
}
