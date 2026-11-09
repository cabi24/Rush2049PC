/* ZOID_RemoveObjectTree.c: take a node, and maybe its subtree, out of the scene (MAT3 zoid.c,
 * ZOID_RemoveObjectTree()). */
#include "game/game.h"

/* Free node `node` and, if `removeChildren`, all its descendants (RemoveObjectTree); its siblings stay. */
void ZOID_RemoveObjectTree(s16 node, s32 removeChildren) {
    RemoveObjectTree(node, removeChildren, 0);
}
