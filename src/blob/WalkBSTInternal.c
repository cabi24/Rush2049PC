/* WalkBSTInternal (MAT3 bstree.c _WalkBST): the in-order walk of an ordered list (InitBST.c has the tree). */
#include "game/game.h"

/* A node's links: the first two words of the caller's record. */
typedef struct BSTNode { struct BSTNode *left; struct BSTNode *right; } BSTNode;

/* Visits the subtree at root in order: its left subtree, then WalkFunc(root, *num) with *num incremented after,
 * then its right subtree. tree is unused. */
void WalkBSTInternal(tOrderedList *tree, void *root, s32 *num, void (*WalkFunc)(void *, s32)) {
    if (root != 0) {
        WalkBSTInternal(tree, ((BSTNode *)root)->left, num, WalkFunc);
        WalkFunc(root, *num);
        (*num)++;
        WalkBSTInternal(tree, ((BSTNode *)root)->right, num, WalkFunc);
    }
}
