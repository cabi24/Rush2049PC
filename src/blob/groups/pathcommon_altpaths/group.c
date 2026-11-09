#include "game/game.h"

typedef struct PathPoint {
    s16 x, y, z;
} PathPoint;

typedef struct {
    /* 0x0 */ u8 type;       /* 2 = standalone (not linked) */
    /* 0x1 */ s8 next;       /* node joined at this node's start */
    /* 0x2 */ u16 nextPos;   /* point index in `next` */
    /* 0x4 */ s8 prev;       /* node joined at this node's end */
    /* 0x6 */ u16 prevPos;   /* point index in `prev` */
    /* 0x8 */ s8 section;
    /* 0xA */ u16 numPoints;
    /* 0xC */ PathPoint *points;
} PathNode;

typedef struct {
    /* 0x0 */ u16 numPoints;
    /* 0x4 */ PathPoint *points;
    /* 0x8 */ u8 numNodes;
    /* 0xC */ PathNode *nodes;
} PathGraph;

typedef struct {
    /* 0x00 */ u8 pad0[8];
    /* 0x08 */ s16 count;     /* valid in element 0 */
    /* 0x0A */ u8 padA[0x2E - 0xA];
    /* 0x2E */ s16 start;
    /* 0x30 */ u8 pad30[8];
    /* 0x38 */ s16 nodeStart[12];
} Section;                    /* 0x50 */

VIEW(PathGraph, gResPath);
#define gResPath gResPath_view

VIEW(Section[], gCheckpoints);
#define gCheckpoints gCheckpoints_view

/* record `node` at `depth` in the walk; 0 if it was already visited or is a dead end */
s32 TraceCheck(s32 node, s32 depth)
{
    s32 i;

    if (node >= 0 && gResPath.nodes[node].type != 0) {
        if ((gResPath.nodes[node].next >= 0 &&
             gResPath.nodes[gResPath.nodes[node].next].type == 0) ||
            (gResPath.nodes[node].prev >= 0 &&
             gResPath.nodes[gResPath.nodes[node].prev].type == 0)) {
            return 0;
        }
    }
    gPathTrace[depth] = node;
    for (i = 0; i < depth; i++) {
        if (node == gPathTrace[i]) {
            return 0;
        }
    }
    return 1;
}

/* nearest point to `pos`: global points first (outNode = -1), then node points */
void FindClosestPt(s32 skip, s32 atStart, s32 *outNode, PathPoint *pos, s32 *outIdx)
{
    f32 best;
    f32 dx, dy, dz, d;
    s32 i, j;

    best = path_closest_start_distance;
    *outNode = -1;
    for (i = 0; i < gResPath.numPoints; i++) {
        dx = gResPath.points[i].x - pos->x;
        dy = gResPath.points[i].y - pos->y;
        dz = gResPath.points[i].z - pos->z;
        d = dx * dx + dy * dy + dz * dz;
        if (d < best) {
            best = d;
            *outIdx = i;
        }
    }
    for (j = 0; j < gResPath.numNodes; j++) {
        if (j == skip) {
            continue;
        }
        if (atStart && j < skip && gResPath.nodes[j].next == skip &&
            gResPath.nodes[j].nextPos == 0) {
            continue;
        }
        if (!atStart && j < skip && gResPath.nodes[j].prev == skip &&
            gResPath.nodes[j].prevPos + 1 == gResPath.nodes[skip].numPoints) {
            continue;
        }
        for (i = 0; i < gResPath.nodes[j].numPoints; i++) {
            dx = gResPath.nodes[j].points[i].x - pos->x;
            dy = gResPath.nodes[j].points[i].y - pos->y;
            dz = gResPath.nodes[j].points[i].z - pos->z;
            d = dx * dx + dy * dy + dz * dz;
            if (d < best) {
                *outIdx = i;
                *outNode = j;
                best = d;
            }
        }
    }
}

/* follow `prev` links from `node`, summing the distance walked */
s32 TraceToEnd(s32 node, s32 pos, s32 *outNode, s32 *outPos, s32 *dist, s32 stopAtTyped, s32 depth)
{
    if (!TraceCheck(node, depth)) {
        return 0;
    }
    if (depth == 0) {
        *dist = 0;
    }
    if (node < 0 || (stopAtTyped && gResPath.nodes[node].type != 0)) {
        *outNode = node;
        *outPos = pos;
        return 1;
    }
    *dist += gResPath.nodes[node].numPoints - pos;
    return TraceToEnd(gResPath.nodes[node].prev, gResPath.nodes[node].prevPos,
                          outNode, outPos, dist, stopAtTyped, depth + 1);
}

/*
 * SyncAltPaths (historical label) @ 0x800B9D68, 504 bytes: link every node to its
 * nearest neighbours (FindClosestPt from its first and from its last point), then assign each
 * node its section (TraceToCheckpoint). Type-2 nodes stay unlinked.
 * Shaping fact: the last-point argument is written index first,
 * `numPoints - 1 + points` (with `&points[numPoints - 1]` three temps rotate, 21 words).
 * Internal callees: FindClosestPt takes skip/atStart/outIdx in t1/t2/t0, so this body only
 * reproduces with the real module in the unit.
 */
void SyncAltPaths(void)
{
    s32 i;
    s32 section;
    s32 node;
    s32 idx;

    for (i = 0; i < gResPath.numNodes; i++) {
        if (gResPath.nodes[i].type == 2) {
            gResPath.nodes[i].next = -1;
            gResPath.nodes[i].nextPos = 0;
            gResPath.nodes[i].prev = -1;
            gResPath.nodes[i].prevPos = 1;
        } else {
            FindClosestPt(i, 1, &node, gResPath.nodes[i].points, &idx);
            gResPath.nodes[i].next = node;
            gResPath.nodes[i].nextPos = idx;
            FindClosestPt(i, 0, &node, gResPath.nodes[i].numPoints - 1 + gResPath.nodes[i].points, &idx);
            gResPath.nodes[i].prev = node;
            gResPath.nodes[i].prevPos = idx;
        }
    }
    for (i = 0; i < gResPath.numNodes; i++) {
        if (gResPath.nodes[i].type == 2) {
            gResPath.nodes[i].section = -1;
        } else if (!TraceToCheckpoint(i, 0, &section, 0)) {
            gResPath.nodes[i].section = -1;
        } else {
            gResPath.nodes[i].section = section;
        }
    }
}

/* section containing point `pos` of `node`, following `next` links back */
s32 TraceToCheckpoint(s32 node, s32 pos, s32 *out, s32 depth)
{
    s32 i;
    s16 best;

    if (!TraceCheck(node, depth)) {
        return 0;
    }
    if (node < 0) {
        for (i = 1; i < gCheckpoints[0].count; i++) {
            if (pos < gCheckpoints[i].start) {
                break;
            }
        }
        *out = i - 1;
        return 1;
    }
    best = -1;
    for (i = 0; i < gCheckpoints[0].count; i++) {
        if (gCheckpoints[i].nodeStart[node] >= 0 &&
            (best < 0 || best < gCheckpoints[i].nodeStart[node]) &&
            pos >= gCheckpoints[i].nodeStart[node]) {
            *out = i;
            best = gCheckpoints[i].nodeStart[node];
        }
    }
    if (best >= 0) {
        return 1;
    }
    return TraceToCheckpoint(gResPath.nodes[node].next, gResPath.nodes[node].nextPos,
                                  out, depth + 1);
}
