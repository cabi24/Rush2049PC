/* AllocateMemoryHeapHigh (MAT3 memsys.c): allocation from the top of a heap, for the long-lived buffers (the frame
 * buffers) so that they stay out of the way of what comes and goes at the bottom. include/game/heap.h has the
 * headers; memsys/group.c the rest of the heap. */
#include "game/game.h"
#include "game/heap.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);

/* Takes size bytes (rounded up to 32) from the highest free chunk of heap (0: the main heap) that holds them: from
 * the chunk's top end when the rest is worth keeping as a free chunk (at least HEAP_MIN_SPLIT bytes), else the
 * whole chunk. Returns the data. Takes the heap lock.
 * Original game: when no free chunk is large enough the search ends at chunk 0 and the code goes on to read it (the
 * MAT3 code calls the heap's out-of-memory callback there). The totals it keeps are never used. */
void *AllocateMemoryHeapHigh(tHeap *heap, u32 size)
{
    tMemoryChunk *taken;
    tMemoryChunk *chunk;
    u32 freeTotal;
    u32 largestFree;
    u32 chunkSize;

    osRecvMesg(&heap_lock_queue, 0, 1);
    freeTotal = 0;
    largestFree = 0;
    heap = ConvertHeap(heap);
    size = (size + 31) & ~31;
    for (chunk = heap->lastChunk; chunk != NULL; chunk = chunk->prev) {
        if (chunk->used == 0) {
            chunkSize = chunk->size;
            freeTotal += chunkSize;
            if (largestFree < chunkSize) {
                largestFree = chunkSize;
            }
            if (chunkSize >= size) {
                break;
            }
        }
    }
    chunkSize = chunk->size;
    if (chunkSize - size >= HEAP_MIN_SPLIT) {
        taken = (tMemoryChunk *)((u8 *)chunk + chunkSize - size);
        chunkSize = chunk->size;
        taken->next = chunk->next;
        if (taken->next != NULL) {
            taken->next->prev = taken;
        } else {
            heap->lastChunk = taken;
        }
        taken->prev = chunk;
        taken->size = size;
        taken->master = NULL;
        taken->used = 1;
        taken->internal = 0;
        taken->locked = 0;
        taken->magic = HEAP_CHUNK_MAGIC;
        chunk->next = taken;
        chunk->size = chunk->size - size - HEAP_CHUNK_HEADER;
        chunk = taken;
    } else {
        chunk->master = NULL;
        chunk->used = 1;
        chunk->internal = 0;
    }
    osSendMesg(&heap_lock_queue, 0, 0);
    return (u8 *)chunk + HEAP_CHUNK_HEADER;
}
