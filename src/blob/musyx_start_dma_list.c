/* musyx_start_dma_list: the MusyX sound library's service that starts a list of sample DMAs (one of its eight
 * services, docs/code/r5.md "MusyX"; N64 only, no MAT3 counterpart). */
#include "game/game.h"

/* Start the library's list of `count` sample DMAs (reqs), which signals doneQueue when done: record it as the
 * list in progress (pc_sample_dma_list, game/audio.h) and start its first transfer (sample_dma_next walks the
 * rest). An empty list does nothing. Always 1. */
s32 musyx_start_dma_list(SampleDmaRequest *reqs, s32 count, OSMesgQueue *doneQueue) {
    if (count != 0) {
        pc_sample_dma_list.reqs = reqs;
        pc_sample_dma_list.count = count;
        pc_sample_dma_list.done = doneQueue;
        pc_sample_dma_list.next = 0;
        pc_sample_dma_list.active = 1;
        sample_dma_next();
    }
    return 1;
}
