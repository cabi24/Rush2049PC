/* heap_total_free: the free space of a heap (MAT3 memsys.c). include/game/heap.h has the headers. */
#include "game/game.h"
#include "game/heap.h"

/* Returns the bytes free in `heap` (NULL the main heap: ConvertHeap), under the heap's lock. */
u32 heap_total_free(tHeap *heap)
{
    u32 sum;

    lock_take(&heap_lock_queue);
    sum = heap_total_free_unlocked(heap);
    lock_give(&heap_lock_queue);
    return sum;
}

/* The same without the lock: the sizes of its unused chunks added up. */
u32 heap_total_free_unlocked(tHeap *heap)
{
    u32 sum;
    tMemoryChunk *chunk;

    chunk = ConvertHeap(heap)->chunks;
    sum = 0;
    while (chunk != NULL) {
        if (chunk->used == 0) {
            sum += chunk->size;
        }
        chunk = chunk->next;
    }
    return sum;
}
