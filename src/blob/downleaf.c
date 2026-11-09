/* downleaf.c: descend the road quadtree to the leaf holding a point (MAT3 stree.c, downleaf()).
 *
 * The road (the surfaces cars drive on) is indexed by a quadtree over the ground plane (stree_nodes, InitSTree);
 * updownleaf climbs from a node until the point is inside it, then calls this to go back down. */
#include "game/game.h"

/* A quadtree node: streetyp (game/types.h). Bit q of flags is set when quadrant q has a child node; minh, maxh are
 * the horizontal (x) bounds, minv, maxv the vertical (z); child[] the quadrants' child nodes, indexes into
 * stree_nodes. */
VIEW(streetyp *, stree_nodes);
#define stree_nodes stree_nodes_view

/* From node tp, go down to the leaf that holds the point (posh, posv): at each node the quadrant is 0 or 1 by the
 * horizontal half (1 the upper), plus 2 for the lower vertical half; descend while that quadrant has a child. The
 * leaf is returned, and the quadrant in it in *quad. A child index of 0 (the root itself) means no node: NULL. */
streetyp *downleaf(streetyp *tp, s16 posh, s16 posv, s16 *quad)
{
    s32 midh, midv;
    s16 q;

    for (;;) {
        midh = tp->minh + tp->maxh;
        if (midh < 0) midh++;
        midv = tp->minv + tp->maxv;
        if (midv < 0) midv++;
        midh >>= 1;
        midv >>= 1;
        q = (posh < midh) ? 0 : 1;
        if (posv < midv) {
            q += 2;
        }
        if (!(tp->flags & (1 << q))) {
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
