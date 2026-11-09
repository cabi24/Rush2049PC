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

s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);

VIEW(s32, musyx_task_done_queue);
#define musyx_task_done_queue musyx_task_done_queue_view

void musyx_wait_audio_task(void) {
    osRecvMesg((OSMesgQueue *) &musyx_task_done_queue, NULL, 1);
}
/* Warning: struct __OSThreadprofile_s is not defined (only forward-declared) */
