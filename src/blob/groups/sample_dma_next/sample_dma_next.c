/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * sample_dma_next (0x8008ABE4): start the next queued PI DMA. sample_dma_list is a request list
 * {u16 count @2; u16 next @4; DmaReq *reqs @8} of 12-byte {size, devAddr, vAddr} requests; if
 * next < count, advance next and start the DMA of the request just taken with the 7-argument
 * PI start call at 0x80008630 (historical label __osPiRawStartDma; the argument list is
 * osPiStartDma's: io message sample_dma_io, priority 1, direction 0 = read, done queue sample_dma_pi_queue).
 * Returns 1 if a DMA was started, else 0.
 *
 * Internal function (IPA): only reproduces with its real callers musyx_start_dma_list and
 * sample_dma_thread_entry in the -O3 unit (four-wide t6-t9 temp ring). Shaping: the request is
 * indexed as reqs[next - 1] after the increment and read field by field with no named pointer
 * (a `DmaReq *r` local adds an `addiu v0,v0,-12`); early returns, not a result variable.
 */
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef struct OSMesgQueue OSMesgQueue;
typedef struct OSIoMesg OSIoMesg;

typedef struct {
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
extern OSIoMesg sample_dma_io;
extern OSMesgQueue sample_dma_pi_queue;
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
