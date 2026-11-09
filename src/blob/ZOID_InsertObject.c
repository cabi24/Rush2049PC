/* ZOID_InsertObject.c: link a scene node into the tree (MAT3 zoid.c, ZOID_InsertObject()). */
#include "game/game.h"

/* Link node `node` in as the last child of `parent`, or as the last root (the list from ObjHead) when `parent`
 * is negative. The node's own next link must be -1 already. */
void ZOID_InsertObject(s16 node, s16 parent) {
    if (parent < 0) {
        if (ObjHead < 0) {
            ObjHead = node;
        } else {
            ObjInstanceList[ZOID_LastSibling(ObjHead)].next = node;
        }
    } else {
        if (ObjInstanceList[parent].child == -1) {
            ObjInstanceList[parent].child = node;
        } else {
            ObjInstanceList[ZOID_LastSibling(ObjInstanceList[parent].child)].next = node;
        }
    }
}
