/* OrderFx: the order of the priority pass's tree (MAT3 audiomgr.c's _OrderFx). */
#include "game/game.h"
#include "game/audiomgr.h"

/* Compares two nodes of the fx tag tree (tSortedFxTag) by their tags' weighted priority: negative, zero or
 * positive as el1's is lower, equal or higher. The priority pass walks the tree in this order (UpdateFx). */
s32 OrderFx(void *el1, void *el2) {
    return ((tSortedFxTag *) el1)->tag->wpriority - ((tSortedFxTag *) el2)->tag->wpriority;
}
