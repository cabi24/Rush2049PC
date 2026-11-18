/* sample_dma_next/sample_dma_next.c: start the next DMA of the sound library's sample list (N64 only).
 *
 * The MusyX sound library streams samples from the cartridge by lists of DMA requests: musyx_start_dma_list fills
 * the list (sample_dma_list) and starts its first request, the sample DMA thread (sample_dma_thread_entry) starts
 * the next each time one completes and signals the list's done queue after the last. */
#include "game/game.h"
#define OSIoMesg OSIoMesgView   /* this file's own OSIoMesg: include/game's OSIoMesg is another type */
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;
typedef struct OSIoMesg OSIoMesg;

/* PC: the list (game/audio.h's SampleDmaList) also holds the done queue musyx_start_dma_list stores at +0xC on the
 * N64. With two host pointers it is 24 bytes, and the N64's 16 bytes before gPathWave can't hold it, so it has its
 * own storage, defined here (docs/port/MUSYX_DATA.md 8.1). */
SampleDmaList pc_sample_dma_list;

s32 osPiStartDma(OSIoMesg *mb, s32 priority, s32 direction, u32 devAddr, void *vAddr, u32 nbytes, OSMesgQueue *mq);

/* Start the list's next request (a cartridge read, its completion to sample_dma_pi_queue) and return 1, or 0 when
 * every request has been started. */
s32 sample_dma_next(void)
{
    if (pc_sample_dma_list.next < pc_sample_dma_list.count) {
        pc_sample_dma_list.next++;
        osPiStartDma(&sample_dma_io, 1, 0, pc_sample_dma_list.reqs[pc_sample_dma_list.next - 1].devAddr,
                          pc_sample_dma_list.reqs[pc_sample_dma_list.next - 1].vAddr,
                          pc_sample_dma_list.reqs[pc_sample_dma_list.next - 1].size, &sample_dma_pi_queue);
        return 1;
    }
    return 0;
}
