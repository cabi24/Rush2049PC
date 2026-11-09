/* heap_total_free: the free space of a heap (MAT3 memsys.c). include/game/heap.h has the headers. */
#include "game/game.h"
#include "game/heap.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);

/* Returns the bytes free in `heap` (NULL the main heap: ConvertHeap), under the heap's lock. */
u32 heap_total_free(tHeap *heap)
{
    u32 sum;

    osRecvMesg(&heap_lock_queue, 0, 1);
    sum = heap_total_free_unlocked(heap);
    osSendMesg(&heap_lock_queue, 0, 0);
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
