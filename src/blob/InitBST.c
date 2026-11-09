/* InitBST (MAT3 bstree.c): an ordered list, a binary search tree of caller-owned nodes whose first two words are the
 * left and right links (tBSTE), ordered by a comparison function. ClearBST empties one, InsertBSTE adds a node,
 * WalkBST visits the nodes in order. The sound manager's priority pass is the one user (audiomgr_thread.c).
 *
 * PC: tOrderedList has the host layout (the order function a pointer, so the root is at +0x10; docs/port/LAYOUT.md
 * 5). Until 2026-11-08 InitBST, ClearBST and WalkBST addressed the N64's (the root at +8); nothing called them on the
 * PC, and audiomgr_thread.c has its own clear and walk with the host layout. */
#include "game/game.h"

/* Makes tree empty, ordered by OrderFunc (negative: the first node goes left of the second). */
void InitBST(tOrderedList *tree, s32 (*OrderFunc)(void *, void *)) {
    tree->root = NULL;
    tree->numElements = 0;
    tree->OrderFunc = OrderFunc;
}
