/* destroy_heap (MAT3 memsys.c DestroyHeap): gives back a heap made by create_heap. include/game/heap.h has the
 * headers. */
#include "game/game.h"
#include "game/heap.h"

/* Unlinks heap from the chain of heaps that starts at gMainHeap and frees the chunk of its parent that holds it,
 * with everything still allocated in it. Takes the heap lock. */
void destroy_heap(tHeap *heap)
{
    tHeap *other;
    tHeap *target;

    lock_take(&heap_lock_queue);
    target = ConvertHeap(heap);
    for (other = gMainHeap; other != NULL; other = other->next) {
        if (other->next == target) {
            other->next = target->next;
            break;
        }
    }
    DeallocateMemoryInternal(target, 1);
    lock_give(&heap_lock_queue);
}
