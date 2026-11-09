/* FindChunk.c: the heap block an address (or a handle) is in (MAT3 memsys.c, _FindChunk(), whose GameCube heap
 * differs).
 *
 * The N64 heap is a list of blocks, each tagged with its category; a heap also has handle pools, tables of slots
 * each holding the address of a movable block (the pointer counters and the purgeable memory use them). */
#include "game/game.h"

enum n64p_Block { n64p_Block_ };
enum n64p_Pool { n64p_Pool_ };
enum n64p_u32 { n64p_u32_ };
/* A heap block's header, the fields used here. */
typedef struct Block {
    u32 pad0;
    enum n64p_Block next;   /* the next block (its N64 address); the block ends where it begins */
    u32 pad8[3];
    s8 pad14;
    s8 tag;                 /* its category */
} Block;
/* A handle pool: count slots from base, then the next pool. */
typedef struct Pool {
    s32 count;
    enum n64p_u32 base;
    enum n64p_Pool next;
} Pool;
typedef struct Heap {
    u32 pad0;
    u32 pad4;
    enum n64p_Block blocks; /* its first block */
    u32 padc[3];
    Pool pool;              /* its first handle pool */
} Heap;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Block: (struct Block *)PC_N64P(x), enum n64p_Pool: (struct Pool *)PC_N64P(x), enum n64p_u32: (u32 *)PC_N64P(x), default: (x))
/* The heap's addresses are 32-bit N64 addresses: PC_N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md
 * 2.3). The pool is the heap's handle table, whose slots are 8 bytes on the PC (HANDLE_SLOT, memsys/group.c): an
 * address inside it is a handle, the slot read as one (PC_SLOT_N64). */
#define POOL_END(p) PC_N64A((uintptr_t *)N64P((p)->base) + (p)->count)
#define SLOT_N64(a) PC_SLOT_N64(PC_N64P(a))

/* The block of `heap` with category `tag` that holds the N64 address addr, or NULL. An address inside one of the
 * heap's handle pools is a handle: the address its slot holds is looked up instead. A block holds the addresses
 * from its header to the next block's (the last block, everything after it). */
struct tMemoryChunk *FindChunk(Heap *heap, u32 addr, s32 tag) {
    Pool *p;
    Block *b;
    p = &heap->pool;
    while (1) {
        if (p == 0 || p->base == 0) break;
        if (addr >= (u32)p->base && addr < POOL_END(p)) {
            addr = SLOT_N64(addr);
            break;
        }
        p = N64P(p->next);
    }
    b = N64P(heap->blocks);
    while (b != 0) {
        if (tag != b->tag || addr < PC_N64A(b) || (b->next != 0 && addr >= (u32)b->next)) {
            b = N64P(b->next);
        } else break;
    }
    return (struct tMemoryChunk *)b;   /* a chunk (game/heap.h's tMemoryChunk), read here as a Block */
}
