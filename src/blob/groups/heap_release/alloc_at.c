/* allocate_memory_at: take the heap block at a given address (the N64's own; it reserves a runtime image's place).
 * include/game/heap.h has the headers. */
#include "game/game.h"
#include "game/heap.h"

/* PC: the block taken is reported (port/ultra/heap.c, docs/port/MEMORY.md 4). */

s32 osRecvMesg(void *mq, void **msg, s32 flags);
s32 osSendMesg(void *mq, void *msg, s32 flags);

/* Takes size bytes (rounded up to 32) of the heap at ptr, which must lie in a free chunk: the chunk is split there
 * (the part before stays free, or joins the chunk before when under HEAP_MIN_SPLIT bytes) and after the size (when
 * HEAP_MIN_SPLIT bytes or more are left). Returns the taken data, at ptr. Under the heap lock. load_overlay takes a
 * runtime image's region so.
 * Original game: the chunk is found by its range alone, free or not, and its end is not checked against the size. */
void *allocate_memory_at(void *ptr, u32 size) {
    tHeap *heap;
    tMemoryChunk *b;
    tMemoryChunk *n;
    u32 before;
    tMemoryChunk *prev;
    u8 *addr = ptr;
    void *result;

    osRecvMesg(&heap_lock_queue, 0, 1);
    heap = FindHeap(addr);
    size = (size + 31) & ~31;
    for (b = heap->chunks; b != NULL; b = b->next) {
        if (addr >= (u8 *)b && (addr < (u8 *)b->next || b->next == NULL)) {
            break;
        }
    }
    result = (u8 *)b + HEAP_CHUNK_HEADER;
    if (addr != result) {
        n = (tMemoryChunk *)(addr - HEAP_CHUNK_HEADER);
        n->next = b->next;
        if (n->next != NULL) {
            n->next->prev = n;
        } else {
            heap->lastChunk = n;
        }
        n->master = NULL;
        n->used = 1;
        n->internal = 0;
        n->locked = 0;
        n->magic = HEAP_CHUNK_MAGIC;
        before = addr - (u8 *)b - HEAP_CHUNK_HEADER;
        n->size = b->size - before;
        if (before >= HEAP_MIN_SPLIT) {
            n->prev = b;
            b->next = n;
            b->size = b->size - n->size - HEAP_CHUNK_HEADER;
        } else {
            prev = b->prev;
            n->prev = prev;
            prev->next = n;
            prev->size = prev->size + (addr - (u8 *)b) - HEAP_CHUNK_HEADER;
        }
        b = n;
        result = (u8 *)b + HEAP_CHUNK_HEADER;
    }
    if (b->size - size >= HEAP_MIN_SPLIT) {
        n = (tMemoryChunk *)((u8 *)b + size + HEAP_CHUNK_HEADER);
        n->next = b->next;
        if (n->next != NULL) {
            n->next->prev = n;
        } else {
            heap->lastChunk = n;
        }
        n->prev = b;
        n->size = b->size - size - HEAP_CHUNK_HEADER;
        n->master = NULL;
        n->used = 0;
        n->internal = 0;
        n->locked = 0;
        n->magic = HEAP_CHUNK_MAGIC;
        b->next = n;
        b->size = size;
    }
    b->master = NULL;
    b->used = 1;
    b->internal = 0; pc_heap_allocated(heap);
    osSendMesg(&heap_lock_queue, 0, 0);
    return result;
}
