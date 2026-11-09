/* FindChunk.c: the heap chunk an address (or a handle) is in (MAT3 memsys.c _FindChunk). include/game/heap.h has the
 * headers. */
#include "game/game.h"
#include "game/heap.h"

/* The chunk of heap with the given internal flag that holds ptr, or NULL. An address inside one of the heap's master
 * tables is a handle: the address its slot holds is looked up instead. A chunk holds the addresses from its header to
 * the next chunk's (the last chunk, everything after it). */
tMemoryChunk *FindChunk(tHeap *heap, void *ptr, s32 internal) {
    tMasterBlock *table;
    tMemoryChunk *chunk;
    u8 *addr = ptr;

    table = &heap->masters;
    while (1) {
        if (table == NULL || table->masters == NULL) break;
        if (addr >= (u8 *)table->masters && addr < (u8 *)(table->masters + table->numMasters)) {
            addr = *(u8 **)addr;
            break;
        }
        table = table->nextBlock;
    }
    chunk = heap->chunks;
    while (chunk != NULL) {
        if (internal != chunk->internal || addr < (u8 *)chunk || (chunk->next != NULL && addr >= (u8 *)chunk->next)) {
            chunk = chunk->next;
        } else break;
    }
    return chunk;
}
