/* Freeing and resizing heap chunks (MAT3 memsys.c _DeallocateMemory, DeallocateMemory, ReallocateMemory), and the
 * small functions that free one known block: the runtime image (overlay), the sound library's blocks, the 3D sound
 * tables. include/game/heap.h has the headers; memsys/group.c allocates. */
#include "game/game.h"
#include "game/heap.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;

s32 osRecvMesg(OSMesgQueue *,void **,s32);s32 osSendMesg(OSMesgQueue *,void *,s32);

/* PC: the free's first fill poisons the freed data (chunk + 32, size). Whatever the asset layer had registered in
 * it in big-endian form (a wave's file) goes with it (port/assets). The other two fills clear merged headers. */
void pc_heap_block_freed(void *p, __SIZE_TYPE__ size);
#define FillFreeMem(p, n) (pc_heap_block_freed(p, n), FillFreeMem(p, n))

/* Frees the chunk whose data is at ptr (or the handle's chunk, for a master slot's address): fills the data, clears
 * its master slot if it has one, and merges it with a free chunk above and then a free chunk below. internal says
 * whether the caller frees one of the heap's own chunks (destroy_heap: 1) or an ordinary one (0). The caller holds
 * the heap lock (MAT3 _DeallocateMemory). */
void DeallocateMemoryInternal(void *ptr, s32 internal) {
    tHeap *heap;
    tMemoryChunk *chunk, *next, *prev;

    heap = FindHeap(ptr);
    chunk = FindChunk(heap, ptr, internal);
    FillFreeMem((u8 *)chunk + HEAP_CHUNK_HEADER, chunk->size);
    if (chunk->master) *chunk->master = NULL;
    next = chunk->next;
    if (next && next->used == 0) {
        chunk->next = next->next;
        if (chunk->next) chunk->next->prev = chunk;
        else heap->lastChunk = chunk;
        chunk->size += next->size + HEAP_CHUNK_HEADER;
        FillFreeMem(next, HEAP_CHUNK_HEADER);
    }
    prev = chunk->prev;
    chunk->master = NULL; chunk->used = 0; chunk->internal = 0; chunk->locked = 0;
    if (prev && prev->used == 0) {
        prev->next = chunk->next;
        if (prev->next) prev->next->prev = prev;
        else heap->lastChunk = prev;
        prev->size += chunk->size + HEAP_CHUNK_HEADER;
        FillFreeMem(chunk, HEAP_CHUNK_HEADER);
    }
}
#undef FillFreeMem

/* Frees the chunk whose data is at ptr, under the heap lock (MAT3 DeallocateMemory). */
__inline void DeallocateMemory(void *ptr) {
    osRecvMesg(&heap_lock_queue, NULL, 1);
    DeallocateMemoryInternal(ptr, 0);
    osSendMesg(&heap_lock_queue, NULL, 0);
}

/* The sound library's free (its allocator hook), the same as DeallocateMemory. */
void musyx_free(void *ptr) {
    osRecvMesg(&heap_lock_queue, NULL, 1);
    DeallocateMemoryInternal(ptr, 0);
    osSendMesg(&heap_lock_queue, NULL, 0);
}

/* Frees runtime image B (the race extras) if it is loaded: its block (runtime_image_block). */
void unload_extra_overlay(void) {
    if (extra_overlay_loaded) {
        osRecvMesg(&heap_lock_queue, NULL, 1);
        DeallocateMemoryInternal(runtime_image_block(), 0);
        osSendMesg(&heap_lock_queue, NULL, 0);
        extra_overlay_loaded = 0;
    }
}

/* Frees runtime image A (the front end) if it is loaded. Nothing calls it: LoadFrontEndOverlay (state.c) frees the
 * image itself. */
void unload_start_overlay(void) {
    if (start_overlay_loaded) {
        osRecvMesg(&heap_lock_queue, NULL, 1);
        DeallocateMemoryInternal(runtime_image_block(), 0);
        osSendMesg(&heap_lock_queue, NULL, 0);
        start_overlay_loaded = 0;
    }
}

