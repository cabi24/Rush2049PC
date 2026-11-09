/* InsertBSTEInternal (MAT3 bstree.c _InsertBSTE): the insertion into an ordered list (InitBST.c has the tree). */
#include "game/game.h"

/* A node's links: the first two words of the caller's record. */
typedef struct BSTNode { struct BSTNode *left; struct BSTNode *right; } BSTNode;

/* Walks down from the link *link (the root's, at first): left where the order function puts element before the
 * node there, else right, and hangs element, with no children, on the first empty link. Counts it in the tree. MAT3
 * recurses; this is the same walk as a loop. */
void InsertBSTEInternal(tOrderedList *tree, BSTNode **link, BSTNode *element) {
    while (1) {
        if (*link == 0) {
            *link = element;
            element->left = 0;
            (*link)->right = 0;
            tree->numElements++;
            return;
        }
        if (tree->OrderFunc(element, *link) < 0) {
            link = &(*link)->left;
        } else {
            link = &(*link)->right;
        }
    }
}
