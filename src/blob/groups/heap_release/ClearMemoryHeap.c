/* ClearMemoryHeap: free and compact a heap (MAT3 memsys.c). include/game/heap.h has the headers. */
#include "game/game.h"
#include "game/heap.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;
s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);

/* Frees every chunk of the heap (hint, or the main heap for NULL) that nobody owns or has locked and that isn't the
 * heap's own, then moves each handle-owned unlocked chunk down into the first free chunk that fits (or that it
 * adjoins), pointing its master slot at the new place, as heap_move_block_low does for one chunk. A chunk whose
 * header a free has merged away is filled over (FillFreeMem), so its magic is gone: the walk then goes on from the
 * chunk before. Under the heap lock. */
void ClearMemoryHeap(tHeap *hint) {
    tHeap *heap;
    tMemoryChunk *chunk, *cursor, *after, *tail, *previous;
    tMemoryChunk *next;
    s32 adjacent;
    u32 size, difference;

    osRecvMesg((OSMesgQueue *)&heap_lock_queue, NULL, 1);
    heap = ConvertHeap(hint);
    chunk = heap->chunks;
    while (chunk != NULL) {
        if (chunk->used && !chunk->locked && !chunk->master && !chunk->internal) {
            DeallocateMemoryInternal((u8 *)chunk + HEAP_CHUNK_HEADER, 0);
            chunk = heap->chunks;
        }
        chunk = chunk->next;
    }
    chunk = heap->chunks;
    while (chunk != NULL) {
        after = chunk->next;
        if (chunk->used && chunk->master && !chunk->locked) {
            cursor = heap->chunks;
            adjacent = 0;
            while (cursor != NULL) {
                if (cursor == chunk) {
                    cursor = NULL;
                    break;
                }
                if (cursor->used) {
                    next = cursor->next;
                } else {
                    if (cursor->size >= chunk->size) break;
                    next = cursor->next;
                    if (chunk == next) {
                        adjacent = 1;
                        break;
                    }
                }
                cursor = next;
            }
            if (cursor != NULL) {
                cursor->master = chunk->master;
                *cursor->master = (u8 *)cursor + HEAP_CHUNK_HEADER;
                cursor->used = 1;
                cursor->internal = chunk->internal;
                cursor->locked = chunk->locked;
                size = chunk->size;
                chunk->master = NULL;
                game_memmove((u8 *)cursor + HEAP_CHUNK_HEADER, (u8 *)chunk + HEAP_CHUNK_HEADER, chunk->size);
                pc_heap_block_moved((u8 *)chunk + HEAP_CHUNK_HEADER, (u8 *)cursor + HEAP_CHUNK_HEADER, size);
                if (adjacent || cursor->size - size >= HEAP_MIN_SPLIT) {
                    tail = (tMemoryChunk *)((u8 *)cursor + size + HEAP_CHUNK_HEADER);
                    if (adjacent) tail->next = after;
                    else tail->next = cursor->next;
                    if (tail->next != NULL) tail->next->prev = tail;
                    else heap->lastChunk = tail;
                    tail->prev = cursor;
                    if (adjacent) difference = 0; else difference = size + HEAP_CHUNK_HEADER;
                    tail->size = cursor->size - difference;
                    tail->master = NULL;
                    tail->used = adjacent;
                    tail->internal = 0;
                    tail->locked = 0;
                    tail->magic = HEAP_CHUNK_MAGIC;
                    cursor->next = tail;
                    cursor->size = size;
                    if (adjacent) chunk = tail;
                }
                previous = chunk->prev;
                DeallocateMemoryInternal((u8 *)chunk + HEAP_CHUNK_HEADER, 0);
                if (after->magic != HEAP_CHUNK_MAGIC) {
                    if (chunk->magic != HEAP_CHUNK_MAGIC) after = previous->next;
                    else after = chunk->next;
                }
            }
        }
        chunk = after;
    }
    osSendMesg((OSMesgQueue *)&heap_lock_queue, NULL, 0);
}
