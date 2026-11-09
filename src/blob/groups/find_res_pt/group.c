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

VIEW(PathGraph, gResPath);
#define gResPath gResPath_view

/* follow `next` links from `node`, summing the distance walked */
s32 TraceToMain(s32 node, s32 pos, s32 *outNode, s32 *dist, s32 depth)
{
    if (!TraceCheck(node, depth)) {
        return 0;
    }
    if (depth == 0) {
        *dist = 0;
    }
    if (node < 0) {
        *outNode = pos;
        return 1;
    }
    *dist += pos;
    return TraceToMain(gResPath.nodes[node].next, gResPath.nodes[node].nextPos,
                              outNode, dist, depth + 1);
}

/* stand-in callers: keep the callees out of line under -O3 */

