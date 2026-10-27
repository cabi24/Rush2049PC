/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef unsigned int u32;
typedef int s32;
typedef signed char s8;
#ifndef TARGET_PC
typedef u32 uintptr_t;
typedef s32 intptr_t;
#endif

#ifdef TARGET_PC
enum n64p_Block { n64p_Block_ };
enum n64p_Pool { n64p_Pool_ };
enum n64p_u32 { n64p_u32_ };
#endif
#ifdef TARGET_PC
typedef struct Block {
    u32 pad0;
    enum n64p_Block next;
    u32 pad8[3];
    s8 pad14;
    s8 tag;
} Block;
#else
typedef struct Block {
    u32 pad0;
    struct Block *next;
    u32 pad8[3];
    s8 pad14;
    s8 tag;
} Block;
#endif
#ifdef TARGET_PC
typedef struct Pool {
    s32 count;
    enum n64p_u32 base;
    enum n64p_Pool next;
} Pool;
#else
typedef struct Pool {
    s32 count;
    u32 *base;
    struct Pool *next;
} Pool;
#endif
#ifdef TARGET_PC
typedef struct Heap {
    u32 pad0;
    u32 pad4;
    enum n64p_Block blocks;
    u32 padc[3];
    Pool pool;
} Heap;
#else
typedef struct Heap {
    u32 pad0;
    u32 pad4;
    Block *blocks;
    u32 padc[3];
    Pool pool;
} Heap;
#endif
#ifdef TARGET_PC
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Block: (struct Block *)(uintptr_t)(x), enum n64p_Pool: (struct Pool *)(uintptr_t)(x), enum n64p_u32: (u32 *)(uintptr_t)(x), default: (x))
#define N64W(x) PC_N64W(x)
#else
#define N64P(x) x
#define N64W(x) x
#endif
Block *func_80095EF4(Heap *heap, u32 addr, s32 tag) {
    Pool *p;
    Block *b;
    p = &heap->pool;
    while (1) {
        if (p == 0 || p->base == 0) break;
        if (addr >= (u32)p->base && addr < (u32)(N64P(p->base) + p->count)) {
            addr = *(u32 *)addr;
            break;
        }
        p = N64P(p->next);
    }
    b = N64P(heap->blocks);
    while (b != 0) {
        if (tag != b->tag || addr < (u32)b || (b->next != 0 && addr >= (u32)b->next)) {
            b = N64P(b->next);
        } else break;
    }
    return b;
}
