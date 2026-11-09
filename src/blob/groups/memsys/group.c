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

extern u32 osMemSize;       /* libultra: the RDRAM's size in bytes */

s32 osRecvMesg();
s32 osSendMesg();
void osCreateMesgQueue();
void *memset(void *, int, unsigned int);

/* The headers' sizes are the N64's (include/game/heap.h): the allocator's arithmetic depends on them. */
_Static_assert(sizeof(tMemoryChunk) == HEAP_CHUNK_HEADER, "a chunk's header is 32 bytes");
_Static_assert(sizeof(tHeap) <= HEAP_HEADER_ROOM, "a heap's header fits before its first chunk");
_Static_assert(sizeof(tMasterBlock) <= HEAP_CHUNK_HEADER, "a master table's header takes one 32-byte unit");

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
 * address (AllocateHandleHeap's) or NULL; internal is 1 for the heap's own tables. Returns the data. The caller holds
 * the heap lock (MAT3 _AllocateMemory, without its out-of-memory callback and fill).
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
    for (chunk = heap->chunks; chunk != NULL; chunk = chunk->next) {
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
    if (chunk == NULL) pc_heap_none(heap, size, freeTotal, largestFree);
    chunkSize = chunk->size;
    if (chunkSize - size >= HEAP_MIN_SPLIT) {
        rest = (tMemoryChunk *)((u8 *)chunk + size + HEAP_CHUNK_HEADER);
        rest->next = chunk->next;
        if (rest->next != NULL) {
            rest->next->prev = rest;
        } else {
            heap->lastChunk = rest;
        }
        rest->prev = chunk;
        rest->size = chunk->size - size - HEAP_CHUNK_HEADER;
        rest->master = NULL;
        rest->used = 0;
        rest->internal = 0;
        rest->locked = 0;
        rest->magic = HEAP_CHUNK_MAGIC;
        chunk->next = rest;
        chunk->size = size;
    }
    chunk->used = 1;
    chunk->master = master;
    chunk->internal = internal; pc_heap_allocated(heap);
    return (u8 *)chunk + HEAP_CHUNK_HEADER;
}

/* Lays out a heap whose header is at heap and whose end (endRam) is already set: one free chunk from the first
 * 32-byte boundary past the header's room (HEAP_HEADER_ROOM) to the end, and a first master table of
 * numInitialMasters slots (none for 0; AllocateHandleHeap then makes one of numAdditionalMasters). The caller holds
 * the lock or has none yet (MAT3 _InitHeap). */
void InitHeap(tHeap *heap, u16 numInitialMasters, u16 numAdditionalMasters)
{
    tMemoryChunk *first;

    heap->magic = HEAP_MAGIC_WORD;
    heap->next = NULL;
    first = (tMemoryChunk *)(((uintptr_t)heap + HEAP_HEADER_ROOM + 3) & ~(uintptr_t)31);
    heap->chunks = first;
    first->magic = HEAP_CHUNK_MAGIC;
    first->next = NULL;
    first->prev = NULL;
    first->size = heap->endRam - (u8 *)first - HEAP_CHUNK_HEADER;
    first->master = NULL;
    first->used = 0;
    first->internal = 0;
    first->locked = 0;
    heap->lastChunk = heap->chunks;
    heap->masters.numMasters = numInitialMasters;
    heap->numAdditionalMasters = numAdditionalMasters;
    heap->masters.nextBlock = NULL;
    if (numInitialMasters > 0) {
        heap->masters.masters = AllocateMemory(heap, numInitialMasters * HEAP_MASTER_SLOT, NULL, 1);
        memset(heap->masters.masters, 0, numInitialMasters * HEAP_MASTER_SLOT);
    } else {
        heap->masters.masters = NULL;
    }
}

/* Allocates size bytes from heap (0: the main heap), under the heap lock. Returns the data. */
void *AllocateMemoryHeap(tHeap *heap, u32 size)
{
    void *data;

    osRecvMesg(&heap_lock_queue, 0, 1);
    data = AllocateMemory(ConvertHeap(heap), size, NULL, 0);
    osSendMesg(&heap_lock_queue, 0, 0);
    return data;
}

/* Allocates size bytes from heap (0: the main heap) behind a master slot, and returns the slot's address (a
 * handle): the slot holds the data's address, which compaction may change. The first free slot of the heap's
 * master tables is taken; when every table is full a table of numAdditionalMasters slots is added. */
void *AllocateHandleHeap(tHeap *heap, u32 size)
{
    tHeap *target;
    void **slot;
    tMasterBlock *table;
    u32 i;

    osRecvMesg(&heap_lock_queue, 0, 1);
    target = ConvertHeap(heap);
    table = &target->masters;
    if (table->masters == NULL) {
        table->numMasters = target->numAdditionalMasters;
        table->masters = AllocateMemory(target, target->numAdditionalMasters * HEAP_MASTER_SLOT, NULL, 1);
        memset(table->masters, 0, target->numAdditionalMasters * HEAP_MASTER_SLOT);
        table->nextBlock = NULL;
    }
    i = 0;
    for (;;) {
        slot = table->masters;
        for (; i < table->numMasters; i++, slot++) {
            if (*slot == NULL) {
                break;
            }
        }
        if (i < table->numMasters) {
            break;
        }
        i = 0;
        if (table->nextBlock == NULL) {
            table->nextBlock = AllocateMemory(target, sizeof(tMasterBlock), NULL, 1);
            table->nextBlock->numMasters = target->numAdditionalMasters;
            table->nextBlock->masters =
                AllocateMemory(target, target->numAdditionalMasters * HEAP_MASTER_SLOT, NULL, 1);
            memset(table->nextBlock->masters, 0, target->numAdditionalMasters * HEAP_MASTER_SLOT);
            table->nextBlock->nextBlock = NULL;
        }
        table = table->nextBlock;
    }
    *slot = AllocateMemory(target, size, slot, 0);
    osSendMesg(&heap_lock_queue, 0, 0);
    return slot;
}

/* Makes a heap of size bytes inside parent (0: the main heap): a chunk of the parent, marked internal, laid out by
 * InitHeap with the given master tables, and appended to the chain of heaps that starts at gMainHeap (which
 * FindHeap walks). Its end is the chunk's: the size and the header's room, rounded up to 32 as the chunk's data
 * is. Returns the new heap; destroy_heap gives it back. */
void *create_heap(tHeap *parent, u32 size, u16 numInitialMasters, u16 numAdditionalMasters)
{
    tHeap *heap;
    tHeap *other;

    osRecvMesg(&heap_lock_queue, 0, 1);
    heap = AllocateMemory(ConvertHeap(parent), size + HEAP_HEADER_ROOM, NULL, 1);
    heap->endRam = (u8 *)heap + ((size + HEAP_HEADER_ROOM + 31) & ~31);
    InitHeap(heap, numInitialMasters, numAdditionalMasters);
    for (other = gMainHeap; other != NULL; other = other->next) {
        if (other->next == NULL) {
            other->next = heap;
            break;
        }
    }
    osSendMesg(&heap_lock_queue, 0, 0);
    return heap;
}

/* Puts the main heap over the memory the PC gives it for the console's RDRAM (pc_heap_memory: on the N64 everything
 * from the end of the game's BSS to the end of RDRAM): its header at the memory's first 32-byte boundary, its end at
 * the memory's end. Notes an Expansion Pak, more than 4 MiB of RDRAM, which changes what the game keeps loaded
 * (expansion_pak_present). The N64 named its heap's start (game_bss_end) and tested the heap's end against the end of
 * 4 MiB of RDRAM: the same heap and the same test. */
void locate_main_heap(void)
{
    tHeap *heap;
    u8 *memory;
    u32 size;

    memory = pc_heap_memory(osMemSize, &size);
    heap = (tHeap *)(((uintptr_t)memory + 31) & ~(uintptr_t)31);
    gMainHeap = heap;
    heap->endRam = memory + size;
    if (osMemSize > 0x400000) {
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
        osCreateMesgQueue(&heap_lock_queue, &heap_lock_msg, 1);
        osSendMesg(&heap_lock_queue, 0, 0);
    }
    locate_main_heap();
    InitHeap(gMainHeap, numInitialMasters, numAdditionalMasters); pc_heap_created(gMainHeap);
}
