/* sample_dma_thread/group.c: the sample DMA thread of the sound library (N64 only).
 *
 * It runs the sample DMA list (sample_dma_next.c): each time a cartridge read completes it starts the next, and
 * after the last it marks the list inactive and signals the list's done queue. */
#include "game/game.h"
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */
#define OSThread_s OSThread_sView   /* this file's own OSThread_s: include/game's OSThread_s is another type */
#define __OSThreadContext __OSThreadContextView   /* this file's own __OSThreadContext: include/game's __OSThreadContext is another type */

typedef struct __OSThreadContext {
    u64 at, v0, v1, a0, a1, a2, a3;
    u64 t0, t1, t2, t3, t4, t5, t6, t7;
    u64 s0, s1, s2, s3, s4, s5, s6, s7;
    u64 t8, t9, gp, sp, s8, ra;
    u64 lo, hi;
    u32 sr, pc, cause, badvaddr, rcp;
    u32 fpcsr;
    f32 fp0, fp2, fp4, fp6, fp8, fp10, fp12, fp14;
    f32 fp16, fp18, fp20, fp22, fp24, fp26, fp28, fp30;
} __OSThreadContext;
typedef struct OSThread_s {
    struct OSThread_s *next;
    s32 priority;
    struct OSThread_s **queue;
    struct OSThread_s *tlnext;
    u16 state;
    u16 flags;
    s32 id;
    s32 fp;
    struct __OSThreadprofile_s *thprof;
    __OSThreadContext context;
} OSThread;

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);

s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);

VIEW(s32, sample_dma_pi_queue);
#define sample_dma_pi_queue sample_dma_pi_queue_view

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
void sample_dma_thread_entry(intptr_t arg) {
    osCreateMesgQueue((OSMesgQueue *) &sample_dma_pi_queue, (void **) &sample_dma_pi_msg, 1);
    sample_dma_list.active = 0;
    for (;;) {
        osRecvMesg((OSMesgQueue *) &sample_dma_pi_queue, NULL, 1);
        if (sample_dma_next() == 0) {
            sample_dma_list.active = 0;
            osSendMesg(DMA_LIST_DONE, NULL, 1);
        }
    }
}
