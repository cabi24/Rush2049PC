/* The largest free chunk of a heap (MAT3 memsys.c LargestAvailableMemoryHeapChunk). include/game/heap.h has the
 * headers. */
#include "game/game.h"
#include "game/heap.h"

/* LargestAvailableMemoryChunk under the heap lock. */
u32 LargestAvailableMemoryHeapChunk(tHeap *heap)
{
    u32 largest;

    lock_take(&heap_lock_queue);
    largest = LargestAvailableMemoryChunk(heap);
    lock_give(&heap_lock_queue);
    return largest;
}

/* The size in bytes of the largest free chunk of heap (0: the main heap): the most one allocation can get. MAT3
 * rounds it down to 64; the N64 doesn't. The caller holds the heap lock. */
u32 LargestAvailableMemoryChunk(tHeap *heap)
{
    u32 largest;
    tMemoryChunk *chunk;

    chunk = ConvertHeap(heap)->chunks;
    largest = 0;
    while (chunk != NULL) {
        if (chunk->used == 0) {
            if (largest < chunk->size) {
                largest = chunk->size;
            }
        }
        chunk = chunk->next;
    }
    return largest;
}
