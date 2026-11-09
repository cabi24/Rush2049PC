#include "game/game.h"

/* PC: the scene node's host layout, as every view has it (docs/port/POINTERS.md 9). */
typedef struct Node {
    u32 flags;
    u32 w4;
    void *obj;
    float scale, w;
    /* 0x18 */ u16 id;
    /* 0x1A */ s16 child;
    /* 0x1C */ s16 sibling;
    s16 mode;
    void *light, *tex[4], *rect, *clip, *pal;
    u32 prim, env;
} Node; /* 0x68 */

VIEW(Node[], ObjInstanceList);
#define ObjInstanceList ObjInstanceList_view

#define SCENE_DEBUG 0

void RemoveObjectTree(s16 idx, s32 freeChildren, s32 freeSiblings)
{
    Node *n;

    if (SCENE_DEBUG) {
    }
    if (freeSiblings) {
        n = &ObjInstanceList[idx];
        if (n->sibling >= 0) {
            RemoveObjectTree(n->sibling, 1, 1);
            n->sibling = -1;
        }
    }
    n = &ObjInstanceList[idx];
    if (freeChildren) {
        if (n->child >= 0) {
            RemoveObjectTree(n->child, 1, 1);
            n->child = -1;
        }
    }
    freeChildren = ZOID_ObjParent(idx);
    if (freeChildren >= 0) {
        ObjInstanceList[freeChildren].child = n->sibling;
    } else if (idx == ObjHead) {
        ObjHead = n->sibling;
    } else {
        freeChildren = ZOID_ObjPrevObj(idx);
        if (freeChildren >= 0) {
            ObjInstanceList[freeChildren].sibling = n->sibling;
        }
    }
    n->id = 0xFFFF;
    n->child = -1;
    n->sibling = -1;
    if (idx + 1 == gNumObjects) {
        do {
            gNumObjects--;
        } while (ObjInstanceList[gNumObjects - 1].id == 0xFFFF && gNumObjects > 0);
    }
}
