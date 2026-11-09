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

u32 LargestAvailableMemoryHeapChunk(Heap *heap)
{
    u32 largest;

    osRecvMesg(&heap_lock_queue, 0, 1);
    largest = LargestAvailableMemoryChunk(heap);
    osSendMesg(&heap_lock_queue, 0, 0);
    return largest;
}

u32 LargestAvailableMemoryChunk(Heap *heap)
{
    u32 largest;
    Block *block;

    block = N64P(ConvertHeap(heap)->first);
    largest = 0;
    while (block != 0) {
        if (block->used == 0) {
            if (largest < block->size) {
                largest = block->size;
            }
        }
        block = N64P(block->next);
    }
    return largest;
}
