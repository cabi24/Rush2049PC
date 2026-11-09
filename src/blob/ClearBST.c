/* ClearBST (MAT3 bstree.c): empties an ordered list (InitBST.c has the tree). The nodes are the caller's. */
#include "game/game.h"

/* Forgets every node of tree: no root, no count. */
void ClearBST(tOrderedList *tree) {
    tree->root = NULL;
    tree->numElements = 0;
}
