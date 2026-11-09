/* The resurrection path's branches at a track's load: how the alternative paths join each other and the main path,
 * and which checkpoint section each starts in (MAT3 pathcommon.c: TraceCheck, FindClosestPt, SyncAltPaths,
 * TraceToCheckpoint; TraceToEnd is maxpath.c's). */
#include "game/game.h"

/* Records path `inPath` (an alternative path, -1 the main path) as step `depth` of a walk along the paths
 * (gPathTrace) and returns 1 to go on, or 0 if the walk has been on it already (a loop) or it is a branch cars may
 * be put back on that joins a branch they may not (the walk can't pass between such branches). */
s32 TraceCheck(s32 inPath, s32 depth)
{
    s32 i;

    if (inPath >= 0 && gResPath.alt[inPath].resurrect != 0) {
        if ((gResPath.alt[inPath].firstPath >= 0 &&
             gResPath.alt[gResPath.alt[inPath].firstPath].resurrect == 0) ||
            (gResPath.alt[inPath].lastPath >= 0 &&
             gResPath.alt[gResPath.alt[inPath].lastPath].resurrect == 0)) {
            return 0;
        }
    }
    gPathTrace[depth] = inPath;
    for (i = 0; i < depth; i++) {
        if (inPath == gPathTrace[i]) {
            return 0;
        }
    }
    return 1;
}

/* Finds the point of the resurrection paths nearest `pos` (x, y, z) other than on alternative path `alt` itself:
 * sets *outPath to its path (-1 the main path, else an alternative path) and *outIdx to its index. `first` says
 * which end of `alt` pos is (1 its first point, 0 its last); an earlier branch already joined to that same end
 * isn't a candidate, so two branches don't join each other end to end. Only points nearer than
 * path_closest_start_distance (a squared distance) count; with none, *outIdx is left as it was. */
void FindClosestPt(s32 alt, s32 first, s32 *outPath, s16 *pos, s32 *outIdx)
{
    f32 min;
    f32 dx, dy, dz, dist;
    s32 p, a;

    min = path_closest_start_distance;
    *outPath = -1;
    for (p = 0; p < gResPath.numPts; p++) {
        dx = gResPath.pts[p][0] - pos[0];
        dy = gResPath.pts[p][1] - pos[1];
        dz = gResPath.pts[p][2] - pos[2];
        dist = dx * dx + dy * dy + dz * dz;
        if (dist < min) {
            min = dist;
            *outIdx = p;
        }
    }
    for (a = 0; a < gResPath.numAlt; a++) {
        if (a == alt) {
            continue;
        }
        if (first && a < alt && gResPath.alt[a].firstPath == alt &&
            gResPath.alt[a].firstIdx == 0) {
            continue;
        }
        if (!first && a < alt && gResPath.alt[a].lastPath == alt &&
            gResPath.alt[a].lastIdx + 1 == gResPath.alt[alt].numPts) {
            continue;
        }
        for (p = 0; p < gResPath.alt[a].numPts; p++) {
            dx = gResPath.alt[a].pts[p][0] - pos[0];
            dy = gResPath.alt[a].pts[p][1] - pos[1];
            dz = gResPath.alt[a].pts[p][2] - pos[2];
            dist = dx * dx + dy * dy + dz * dz;
            if (dist < min) {
                *outIdx = p;
                *outPath = a;
                min = dist;
            }
        }
    }
}

/* Follows the resurrection paths forward from point `inIdx` of path `inPath` (-1 the main path), from each branch's
 * end to the point it joins, until the main path, or with `honorAltRes` a branch cars may be put back on. Sets
 * *outPath and *outIdx to where it stops and *numPts to the points walked to get there. Returns 0 if the walk
 * loops (TraceCheck), with step `depth` of it this call. */
s32 TraceToEnd(s32 inPath, s32 inIdx, s32 *outPath, s32 *outIdx, s32 *numPts, s32 honorAltRes, s32 depth)
{
    if (!TraceCheck(inPath, depth)) {
        return 0;
    }
    if (depth == 0) {
        *numPts = 0;
    }
    if (inPath < 0 || (honorAltRes && gResPath.alt[inPath].resurrect != 0)) {
        *outPath = inPath;
        *outIdx = inIdx;
        return 1;
    }
    *numPts += gResPath.alt[inPath].numPts - inIdx;
    return TraceToEnd(gResPath.alt[inPath].lastPath, gResPath.alt[inPath].lastIdx,
                      outPath, outIdx, numPts, honorAltRes, depth + 1);
}

/* Joins the alternative paths up: each branch's first and last points to the nearest point of another path
 * (FindClosestPt), except a branch joined to nothing (ALT_PATH_ALONE); then gives each branch the checkpoint
 * section it starts in (TraceToCheckpoint), -1 if none. */
void SyncAltPaths(void)
{
    s32 i;
    s32 cp;
    s32 path;
    s32 idx;

    for (i = 0; i < gResPath.numAlt; i++) {
        if (gResPath.alt[i].resurrect == ALT_PATH_ALONE) {
            gResPath.alt[i].firstPath = -1;
            gResPath.alt[i].firstIdx = 0;
            gResPath.alt[i].lastPath = -1;
            gResPath.alt[i].lastIdx = 1;
        } else {
            FindClosestPt(i, 1, &path, gResPath.alt[i].pts[0], &idx);
            gResPath.alt[i].firstPath = path;
            gResPath.alt[i].firstIdx = idx;
            /* the last point, written index first as the N64 code computes it */
            FindClosestPt(i, 0, &path, *(gResPath.alt[i].numPts - 1 + gResPath.alt[i].pts), &idx);
            gResPath.alt[i].lastPath = path;
            gResPath.alt[i].lastIdx = idx;
        }
    }
    for (i = 0; i < gResPath.numAlt; i++) {
        if (gResPath.alt[i].resurrect == ALT_PATH_ALONE) {
            gResPath.alt[i].startSection = -1;
        } else if (!TraceToCheckpoint(i, 0, &cp, 0)) {
            gResPath.alt[i].startSection = -1;
        } else {
            gResPath.alt[i].startSection = cp;
        }
    }
}

/* Finds the checkpoint section that point `inIdx` of path `inPath` (-1 the main path) lies in: the last
 * checkpoint the path crosses at or before the point, following a branch back from its start to the path it
 * leaves if the branch crosses none before the point. Sets *cp to it and returns 1, or 0 if the walk loops
 * (TraceCheck), with step `depth` of it this call. */
s32 TraceToCheckpoint(s32 inPath, s32 inIdx, s32 *cp, s32 depth)
{
    s32 i;
    s16 idx;

    if (!TraceCheck(inPath, depth)) {
        return 0;
    }
    if (inPath < 0) {
        for (i = 1; i < gCheckpoints.numPoints; i++) {
            if (inIdx < gCheckpoints.points[i].resPathPt) {
                break;
            }
        }
        *cp = i - 1;
        return 1;
    }
    idx = -1;
    for (i = 0; i < gCheckpoints.numPoints; i++) {
        if (gCheckpoints.points[i].altPathPt[inPath] >= 0 &&
            (idx < 0 || idx < gCheckpoints.points[i].altPathPt[inPath]) &&
            inIdx >= gCheckpoints.points[i].altPathPt[inPath]) {
            *cp = i;
            idx = gCheckpoints.points[i].altPathPt[inPath];
        }
    }
    if (idx >= 0) {
        return 1;
    }
    return TraceToCheckpoint(gResPath.alt[inPath].firstPath, gResPath.alt[inPath].firstIdx,
                             cp, depth + 1);
}
