/* PurgeableMemory (MAT3 memsys.c): a chunk's lock count, which keeps compaction from moving it. include/game/heap.h
 * has the headers. */
#include "game/game.h"
#include "game/heap.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;

VIEW(s32, heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view

s32 osRecvMesg(OSMesgQueue *,void **,s32);s32 osSendMesg(OSMesgQueue *,void *,s32);

/* Unlocks (purgeable != 0: the count goes down, not below 0) or locks (up, not above 255) the chunk whose data is at
 * address. A handle's chunk (one with a master slot) is left alone. Takes the heap lock. */
void PurgeableMemory(u32 address, s32 purgeable) {
    void *heap;
    tMemoryChunk *chunk;
    s32 count;

    osRecvMesg((OSMesgQueue *)&heap_lock_queue, NULL, 1);
    heap = FindHeap(address);
    chunk = FindChunk(heap, address, 0);
    if (chunk->master != 0) {
        osSendMesg((OSMesgQueue *)&heap_lock_queue, NULL, 0);
        return;
    }
    if (purgeable) {
        count = chunk->locked;
        if (count > 0) chunk->locked = count - 1;
    } else {
        count = chunk->locked;
        if (count < 255) chunk->locked = count + 1;
    }
    osSendMesg((OSMesgQueue *)&heap_lock_queue, NULL, 0);
}
