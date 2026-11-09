/* destroy_heap (MAT3 memsys.c DestroyHeap): gives back a heap made by create_heap. include/game/heap.h has the
 * headers. */
#include "game/game.h"
#include "game/heap.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;
s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);

/* Unlinks heap from the chain of heaps that starts at gMainHeap and frees the chunk of its parent that holds it,
 * with everything still allocated in it. Takes the heap lock. */
void destroy_heap(tHeap *heap)
{
    tHeap *other;
    tHeap *target;

    osRecvMesg(&heap_lock_queue, NULL, 1);
    target = ConvertHeap(heap);
    for (other = gMainHeap; other != NULL; other = N64P(other->next)) {
        if (N64P(other->next) == target) {
            other->next = target->next;
            break;
        }
    }
    DeallocateMemoryInternal(PC_N64A(target), 1);
    osSendMesg(&heap_lock_queue, NULL, 0);
}
