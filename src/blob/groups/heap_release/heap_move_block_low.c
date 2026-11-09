/* heap_move_block_low: move a heap block to the lowest free place it fits (MAT3 memsys.c; the heap's blocks are a
 * 32-byte header, FEDCBA98, then the data, linked by N64 addresses until the memory model's wave 3). */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

enum n64p_Block { n64p_Block_ };
enum n64p_Heap { n64p_Heap_ };
enum n64p_ptr { n64p_ptr_ };
typedef struct Block {
    /* 0x00 */ u32 magic;
    /* 0x04 */ enum n64p_Block next;
    /* 0x08 */ enum n64p_Block prev;
    /* 0x0C */ u32 size;
    /* 0x10 */ enum n64p_ptr owner;
    /* 0x14 */ s8 used;
    /* 0x15 */ s8 tag;
    /* 0x16 */ u8 count;
    /* 0x17 */ u8 pad17[9];
} Block; /* 0x20 */

typedef struct Heap {
    /* 0x00 */ u32 magic;
    /* 0x04 */ enum n64p_Heap next;
    /* 0x08 */ enum n64p_Block first;
    /* 0x0C */ enum n64p_Block last;
} Heap;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Block: (struct Block *)PC_N64P(x), enum n64p_Heap: (struct Heap *)PC_N64P(x), enum n64p_ptr: (void **)PC_N64P(x), default: (x))

typedef struct OSMesgQueue OSMesgQueue;

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);


/* Moves the handle-owned block at addr down to the first free block before it that is big enough (or that it adjoins):
 * the data is copied, the owner's handle pointed at the new place, the rest of the free block split off when it is
 * 64 bytes or more, and the old block freed. Nothing when no free place lies before it. Under the heap lock. Keeps
 * the heap's free space together (a ghost's data is moved low before a race, ReadGhostDataInternal). */
void heap_move_block_low(void *addr)
{
    Heap *heap;
    Block *old;
    Block *after;
    Block *n;
    Block *b;
    Block *next;
    u32 used;
    s32 adjacent;
    u32 size;
    s32 pad[7];

    osRecvMesg(&heap_lock_queue, 0, 1);
    heap = FindHeap(PC_N64A(addr));
    old = FindChunk(heap, PC_N64A(addr), 0);
    after = N64P(old->next);
    adjacent = 0;
    for (b = N64P(heap->first); b != 0; b = next) {
        if (b == old) {
            b = 0;
            break;
        }
        if (b->used != 0) {
            next = N64P(b->next);
        } else {
            if (b->size >= old->size) {
                break;
            }
            next = N64P(b->next);
            if (old == next) {
                adjacent = 1;
                break;
            }
        }
    }
    if (b == 0) {
        osSendMesg(&heap_lock_queue, 0, 0);
        return;
    }
    b->owner = old->owner;
    *N64P(b->owner) = (u8 *)b + 32;
    b->used = 1;
    b->tag = old->tag;
    b->count = old->count;
    size = old->size;
    old->owner = 0;
    game_memmove((u8 *)b + 32, (u8 *)old + 32, old->size);
    if (adjacent || b->size - size >= 64) {
        n = (Block *)((u8 *)b + size + 32);
        if (adjacent) {
            n->next = PC_N64W(after);
        } else {
            n->next = b->next;
        }
        if (n->next != 0) {
            N64P(n->next)->prev = PC_N64W(n);
        } else {
            heap->last = PC_N64W(n);
        }
        n->prev = PC_N64W(b);
        if (adjacent) {
            used = 0;
        } else {
            used = size + 32;
        }
        n->size = b->size - used;
        n->owner = 0;
        n->used = adjacent;
        n->tag = 0;
        n->count = 0;
        n->magic = 0xFEDCBA98;
        b->next = PC_N64W(n);
        b->size = size;
        if (adjacent) {
            old = n;
        }
    }
    DeallocateMemoryInternal(PC_N64A((u8 *)old + 32), 0);
    osSendMesg(&heap_lock_queue, 0, 0);
}
