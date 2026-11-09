/* FindHeap (MAT3 memsys.c _FindHeap): the heap an address is in. include/game/heap.h has the headers. */
#include "game/game.h"
#include "game/heap.h"

/* The last heap, in the chain from gMainHeap, whose chunks span ptr: a heap made inside another lies within its
 * parent's range and comes later in the chain, so the innermost one wins. NULL when none. */
tHeap *FindHeap(void *ptr) {
    tHeap *heap;
    tHeap *found;
    u8 *addr = ptr;

    heap = gMainHeap;
    found = NULL;
    if (heap != NULL) {
        do {
            if (addr >= (u8 *)heap->chunks && addr < heap->endRam) {
                found = heap;
            }
            heap = heap->next;
        } while (heap != NULL);
    }
    return found;
}
