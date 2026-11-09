#include "game/game.h"

typedef struct QNode {
    u8 f0[3];
    u8 leafMask;    /* 0x03: bit q set = quadrant q has a child node */
    s16 x0, x1;     /* 0x04, 0x06: horizontal bounds */
    s16 y0, y1;     /* 0x08, 0x0A: vertical bounds */
    u16 child[4];   /* 0x0C: node indices into stree_nodes */
} QNode; /* 0x14 */

VIEW(QNode *, stree_nodes);
#define stree_nodes stree_nodes_view

QNode *downleaf(QNode *tp, s16 x, s16 y, s16 *quad)
{
    s32 midh, midv;
    s16 q;

    for (;;) {
        midh = tp->x0 + tp->x1;
        if (midh < 0) midh++;
        midv = tp->y0 + tp->y1;
        if (midv < 0) midv++;
        midh >>= 1;
        midv >>= 1;
        q = (x < midh) ? 0 : 1;
        if (y < midv) {
            q += 2;
        }
        if (!(tp->leafMask & (1 << q))) {
            break;
        }
        tp = &stree_nodes[tp->child[q]];
        if (tp == stree_nodes) {
            return 0;
        }
    }
    *quad = q;
    return tp;
}
