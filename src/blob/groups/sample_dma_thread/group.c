/* sample_dma_thread/group.c: the sample DMA thread of the sound library (N64 only).
 *
 * It runs the sample DMA list (sample_dma_next.c): each time a cartridge read completes it starts the next, and
 * after the last it marks the list inactive and signals the list's done queue. */
#include "game/game.h"

/* PC: the DMA request list has its own storage, and its done queue follows the 8-byte request pointer
 * (sample_dma_next.c's DmaList, docs/port/MUSYX_DATA.md 8.1). The fields used here: */
typedef struct SampleDmaList {
    s8 active;          /* its first byte: the list is running */
    u8 pad1[0x10 - 1];
    OSMesgQueue *done;  /* +0x10 signalled when the last request is done */
} SampleDmaList;
#define sample_dma_list pc_sample_dma_list
extern SampleDmaList sample_dma_list;
/* Its done field, read by byte offset as the decompiled code does (a plain field read gives other unoptimised
 * code). */
#define DMA_LIST_DONE M2C_FIELD(&sample_dma_list, OSMesgQueue **, 0x10)

/* The thread's body (arg unused): make the cartridge-read queue (one message), mark the list inactive, then forever:
 * wait for a read to complete and start the list's next; when none is left, mark the list inactive and signal its
 * done queue. */
void sample_dma_thread_entry(void *arg) {
    queue_init(&sample_dma_pi_queue, &sample_dma_pi_msg, 1);
    sample_dma_list.active = 0;
    for (;;) {
        queue_recv(&sample_dma_pi_queue);
        if (sample_dma_next() == 0) {
            sample_dma_list.active = 0;
            queue_send(DMA_LIST_DONE, NULL);
        }
    }
}
