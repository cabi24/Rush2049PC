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
 * (sample_dma_next.c, docs/port/MUSYX_DATA.md 8.1) */
#define sample_dma_list pc_sample_dma_list
#define DMA_DONE_OFF 0x10
extern s32 sample_dma_list;

void sample_dma_thread_entry(intptr_t arg0) {
    osCreateMesgQueue((OSMesgQueue *) &sample_dma_pi_queue, (void **) &sample_dma_pi_msg, 1);
    M2C_FIELD(&sample_dma_list, s8 *, 0) = 0;
    for (;;) {
        osRecvMesg((OSMesgQueue *) &sample_dma_pi_queue, NULL, 1);
        if (sample_dma_next() == 0) {
            M2C_FIELD(&sample_dma_list, s8 *, 0) = 0;
            osSendMesg(M2C_FIELD(&sample_dma_list, OSMesgQueue **, DMA_DONE_OFF), NULL, 1);
        }
    }
}

