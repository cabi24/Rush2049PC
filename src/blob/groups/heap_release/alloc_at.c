#include "game/game.h"

enum n64p_Block { n64p_Block_ };
enum n64p_Heap { n64p_Heap_ };
enum n64p_void { n64p_void_ };
typedef struct Block {
    /* 0x00 */ u32 magic;
    /* 0x04 */ enum n64p_Block next;
    /* 0x08 */ enum n64p_Block prev;
    /* 0x0C */ u32 size;
    /* 0x10 */ enum n64p_void owner;
    /* 0x14 */ s8 used;
    /* 0x15 */ u8 tag;
    /* 0x16 */ u8 pad16[10];
} Block; /* 0x20 */

typedef struct Heap {
    /* 0x00 */ u32 magic;
    /* 0x04 */ enum n64p_Heap next;
    /* 0x08 */ enum n64p_Block first;
    /* 0x0C */ enum n64p_Block last;
    /* 0x10 */ u32 end;
} Heap;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Block: (struct Block *)PC_N64P(x), enum n64p_Heap: (struct Heap *)PC_N64P(x), enum n64p_void: (void *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)

/* PC: the block taken is reported (port/ultra/heap.c, docs/port/MEMORY.md 4). */

VIEW(s32, heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view

s32 osRecvMesg(void *mq, void **msg, s32 flags);
s32 osSendMesg(void *mq, void *msg, s32 flags);

/* The block's place is a 32-bit N64 address: N64A(p) is a window pointer's, N64PTR(T, a) the pointer at one
 * (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
#define N64PTR(T, a) ((T)PC_N64P(a))
void *allocate_memory_at(u32 addr, u32 size) {
    Heap *heap;
    Block *b;
    Block *n;
    u32 before;
    Block *prev;
    s32 unused1;
    s32 unused2;
    s32 unused3;
    s32 unused4;
    s32 unused5;
    void *result;

    osRecvMesg(&heap_lock_queue, 0, 1);
    heap = FindHeap(addr);
    size = (size + 31) & ~31;
    for (b = N64P(heap->first); b != 0; b = N64P(b->next)) {
        if (addr >= N64A(b) && (addr < (u32) b->next || b->next == 0)) {
            if (N64A(b) + b->size + 32 < addr + size) {
            }
            break;
        }
    }
    result = (u8 *) b + 32;
    if (addr != N64A(result)) {
        n = N64PTR(Block *, (addr - 32));
        n->next = b->next;
        if (n->next != 0) {
            N64P(n->next)->prev = N64W(n);
        } else {
            heap->last = N64W(n);
        }
        n->owner = 0;
        n->used = 1;
        n->tag = 0;
        n->pad16[0] = 0;
        n->magic = 0xFEDCBA98;
        before = addr - N64A(b) - 32;
        n->size = b->size - before;
        if (before >= 64) {
            n->prev = N64W(b);
            b->next = N64W(n);
            b->size = b->size - n->size - 32;
        } else {
            prev = N64P(b->prev);
            n->prev = N64W(prev);
            N64P(b->prev)->next = N64W(n);
            prev = N64P(b->prev);
            prev->size = prev->size + addr - N64A(b) - 32;
        }
        b = n;
        result = (u8 *) b + 32;
    }
    if (b->size - size >= 64) {
        n = (Block *) ((u8 *) b + size + 32);
        n->next = b->next;
        if (n->next != 0) {
            N64P(n->next)->prev = N64W(n);
        } else {
            heap->last = N64W(n);
        }
        n->prev = N64W(b);
        n->size = b->size - size - 32;
        n->owner = 0;
        n->used = 0;
        n->tag = 0;
        n->pad16[0] = 0;
        n->magic = 0xFEDCBA98;
        b->next = N64W(n);
        b->size = size;
    }
    b->owner = 0;
    b->used = 1;
    b->tag = 0; pc_heap_allocated(heap);
    osSendMesg(&heap_lock_queue, 0, 0);
    return result;
}
