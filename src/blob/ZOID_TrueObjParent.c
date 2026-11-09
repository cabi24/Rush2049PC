/* ZOID_TrueObjParent.c: a scene node's parent (MAT3 zoid.c, ZOID_TrueObjParent()). */
#include "game/game.h"

/* The parent of node `child`: the node among whose children (its first child and that one's siblings) it is,
 * or -1 for a root. A search of the whole table: the nodes keep no parent link. */
s16 ZOID_TrueObjParent(s16 child)
{
    s32 i;
    s16 target;
    s32 node;
    s16 first_child;
    s16 sibling;
    ObjectInstance *parent;

    i = 0;
    if (gNumObjects > 0) {
        target = child;
        do {
            parent = &ObjInstanceList[i];
            first_child = parent->child;
            if (target == first_child)
                return i;
            node = first_child;
            if (first_child != -1) {
                do {
                    sibling = ObjInstanceList[node].next;
                    if (target == sibling)
                        return i;
                    node = sibling;
                } while (sibling != -1);
            }
            i++;
        } while (i < gNumObjects);
    }
    return -1;
}
