/* WalkBST (MAT3 bstree.c): visits an ordered list's nodes in order (InitBST.c has the tree). */
#include "game/game.h"

/* Calls WalkFunc(node, n) for each node of tree, smallest first, n counting from 0. */
void WalkBST(tOrderedList *tree, void (*WalkFunc)(void *, s32)) {
    s32 num;

    num = 0;
    WalkBSTInternal(tree, tree->root, &num, WalkFunc);
}
