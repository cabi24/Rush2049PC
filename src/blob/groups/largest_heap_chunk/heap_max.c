/* The largest free chunk of a heap (MAT3 memsys.c LargestAvailableMemoryHeapChunk). include/game/heap.h has the
 * headers. */
#include "game/game.h"
#include "game/heap.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;

VIEW(OSMesgQueue, heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);

/* LargestAvailableMemoryChunk under the heap lock. */
u32 LargestAvailableMemoryHeapChunk(tHeap *heap)
{
    u32 largest;

    osRecvMesg(&heap_lock_queue, 0, 1);
    largest = LargestAvailableMemoryChunk(heap);
    osSendMesg(&heap_lock_queue, 0, 0);
    return largest;
}

/* The size in bytes of the largest free chunk of heap (0: the main heap): the most one allocation can get. MAT3
 * rounds it down to 64; the N64 doesn't. The caller holds the heap lock. */
u32 LargestAvailableMemoryChunk(tHeap *heap)
{
    u32 largest;
    tMemoryChunk *chunk;

    chunk = N64P(ConvertHeap(heap)->chunks);
    largest = 0;
    while (chunk != 0) {
        if (chunk->used == 0) {
            if (largest < chunk->size) {
                largest = chunk->size;
            }
        }
        chunk = N64P(chunk->next);
    }
    return largest;
}
