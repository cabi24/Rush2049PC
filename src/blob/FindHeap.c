/* FindHeap (MAT3 memsys.c _FindHeap): the heap an address is in. include/game/heap.h has the headers. */
#include "game/game.h"
#include "game/heap.h"

/* The last heap, in the chain from gMainHeap, whose chunks span address (a window address): a heap made inside
 * another lies within its parent's range and comes later in the chain, so the innermost one wins. 0 when none. */
void *FindHeap(u32 address) {
    tHeap *heap;
    tHeap *found;

    heap = gMainHeap;
    found = 0;
    if (heap != 0) {
        do {
            if ((address >= (u32) heap->chunks) && (address < (u32) heap->endRam)) {
                found = heap;
            }
            heap = N64P(heap->next);
        } while (heap != 0);
    }
    return (void *) found;
}
