/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef unsigned int u32;
typedef int s32;
typedef signed char s8;

enum n64p_Block { n64p_Block_ };
enum n64p_Pool { n64p_Pool_ };
enum n64p_u32 { n64p_u32_ };
typedef struct Block {
    u32 pad0;
    enum n64p_Block next;
    u32 pad8[3];
    s8 pad14;
    s8 tag;
} Block;
typedef struct Pool {
    s32 count;
    enum n64p_u32 base;
    enum n64p_Pool next;
} Pool;
typedef struct Heap {
    u32 pad0;
    u32 pad4;
    enum n64p_Block blocks;
    u32 padc[3];
    Pool pool;
} Heap;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Block: (struct Block *)PC_N64P(x), enum n64p_Pool: (struct Pool *)PC_N64P(x), enum n64p_u32: (u32 *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)
/* The heap's addresses are 32-bit N64 addresses: N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md
 * 2.3). The pool is the heap's handle table, whose slots are 8 bytes on the PC (HANDLE_SLOT, memsys/group.c): an
 * address inside it is a handle, the slot read as one (PC_SLOT_N64). */
#define N64A(x) PC_N64A(x)
#define POOL_END(p) N64A((uintptr_t *)N64P((p)->base) + (p)->count)
#define SLOT_N64(a) PC_SLOT_N64(PC_N64P(a))
Block *FindChunk(Heap *heap, u32 addr, s32 tag) {
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
        if (tag != b->tag || addr < N64A(b) || (b->next != 0 && addr >= (u32)b->next)) {
            b = N64P(b->next);
        } else break;
    }
    return b;
}
