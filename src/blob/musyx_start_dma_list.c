/* musyx_start_dma_list: the MusyX sound library's service that starts a list of sample DMAs (one of its eight
 * services, docs/code/r5.md "MusyX"; N64 only, no MAT3 counterpart). */
#include "game/game.h"

/* The DMA list in progress (pc_sample_dma_list, sample_dma_next.c's DmaList). PC: the requests and the done queue
 * are pointers, in the list's own storage (docs/port/MUSYX_DATA.md 8.1). */
typedef struct DmaListView {
    s8 active;              /* 0x00 a list is in progress */
    s8 pad1;
    s16 count;              /* 0x02 its requests */
    s16 next;               /* 0x04 the next request sample_dma_next starts */
    s16 pad6;
    intptr_t reqs;          /* 0x08 the requests (size, cartridge address, buffer) */
    intptr_t done;          /* 0x10 the queue signalled when they are done */
} DmaListView;
#define sample_dma_list pc_sample_dma_list
extern DmaListView sample_dma_list;

/* Start the library's list of `count` sample DMAs (reqs), which signals doneQueue when done: record it as the
 * list in progress and start its first transfer (sample_dma_next walks the rest). An empty list does nothing.
 * Always 1. */
s32 musyx_start_dma_list(intptr_t reqs, s32 count, intptr_t doneQueue) {
    if (count != 0) {
        sample_dma_list.reqs = reqs;
        sample_dma_list.count = count;
        sample_dma_list.done = doneQueue;
        sample_dma_list.next = 0;
        sample_dma_list.active = 1;
        sample_dma_next();
    }
    return 1;
}
