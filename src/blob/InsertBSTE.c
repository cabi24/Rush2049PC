/* InsertBSTE (MAT3 bstree.c): adds a node to an ordered list (InitBST.c has the tree). */
#include "game/game.h"

/* Inserts element (a record whose first two words are free for the links) into tree by its order function. */
void InsertBSTE(tOrderedList *tree, void *element) {
    InsertBSTEInternal(tree, (struct BSTNode **)&tree->root, element);
}
