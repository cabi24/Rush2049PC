/* Freeing and resizing heap chunks (MAT3 memsys.c _DeallocateMemory, DeallocateMemory, ReallocateMemory), and the
 * small functions that free one known block: the runtime image (overlay), a car's palette, the 3D sound tables.
 * include/game/heap.h has the headers; memsys/group.c allocates. */
#include "game/game.h"
#include "game/heap.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

/* A chunk's master is the handle slot that holds its data's address (AllocateHandleHeap): freeing the chunk clears
 * the slot (PC_SLOT_SET writes a whole pointer-sized slot). */
#define SLOT_CLEAR(o) PC_SLOT_SET(PC_N64P(o), 0)
typedef struct OSMesgQueue OSMesgQueue;

VIEW(s32, heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view

extern u8 overlay_base[];   /* where the runtime image (front end A or race extras B) is loaded: its heap chunk */
s32 osRecvMesg(OSMesgQueue *,void **,s32);s32 osSendMesg(OSMesgQueue *,void *,s32);

/* PC: the free's first fill poisons the freed data (chunk + 32, size). Whatever the asset layer had registered in
 * it in big-endian form (a wave's file) goes with it (port/assets). The other two fills clear merged headers. */
void pc_heap_block_freed(void *p, __SIZE_TYPE__ size);
#define FillFreeMem(p, n) (pc_heap_block_freed(p, n), FillFreeMem(p, n))

/* Frees the chunk whose data is at address (a window address): fills the data, clears its master slot if it has
 * one, and merges it with a free chunk above and then a free chunk below. internal says whether the caller frees
 * one of the heap's own chunks (destroy_heap: 1) or an ordinary one (0). The caller holds the heap lock
 * (MAT3 _DeallocateMemory). */
void DeallocateMemoryInternal(u32 address, s32 internal) {
    tHeap *heap;
    tMemoryChunk *chunk, *next, *prev;

    heap = FindHeap(address);
    chunk = FindChunk(heap, address, internal);
    FillFreeMem((u8 *)chunk + HEAP_CHUNK_HEADER, chunk->size);
    if (chunk->master) SLOT_CLEAR(chunk->master);
    next = N64P(chunk->next);
    if (next && next->used == 0) {
        chunk->next = next->next;
        if (chunk->next) N64P(chunk->next)->prev = PC_N64A(chunk);
        else heap->lastChunk = PC_N64A(chunk);
        chunk->size += next->size + HEAP_CHUNK_HEADER;
        FillFreeMem(next, HEAP_CHUNK_HEADER);
    }
    prev = ((tMemoryChunk *)PC_N64P(chunk->prev));
    chunk->master = 0; chunk->used = 0; chunk->internal = 0; chunk->locked = 0;
    if (prev && prev->used == 0) {
        prev->next = chunk->next;
        if (prev->next) N64P(prev->next)->prev = PC_N64A(prev);
        else heap->lastChunk = PC_N64A(prev);
        prev->size += chunk->size + HEAP_CHUNK_HEADER;
        FillFreeMem(chunk, HEAP_CHUNK_HEADER);
    }
}
#undef FillFreeMem

/* Frees the chunk whose data is at address (a window address), under the heap lock (MAT3 DeallocateMemory). */
__inline void DeallocateMemory(u32 address) {
    osRecvMesg((OSMesgQueue *)&heap_lock_queue, NULL, 1);
    DeallocateMemoryInternal(address, 0);
    osSendMesg((OSMesgQueue *)&heap_lock_queue, NULL, 0);
}

/* The sound library's free (its allocator hook), the same as DeallocateMemory. */
void musyx_free(u32 address) {
    osRecvMesg((OSMesgQueue *)&heap_lock_queue, NULL, 1);
    DeallocateMemoryInternal(address, 0);
    osSendMesg((OSMesgQueue *)&heap_lock_queue, NULL, 0);
}

/* Frees runtime image B (the race extras) if it is loaded. */
void unload_extra_overlay(void) {
    if (extra_overlay_loaded) {
        osRecvMesg((OSMesgQueue *)&heap_lock_queue, NULL, 1);
        DeallocateMemoryInternal(PC_N64A(overlay_base), 0);
        osSendMesg((OSMesgQueue *)&heap_lock_queue, NULL, 0);
        extra_overlay_loaded = 0;
    }
}

/* Frees runtime image A (the front end) if it is loaded. Nothing calls it: LoadFrontEndOverlay (state.c) frees the
 * image itself. */
void unload_start_overlay(void) {
    if (start_overlay_loaded) {
        osRecvMesg((OSMesgQueue *)&heap_lock_queue, NULL, 1);
        DeallocateMemoryInternal(PC_N64A(overlay_base), 0);
        osSendMesg((OSMesgQueue *)&heap_lock_queue, NULL, 0);
        start_overlay_loaded = 0;
    }
}

/* Frees the heap block of car palette index (car_palettes[index].data, its window address). */
void free_car_palette(s32 index) {
    u32 address = car_palettes[index].data;

    osRecvMesg((OSMesgQueue *)&heap_lock_queue, NULL, 1);
    DeallocateMemoryInternal(address, 0);
    osSendMesg((OSMesgQueue *)&heap_lock_queue, NULL, 0);
}

/* Resizes the chunk whose data is at address to desired bytes (rounded up to 32), in place, under the heap lock
 * (MAT3 ReallocateMemory and _ReallocateMemory):
 *   smaller, and the chunk above is missing or in use: splits off the rest as a free chunk if it is at least
 *     HEAP_MIN_SPLIT bytes;
 *   smaller, and the chunk above is free: moves that chunk's header down, growing it by what is given up, if that
 *     is at least 32 bytes;
 *   larger: takes from the chunk above (whole if less than a header would be left, else moving its header up).
 * Original game: growing takes the chunk above without checking that there is one, or that it is free and large
 * enough.
 * PC: a chunk that grows takes from the heap: reported as an allocation (port/ultra/heap.c). */
void ReallocateMemory(u32 address, u32 desired) {
    tHeap *heap;
    tMemoryChunk *chunk;
    tMemoryChunk *next;
    tMemoryChunk *moved;
    u32 size;
    u32 nextSize;

    osRecvMesg((OSMesgQueue *)&heap_lock_queue, NULL, 1);
    desired = (desired + 31) & ~31U;
    heap = (tHeap *)FindHeap(address);
    chunk = FindChunk(heap, address, 0);
    size = chunk->size;
    if (size >= desired) {
        next = N64P(chunk->next);
        if (N64P(chunk->next) == NULL || next->used != 0) {
            if (size - desired >= HEAP_MIN_SPLIT) {
                moved = (tMemoryChunk *)((u8 *)chunk + desired + HEAP_CHUNK_HEADER);
                moved->next = chunk->next;
                if (N64P(moved->next) != NULL) {
                    N64P(moved->next)->prev = PC_N64W(moved);
                } else {
                    heap->lastChunk = PC_N64W(moved);
                }
                moved->prev = PC_N64W(chunk);
                moved->size = chunk->size - desired - HEAP_CHUNK_HEADER;
                moved->master = PC_N64W(NULL);
                moved->used = 0;
                moved->internal = 0;
                moved->locked = 0;
                moved->magic = HEAP_MAGIC_WORD;
                chunk->next = PC_N64W(moved);
                chunk->size = desired;
            }
        } else {
            if (size - desired >= HEAP_CHUNK_HEADER) {
                moved = (tMemoryChunk *)((u8 *)chunk + desired + HEAP_CHUNK_HEADER);
                moved->next = next->next;
                if (N64P(moved->next) != NULL) {
                    N64P(moved->next)->prev = PC_N64W(moved);
                } else {
                    heap->lastChunk = PC_N64W(moved);
                }
                moved->prev = PC_N64W(chunk);
                moved->size = chunk->size - desired + next->size;
                moved->master = PC_N64W(NULL);
                moved->used = 0;
                moved->internal = 0;
                moved->locked = 0;
                moved->magic = HEAP_MAGIC_WORD;
                chunk->next = PC_N64W(moved);
                chunk->size = desired;
            }
        }
    } else {
        next = N64P(chunk->next);
        nextSize = next->size;
        if (nextSize - desired + size < HEAP_CHUNK_HEADER) {
            chunk->next = next->next;
            if (N64P(chunk->next) != NULL) {
                N64P(chunk->next)->prev = PC_N64W(chunk);
            } else {
                heap->lastChunk = PC_N64W(chunk);
            }
            desired = chunk->size + next->size + HEAP_CHUNK_HEADER;
        } else {
            moved = (tMemoryChunk *)((u8 *)chunk + desired + HEAP_CHUNK_HEADER);
            moved->next = next->next;
            if (N64P(moved->next) != NULL) {
                N64P(moved->next)->prev = PC_N64W(moved);
            } else {
                heap->lastChunk = PC_N64W(moved);
            }
            moved->prev = PC_N64W(chunk);
            moved->size = chunk->size + next->size - desired;
            moved->master = PC_N64W(NULL);
            moved->used = 0;
            moved->internal = 0;
            moved->locked = 0;
            moved->magic = HEAP_MAGIC_WORD;
            chunk->next = PC_N64W(moved);
        }
        chunk->size = desired;
    }
    pc_heap_allocated(heap); osSendMesg((OSMesgQueue *)&heap_lock_queue, NULL, 0);
}

VIEW(uintptr_t, gListeners);
#define gListeners gListeners_view
VIEW(uintptr_t, gEmitters);
#define gEmitters gEmitters_view

/* Frees the 3D sound tables (the listeners and the emitters), if they are allocated (MAT3 audiomgr.c AudFree3D). */
void AudFree3D(void) {
    if (gListeners) {
        DeallocateMemory(gListeners);
        gListeners = 0;
    }
    if (gEmitters) {
        DeallocateMemory(gEmitters);
        gEmitters = 0;
    }
}
