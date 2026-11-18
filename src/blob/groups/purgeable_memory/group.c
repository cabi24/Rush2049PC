/* PurgeableMemory (MAT3 memsys.c): a chunk's lock count, which keeps compaction from moving it. include/game/heap.h
 * has the headers. */
#include "game/game.h"
#include "game/heap.h"

/* Unlocks (purgeable != 0: the count goes down, not below 0) or locks (up, not above 255) the chunk whose data is at
 * ptr. A handle's chunk (one with a master slot) is left alone. Takes the heap lock. */
void PurgeableMemory(void *ptr, s32 purgeable) {
    tHeap *heap;
    tMemoryChunk *chunk;
    s32 count;

    lock_take(&heap_lock_queue);
    heap = FindHeap(ptr);
    chunk = FindChunk(heap, ptr, 0);
    if (chunk->master != NULL) {
        lock_give(&heap_lock_queue);
        return;
    }
    if (purgeable) {
        count = chunk->locked;
        if (count > 0) chunk->locked = count - 1;
    } else {
        count = chunk->locked;
        if (count < 255) chunk->locked = count + 1;
    }
    lock_give(&heap_lock_queue);
}
