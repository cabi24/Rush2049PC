/* RemoveObjectTree.c: take a scene node and its subtree out of the scene (MAT3 zoid.c, _RemoveObjectTree()).
 *
 * The scene is a tree of nodes in ObjInstanceList (ObjectInstance: the first child and the next sibling are
 * indexes, -1 none; ObjHead is the first top-level node). ZOID_RemoveObjectTree and ZOID_RemoveObject call this. */
#include "game/game.h"

/* Remove the node objnum from the scene: with removeNext set, its following siblings (and their subtrees) first;
 * with removeChildren set, its children; then unlink it from its parent's child list, from the top-level list or
 * from its previous sibling, and mark it free (index 0xFFFF, no links). When it was the last node in use, the
 * count of nodes in use (gNumObjects) drops past it and every free node before it. The parameter removeChildren is
 * reused for the parent's and the previous sibling's index, as the original code does. */
void RemoveObjectTree(s16 objnum, s32 removeChildren, s32 removeNext)
{
    ObjectInstance *n;

    if (removeNext) {
        n = &ObjInstanceList[objnum];
        if (n->next >= 0) {
            RemoveObjectTree(n->next, 1, 1);
            n->next = -1;
        }
    }
    n = &ObjInstanceList[objnum];
    if (removeChildren) {
        if (n->child >= 0) {
            RemoveObjectTree(n->child, 1, 1);
            n->child = -1;
        }
    }
    removeChildren = ZOID_ObjParent(objnum);
    if (removeChildren >= 0) {
        ObjInstanceList[removeChildren].child = n->next;
    } else if (objnum == ObjHead) {
        ObjHead = n->next;
    } else {
        removeChildren = ZOID_ObjPrevObj(objnum);
        if (removeChildren >= 0) {
            ObjInstanceList[removeChildren].next = n->next;
        }
    }
    n->index = 0xFFFF;
    n->child = -1;
    n->next = -1;
    if (objnum + 1 == gNumObjects) {
        do {
            gNumObjects--;
        } while (ObjInstanceList[gNumObjects - 1].index == 0xFFFF && gNumObjects > 0);
    }
}