/* Resizes the chunk whose data is at ptr to desired bytes (rounded up to 32), in place, under the heap lock
 * (MAT3 ReallocateMemory and _ReallocateMemory):
 *   smaller, and the chunk above is missing or in use: splits off the rest as a free chunk if it is at least
 *     HEAP_MIN_SPLIT bytes;
 *   smaller, and the chunk above is free: moves that chunk's header down, growing it by what is given up, if that
 *     is at least 32 bytes;
 *   larger: takes from the chunk above (whole if less than a header would be left, else moving its header up).
 * Original game: growing takes the chunk above without checking that there is one, or that it is free and large
 * enough.
 * PC: a chunk that grows takes from the heap: reported as an allocation (port/ultra/heap.c). */
void ReallocateMemory(void *ptr, u32 desired) {
    tHeap *heap;
    tMemoryChunk *chunk;
    tMemoryChunk *next;
    tMemoryChunk *moved;
    u32 size;
    u32 nextSize;

    osRecvMesg(&heap_lock_queue, NULL, 1);
    desired = (desired + 31) & ~31U;
    heap = FindHeap(ptr);
    chunk = FindChunk(heap, ptr, 0);
    size = chunk->size;
    if (size >= desired) {
        next = chunk->next;
        if (chunk->next == NULL || next->used != 0) {
            if (size - desired >= HEAP_MIN_SPLIT) {
                moved = (tMemoryChunk *)((u8 *)chunk + desired + HEAP_CHUNK_HEADER);
                moved->next = chunk->next;
                if (moved->next != NULL) {
                    moved->next->prev = moved;
                } else {
                    heap->lastChunk = moved;
                }
                moved->prev = chunk;
                moved->size = chunk->size - desired - HEAP_CHUNK_HEADER;
                moved->master = NULL;
                moved->used = 0;
                moved->internal = 0;
                moved->locked = 0;
                moved->magic = HEAP_CHUNK_MAGIC;
                chunk->next = moved;
                chunk->size = desired;
            }
        } else {
            if (size - desired >= HEAP_CHUNK_HEADER) {
                moved = (tMemoryChunk *)((u8 *)chunk + desired + HEAP_CHUNK_HEADER);
                moved->next = next->next;
                if (moved->next != NULL) {
                    moved->next->prev = moved;
                } else {
                    heap->lastChunk = moved;
                }
                moved->prev = chunk;
                moved->size = chunk->size - desired + next->size;
                moved->master = NULL;
                moved->used = 0;
                moved->internal = 0;
                moved->locked = 0;
                moved->magic = HEAP_CHUNK_MAGIC;
                chunk->next = moved;
                chunk->size = desired;
            }
        }
    } else {
        next = chunk->next;
        nextSize = next->size;
        if (nextSize - desired + size < HEAP_CHUNK_HEADER) {
            chunk->next = next->next;
            if (chunk->next != NULL) {
                chunk->next->prev = chunk;
            } else {
                heap->lastChunk = chunk;
            }
            desired = chunk->size + next->size + HEAP_CHUNK_HEADER;
        } else {
            moved = (tMemoryChunk *)((u8 *)chunk + desired + HEAP_CHUNK_HEADER);
            moved->next = next->next;
            if (moved->next != NULL) {
                moved->next->prev = moved;
            } else {
                heap->lastChunk = moved;
            }
            moved->prev = chunk;
            moved->size = chunk->size + next->size - desired;
            moved->master = NULL;
            moved->used = 0;
            moved->internal = 0;
            moved->locked = 0;
            moved->magic = HEAP_CHUNK_MAGIC;
            chunk->next = moved;
        }
        chunk->size = desired;
    }
    pc_heap_allocated(heap); osSendMesg(&heap_lock_queue, NULL, 0);
}

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
