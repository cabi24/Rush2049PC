#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;
s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);

VIEW(OSMesgQueue, heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view

enum n64p_Heap { n64p_Heap_ };
typedef struct Heap {
    u32 pad0;
    enum n64p_Heap next;  /* 4 */
} Heap;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Heap: (struct Heap *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)

Heap *ConvertHeap(Heap *heap);

/* The heap's API takes N64 addresses: N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
void destroy_heap(Heap *heap)
{
    Heap *p;
    Heap *h;

    if (heap == NULL) {
    }
    osRecvMesg(&heap_lock_queue, NULL, 1);
    h = ConvertHeap(heap);
    for (p = gMainHeap; p != NULL; p = N64P(p->next)) {
        if (N64P(p->next) == h) {
            p->next = h->next;
            break;
        }
    }
    DeallocateMemoryInternal(N64A(h), 1);
    osSendMesg(&heap_lock_queue, NULL, 0);
}
