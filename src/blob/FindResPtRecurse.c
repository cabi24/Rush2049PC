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
    /* 0x00 */ u8 pad0[2];
    /* 0x02 */ s16 last;      /* valid in element 0 */
    /* 0x04 */ u8 pad4[4];
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

s32 FindResPtRecurse(s32 node, s32 pos, s32 *outNode, s32 *outPos, s32 stopAtTyped, s32 depth)
{
    s32 i;
    s32 unused0, unused1;
    s32 dist1, total, dist2, start1, node2, pos2, start2;
    s32 v;

    if (depth == 0) {
        *outNode = node;
        *outPos = pos;
    }
    gPathSearch[depth] = node;
    for (i = 0; i < depth; i++) {
        if (node == gPathSearch[i]) {
            return 0;
        }
    }
    v = gResPath.nodes[node].next;
    if (v == gResPath.nodes[node].prev) {
        if (gResPath.nodes[node].nextPos < gResPath.nodes[node].prevPos) {
            v = gResPath.nodes[node].prevPos - gResPath.nodes[node].nextPos + 1;
            *outPos = gResPath.nodes[node].nextPos + pos * v / gResPath.nodes[node].numPoints;
        } else {
            if (v != -1) {
                return 0;
            }
            v = gResPath.nodes[node].prevPos - gCheckpoints[gCheckpoints[0].last].start + gResPath.numPoints - gResPath.nodes[node].nextPos + 1;
            *outPos = gResPath.nodes[node].nextPos + pos * v / gResPath.nodes[node].numPoints;
            if (*outPos >= gResPath.numPoints) {
                *outPos = *outPos + gCheckpoints[gCheckpoints[0].last].start - gResPath.numPoints;
            }
        }
        *outNode = gResPath.nodes[node].next;
        if (*outNode < 0 || (stopAtTyped && gResPath.nodes[*outNode].type != 0)) {
            return 1;
        }
        return FindResPtRecurse(*outNode, *outPos, outNode, outPos, stopAtTyped, depth + 1);
    }
    if (!TraceToMain(node, pos, &start1, &dist1, 0)) {
        return 0;
    }
    if (!TraceToEnd(node, pos, &node2, &pos2, &total, stopAtTyped, 0)) {
        return 0;
    }
    if (!TraceToMain(node2, pos2, &start2, &dist2, 0)) {
        return 0;
    }
    if (start2 < start1) {
        v = start2 + gResPath.numPoints - start1 - gCheckpoints[gCheckpoints[0].last].start;
        if (start1 - start2 < v) {
            dist1 = dist1 + start1 - start2;
            start1 = start2;
        } else {
            dist2 = dist2 + v;
        }
    } else if (start2 - start1 > start1 + gResPath.numPoints - start2) {
        dist1 = dist1 + start1 + gResPath.numPoints - start2;
        start1 = start2;
    } else {
        dist2 = dist2 + start2 - start1;
    }
    total = total + dist1;
    TraceToIndex(node2, pos2, dist2 - dist1 * dist2 / total, outNode, outPos);
    return 1;
}

