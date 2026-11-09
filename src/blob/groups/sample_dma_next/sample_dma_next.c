#include "game/game.h"
#define DmaList DmaListView   /* this file's own DmaList: include/game's DmaList is another type */
#define OSIoMesg OSIoMesgView   /* this file's own OSIoMesg: include/game's OSIoMesg is another type */
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;
typedef struct OSIoMesg OSIoMesg;

typedef struct DmaReq {
    u32 size;
    u32 devAddr;
    void *vAddr;
} DmaReq;

/* PC: the list also holds the done queue musyx_start_dma_list stores at +0xC on the N64. With two host
 * pointers it is 24 bytes, and 0x80153F10 has 16 before gPathWave, so it has its own storage, defined
 * here; musyx_start_dma_list.c and sample_dma_thread_entry use the same name and layout
 * (docs/port/MUSYX_DATA.md 8.1). */
typedef struct {
    u16 pad0;
    u16 count;
    u16 next;
    u16 pad6;
    DmaReq *reqs;
    OSMesgQueue *done;
} DmaList;
#define sample_dma_list pc_sample_dma_list
DmaList pc_sample_dma_list;

extern DmaList sample_dma_list;
VIEW(OSIoMesg, sample_dma_io);
#define sample_dma_io sample_dma_io_view
VIEW(OSMesgQueue, sample_dma_pi_queue);
#define sample_dma_pi_queue sample_dma_pi_queue_view
s32 osPiStartDma(OSIoMesg *mb, s32 priority, s32 direction, u32 devAddr, void *vAddr, u32 nbytes, OSMesgQueue *mq);

s32 sample_dma_next(void)
{
    if (sample_dma_list.next < sample_dma_list.count) {
        sample_dma_list.next++;
        osPiStartDma(&sample_dma_io, 1, 0, sample_dma_list.reqs[sample_dma_list.next - 1].devAddr,
                          sample_dma_list.reqs[sample_dma_list.next - 1].vAddr, sample_dma_list.reqs[sample_dma_list.next - 1].size,
                          &sample_dma_pi_queue);
        return 1;
    }
    return 0;
}
