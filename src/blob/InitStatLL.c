/* InitStatLL (MAT3 statll.c): a static list, a pool of fixed-size elements carved from an array, linked singly or
 * doubly. GetStatLLE takes an element, FreeStatLLE gives one back, ClearStatLL frees them all.
 *
 * The header is include/game's tStatList (MAT3's, with host pointers). Every list's elements link by host pointers:
 * an element's first pointer is its link (MAT3 tStatLLE), a doubly linked one's second its link back (tDLLE). Until
 * the level-object nodes left the RDRAM window (docs/port/NATIVE.md step 3) the node pool linked them by 32-bit
 * window addresses instead, flagged in its header (n64); no list does now. */
#include "game/game.h"

/* Makes a list of numElements elements of elementSize bytes each from array, all free; dbl says whether the
 * elements are doubly linked (a tDLLE at their start, else a tStatLLE). */
void InitStatLL(tStatList *list, void *array, s32 elementSize, s32 numElements, u8 dbl) {
    list->list = array;
    list->numElements = numElements;
    list->elementSize = elementSize;
    list->dbl = dbl;
    ClearStatLL(list);
}
