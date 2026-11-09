#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

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
} Heap;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Block: (struct Block *)PC_N64P(x), enum n64p_Heap: (struct Heap *)PC_N64P(x), enum n64p_void: (void *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)
typedef struct OSMesgQueue OSMesgQueue;

VIEW(OSMesgQueue, heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view
s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);

Heap *ConvertHeap(Heap *heap);

void *AllocateMemoryHeapHigh(Heap *heap, u32 size)
{
    Block *n;
    Block *b;
    u32 total;
    u32 largest;
    u32 bs;

    osRecvMesg(&heap_lock_queue, 0, 1);
    total = 0;
    largest = 0;
    heap = ConvertHeap(heap);
    size = (size + 31) & ~31;
    for (b = N64P(heap->last); b != 0; b = N64P(b->prev)) {
        if (b->used == 0) {
            bs = b->size;
            total += bs;
            if (largest < bs) {
                largest = bs;
            }
            if (bs >= size) {
                break;
            }
        }
    }
    bs = b->size;
    if (bs - size >= 64) {
        n = (Block *)((u8 *)b + bs - size);
        bs = b->size;
        n->next = b->next;
        if (n->next != 0) {
            N64P(n->next)->prev = N64W(n);
        } else {
            heap->last = N64W(n);
        }
        n->prev = N64W(b);
        n->size = size;
        n->owner = 0;
        n->used = 1;
        n->tag = 0;
        n->pad16[0] = 0;
        n->magic = 0xFEDCBA98;
        b->next = N64W(n);
        b->size = b->size - size - 32;
        b = n;
    } else {
        b->owner = 0;
        b->used = 1;
        b->tag = 0;
    }
    osSendMesg(&heap_lock_queue, 0, 0);
    return (u8 *)b + 32;
}
