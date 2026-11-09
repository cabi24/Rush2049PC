/* MBOX_GetChild.c: a scene node's first child (MAT3 mbox.c, MBOX_GetChild()).
 *
 * The scene is a tree of object instances (nodes) in ObjInstanceList, linked by index: each node has its first
 * child and its next sibling, -1 for none; the roots are a sibling list from ObjHead. */
#include "game/game.h"

/* The first child of node `node`, or -1. */
s16 MBOX_GetChild(s32 node) {
    return (ObjInstanceList + node)->child;
}
