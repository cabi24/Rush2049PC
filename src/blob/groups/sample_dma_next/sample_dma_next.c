/* sample_dma_next/sample_dma_next.c: start the next read of the sound library's sample list.
 *
 * The MusyX sound library streams samples from the cartridge by lists of reads: musyx_start_dma_list fills the list
 * (sample_dma_list) and starts its first request, the sample DMA thread (sample_dma_thread_entry) starts the next each
 * time one completes and signals the list's done queue after the last. */
#include "game/game.h"


/* PC: the list (game/audio.h's SampleDmaList) also holds the done queue musyx_start_dma_list stores at +0xC on the
 * N64. With two host pointers it is 24 bytes, and the N64's 16 bytes before gPathWave can't hold it, so it has its
 * own storage, defined here (docs/port/MUSYX_DATA.md 8.1). */
SampleDmaList pc_sample_dma_list;


/* Start the list's next request (a cartridge read, its completion to sample_dma_pi_queue) and return 1, or 0 when
 * every request has been started. */
s32 sample_dma_next(void)
{
    if (pc_sample_dma_list.next < pc_sample_dma_list.count) {
        pc_sample_dma_list.next++;
        pc_cart_read(pc_sample_dma_list.reqs[pc_sample_dma_list.next - 1].source,
                     pc_sample_dma_list.reqs[pc_sample_dma_list.next - 1].destination,
                     pc_sample_dma_list.reqs[pc_sample_dma_list.next - 1].length, &sample_dma_pi_queue);
        return 1;
    }
    return 0;
}
