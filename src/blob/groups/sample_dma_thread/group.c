/* sample_dma_thread/group.c: the sample DMA thread of the sound library (N64 only).
 *
 * It runs the sample DMA list (sample_dma_next.c): each time a cartridge read completes it starts the next, and
 * after the last it marks the list inactive and signals the list's done queue. */
#include "game/game.h"

/* The thread's body (arg unused): make the cartridge-read queue (one message), mark the list (pc_sample_dma_list,
 * game/audio.h) inactive, then forever: wait for a read to complete and start the list's next; when none is left,
 * mark the list inactive and signal its done queue. */
void sample_dma_thread_entry(void *arg) {
    queue_init(&sample_dma_pi_queue, &sample_dma_pi_msg, 1);
    pc_sample_dma_list.active = 0;
    for (;;) {
        queue_recv(&sample_dma_pi_queue);
        if (sample_dma_next() == 0) {
            pc_sample_dma_list.active = 0;
            queue_send(pc_sample_dma_list.done, NULL);
        }
    }
}
