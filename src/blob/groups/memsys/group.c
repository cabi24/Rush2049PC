/* The heap (MAT3 memsys.c): making the main heap and heaps inside it, allocating from the bottom of a heap, and the
 * master slots (handles) whose data the heap may move. include/game/heap.h describes the headers;
 * AllocateMemoryHeapHigh.c allocates from the top, heap_release/group.c frees and resizes, pointer_counters/ and
 * purgeable_memory/ lock chunks, FindHeap.c finds the heap an address is in.
 *
 * Every entry point but AllocateMemory and InitHeap takes the heap lock (heap_lock_queue, a one-message queue).
 * PC (port/ultra/heap.c, docs/port/MEMORY.md 4): the heap's making, each chunk taken, and an allocation no free chunk
 * holds are reported (pc_heap_*); nothing in the game's own code changes. */
#include "game/game.h"
#include "game/heap.h"

extern u8 game_bss_end[];   /* the end of the game image's BSS: the main heap starts at the next 32-byte boundary */
extern u32 osMemSize;       /* libultra: the RDRAM's size in bytes */

VIEW(s32[], heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view

s32 osRecvMesg();
s32 osSendMesg();
void osCreateMesgQueue();
void *memset(void *, int, unsigned int);   /* the master slots: N64P gives their pointer */

/* The heap a caller means: heap itself, or the main heap for 0 (MAT3 _ConvertHeap). */
tHeap *ConvertHeap(tHeap *heap)
{
    if (heap != 0) {
        return heap;
    }
    return gMainHeap;
}

/* Takes size bytes (rounded up to 32) from the lowest free chunk of heap that holds them, and splits off what is
 * left as a free chunk when that is at least HEAP_MIN_SPLIT bytes. master is the slot that will hold the data's
 * address (AllocateHandleHeap's) or 0; internal is 1 for the heap's own tables. Returns the data's address. The
 * caller holds the heap lock (MAT3 _AllocateMemory, without its out-of-memory callback and fill).
 * Original game: when no free chunk is large enough the search ends at chunk 0 and the code goes on to read it;
 * the PC reports that first (pc_heap_none). */
void *AllocateMemory(tHeap *heap, u32 size, void *master, s32 internal)
{
    tMemoryChunk *chunk;
    tMemoryChunk *rest;
    u32 freeTotal = 0;
    u32 largestFree = 0;
    u32 chunkSize;

    size = (size + 31) & ~31;
    for (chunk = N64P(heap->chunks); chunk != 0; chunk = N64P(chunk->next)) {
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
    if (chunk == 0) pc_heap_none(heap, size, freeTotal, largestFree);
    chunkSize = chunk->size;
    if (chunkSize - size >= HEAP_MIN_SPLIT) {
        rest = (tMemoryChunk *)((u8 *)chunk + size + HEAP_CHUNK_HEADER);
        rest->next = chunk->next;
        if (rest->next != 0) {
            N64P(rest->next)->prev = N64W(rest);
        } else {
            heap->lastChunk = N64W(rest);
        }
        rest->prev = N64W(chunk);
        rest->size = chunk->size - size - HEAP_CHUNK_HEADER;
        rest->master = 0;
        rest->used = 0;
        rest->internal = 0;
        rest->locked = 0;
        rest->magic = HEAP_MAGIC_WORD;
        chunk->next = N64W(rest);
        chunk->size = size;
    }
    chunk->used = 1;
    chunk->master = N64W(master);
    chunk->internal = internal; pc_heap_allocated(heap);
    return (u8 *)chunk + HEAP_CHUNK_HEADER;
}

/* Lays out a heap whose header is at heap and whose end (endRam) is already set: one free chunk from the first
 * 32-byte boundary past the header to the end, and a first master table of numInitialMasters slots (none for 0;
 * AllocateHandleHeap then makes one of numAdditionalMasters). The caller holds the lock or has none yet
 * (MAT3 _InitHeap). */
void InitHeap(tHeap *heap, u16 numInitialMasters, u16 numAdditionalMasters)
{
    heap->magic = HEAP_MAGIC_WORD;
    heap->next = 0;
    heap->chunks = N64W(N64PTR(tMemoryChunk *, ((N64A(heap) + 67) & ~31)));
    N64P(heap->chunks)->magic = HEAP_MAGIC_WORD;
    N64P(heap->chunks)->next = 0;
    N64P(heap->chunks)->prev = 0;
    N64P(heap->chunks)->size = heap->endRam - (u32)heap->chunks - HEAP_CHUNK_HEADER;
    N64P(heap->chunks)->master = 0;
    N64P(heap->chunks)->used = 0;
    N64P(heap->chunks)->internal = 0;
    N64P(heap->chunks)->locked = 0;
    heap->lastChunk = heap->chunks;
    heap->masters.numMasters = numInitialMasters;
    heap->numAdditionalMasters = numAdditionalMasters;
    heap->masters.nextBlock = 0;
    if (numInitialMasters > 0) {
        heap->masters.masters = N64W(AllocateMemory(heap, numInitialMasters * HEAP_MASTER_SLOT, 0, 1));
        memset(N64P(heap->masters.masters), 0, numInitialMasters * HEAP_MASTER_SLOT);
    } else {
        heap->masters.masters = 0;
    }
}

/* Allocates size bytes from heap (0: the main heap), under the heap lock. Returns the data's address. */
void *AllocateMemoryHeap(tHeap *heap, u32 size)
{
    void *data;

    osRecvMesg(heap_lock_queue, 0, 1);
    data = AllocateMemory(ConvertHeap(heap), size, 0, 0);
    osSendMesg(heap_lock_queue, 0, 0);
    return data;
}

/* Allocates size bytes from heap (0: the main heap) behind a master slot, and returns the slot's address (a
 * handle): the slot holds the data's address, which compaction may change. The first free slot of the heap's
 * master tables is taken; when every table is full a table of numAdditionalMasters slots is added. */
void *AllocateHandleHeap(tHeap *heap, u32 size)
{
    tHeap *target;
    uintptr_t *slot;
    tMasterBlock *table;
    u32 i;

    osRecvMesg(heap_lock_queue, 0, 1);
    target = ConvertHeap(heap);
    table = &target->masters;
    if (table->masters == 0) {
        table->numMasters = target->numAdditionalMasters;
        table->masters = N64W(AllocateMemory(target, target->numAdditionalMasters * HEAP_MASTER_SLOT, 0, 1));
        memset(N64P(table->masters), 0, target->numAdditionalMasters * HEAP_MASTER_SLOT);
        table->nextBlock = 0;
    }
    i = 0;
    for (;;) {
        slot = N64P(table->masters);
        for (; i < table->numMasters; i++, slot++) {
            if (*slot == 0) {
                break;
            }
        }
        if (i < table->numMasters) {
            break;
        }
        i = 0;
        if (table->nextBlock == 0) {
            table->nextBlock = N64W(AllocateMemory(target, sizeof(tMasterBlock), 0, 1));
            N64P(table->nextBlock)->numMasters = target->numAdditionalMasters;
            N64P(table->nextBlock)->masters =
                N64W(AllocateMemory(target, target->numAdditionalMasters * HEAP_MASTER_SLOT, 0, 1));
            memset(N64P(N64P(table->nextBlock)->masters), 0, target->numAdditionalMasters * HEAP_MASTER_SLOT);
            N64P(table->nextBlock)->nextBlock = 0;
        }
        table = N64P(table->nextBlock);
    }
    *slot = (uintptr_t)AllocateMemory(target, size, slot, 0);
    osSendMesg(heap_lock_queue, 0, 0);
    return slot;
}

/* Makes a heap of size bytes inside parent (0: the main heap): a chunk of the parent, marked internal, laid out by
 * InitHeap with the given master tables, and appended to the chain of heaps that starts at gMainHeap (which
 * FindHeap walks). Returns the new heap; destroy_heap gives it back. */
void *create_heap(tHeap *parent, u32 size, u16 numInitialMasters, u16 numAdditionalMasters)
{
    tHeap *heap;
    tHeap *other;

    osRecvMesg(heap_lock_queue, 0, 1);
    heap = AllocateMemory(ConvertHeap(parent), size + 64, 0, 1);
    heap->endRam = N64A(heap) + ((size + 95) & ~31);
    InitHeap(heap, numInitialMasters, numAdditionalMasters);
    for (other = gMainHeap; other != 0; other = N64P(other->next)) {
        if (other->next == 0) {
            other->next = N64W(heap);
            break;
        }
    }
    osSendMesg(heap_lock_queue, 0, 0);
    return heap;
}

/* Puts the main heap's header at the first 32-byte boundary past the game's BSS and its end at the end of RDRAM, and
 * notes an Expansion Pak (more than 4 MiB), which changes what the game keeps loaded (expansion_pak_present). */
void locate_main_heap(void)
{
    tHeap *heap;

    heap = N64PTR(tHeap *, ((N64A(game_bss_end) + 31) & ~31));
    gMainHeap = heap;
    heap->endRam = osMemSize | 0x80000000;
    if (heap->endRam >= 0x80400001) {
        expansion_pak_present = 1;
    }
}

/* The heap's start-up (MAT3 InitMemory): makes the heap lock the first time, then lays out the main heap over all
 * free memory with numInitialMasters master slots (InitSystem asks for 50, and 10 more each time they run out). */
void InitMemory(u16 numInitialMasters, u16 numAdditionalMasters)
{
    static s8 heap_lock_created = 0;

    if (heap_lock_created == 0) {
        heap_lock_created = 1;
        osCreateMesgQueue(heap_lock_queue, &heap_lock_msg, 1);
        osSendMesg(heap_lock_queue, 0, 0);
    }
    locate_main_heap();
    InitHeap(gMainHeap, numInitialMasters, numAdditionalMasters); pc_heap_created(gMainHeap);
}
