/* A chunk's size and its lock count, by its data's address (MAT3 memsys.c GetPtrSize; LockMemory and UnlockMemory
 * are the N64's). include/game/heap.h has the headers. */
#include "game/game.h"
#include "game/heap.h"

s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);

/* The size in bytes of the chunk whose data is at ptr, or of a handle's chunk (a multiple of 32). Takes the heap
 * lock. */
u32 GetPtrSize(void *ptr) {
    tHeap *heap;
    tMemoryChunk *chunk;
    u32 size;

    osRecvMesg((OSMesgQueue *)&heap_lock_queue, NULL, 1);
    heap = FindHeap(ptr);
    chunk = FindChunk(heap, ptr, 0);
    size = chunk->size;
    osSendMesg((OSMesgQueue *)&heap_lock_queue, NULL, 0);
    return size;
}

/* One unlock of the chunk whose data is at ptr: its lock count goes down, not below 0. Takes the heap lock. */
void unlock_memory(void *ptr) {
    tHeap *heap;
    tMemoryChunk *chunk;
    s32 count;

    osRecvMesg((OSMesgQueue *)&heap_lock_queue, NULL, 1);
    heap = FindHeap(ptr);
    chunk = FindChunk(heap, ptr, 0);
    count = chunk->locked;
    if (count > 0) chunk->locked = count - 1;
    osSendMesg((OSMesgQueue *)&heap_lock_queue, NULL, 0);
}

/* One lock of the chunk whose data is at ptr: its lock count goes up, not above 255. Takes the heap lock. */
void lock_memory(void *ptr) {
    tHeap *heap;
    tMemoryChunk *chunk;
    s32 count;

    osRecvMesg((OSMesgQueue *)&heap_lock_queue, NULL, 1);
    heap = FindHeap(ptr);
    chunk = FindChunk(heap, ptr, 0);
    count = chunk->locked;
    if (count < 255) chunk->locked = count + 1;
    osSendMesg((OSMesgQueue *)&heap_lock_queue, NULL, 0);
}
