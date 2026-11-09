/* heap_move_block_low: move a heap chunk to the lowest free place it fits (MAT3 memsys.c). include/game/heap.h has
 * the headers. */
#include "game/game.h"
#include "game/heap.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);


/* Moves the handle-owned chunk of ptr (a handle) down to the first free chunk before it that is big enough (or that it
 * adjoins): the data is copied, the master slot pointed at the new place, the rest of the free chunk split off when
 * it is HEAP_MIN_SPLIT bytes or more, and the old chunk freed. Nothing when no free place lies before it. Under the
 * heap lock. Keeps the heap's free space together (a ghost's data is moved low before a race,
 * ReadGhostDataInternal). */
void heap_move_block_low(void *ptr)
{
    tHeap *heap;
    tMemoryChunk *old;
    tMemoryChunk *after;
    tMemoryChunk *n;
    tMemoryChunk *b;
    tMemoryChunk *next;
    u32 used;
    s32 adjacent;
    u32 size;

    osRecvMesg(&heap_lock_queue, 0, 1);
    heap = FindHeap(ptr);
    old = FindChunk(heap, ptr, 0);
    after = old->next;
    adjacent = 0;
    for (b = heap->chunks; b != NULL; b = next) {
        if (b == old) {
            b = NULL;
            break;
        }
        if (b->used != 0) {
            next = b->next;
        } else {
            if (b->size >= old->size) {
                break;
            }
            next = b->next;
            if (old == next) {
                adjacent = 1;
                break;
            }
        }
    }
    if (b == NULL) {
        osSendMesg(&heap_lock_queue, 0, 0);
        return;
    }
    b->master = old->master;
    *b->master = (u8 *)b + HEAP_CHUNK_HEADER;
    b->used = 1;
    b->internal = old->internal;
    b->locked = old->locked;
    size = old->size;
    old->master = NULL;
    game_memmove((u8 *)b + HEAP_CHUNK_HEADER, (u8 *)old + HEAP_CHUNK_HEADER, old->size);
    pc_heap_block_moved((u8 *)old + HEAP_CHUNK_HEADER, (u8 *)b + HEAP_CHUNK_HEADER, size);
    if (adjacent || b->size - size >= HEAP_MIN_SPLIT) {
        n = (tMemoryChunk *)((u8 *)b + size + HEAP_CHUNK_HEADER);
        if (adjacent) {
            n->next = after;
        } else {
            n->next = b->next;
        }
        if (n->next != NULL) {
            n->next->prev = n;
        } else {
            heap->lastChunk = n;
        }
        n->prev = b;
        if (adjacent) {
            used = 0;
        } else {
            used = size + HEAP_CHUNK_HEADER;
        }
        n->size = b->size - used;
        n->master = NULL;
        n->used = adjacent;
        n->internal = 0;
        n->locked = 0;
        n->magic = HEAP_CHUNK_MAGIC;
        b->next = n;
        b->size = size;
        if (adjacent) {
            old = n;
        }
    }
    DeallocateMemoryInternal((u8 *)old + HEAP_CHUNK_HEADER, 0);
    osSendMesg(&heap_lock_queue, 0, 0);
}
