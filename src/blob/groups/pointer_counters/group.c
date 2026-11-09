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
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);

VIEW(s32, heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view

void *FindChunk(void *,u32,s32);

void *GetPtrSize(u32 address) {
 void *owner,*block,*result;
 osRecvMesg((OSMesgQueue *)&heap_lock_queue,NULL,1);
 owner=FindHeap(address);
 block=FindChunk(owner,address,0);
 result=M2C_FIELD(block,void **,12);
 osSendMesg((OSMesgQueue *)&heap_lock_queue,NULL,0);
 return result;
}
void unlock_memory(u32 address) {
 void *owner,*block; s32 count;
 osRecvMesg((OSMesgQueue *)&heap_lock_queue,NULL,1);
 owner=FindHeap(address);
 block=FindChunk(owner,address,0);
 count=M2C_FIELD(block,u8 *,22);
 if(count>0)M2C_FIELD(block,u8 *,22)=count-1;
 osSendMesg((OSMesgQueue *)&heap_lock_queue,NULL,0);
}
void lock_memory(u32 address) {
 void *owner,*block; s32 count;
 osRecvMesg((OSMesgQueue *)&heap_lock_queue,NULL,1);
 owner=FindHeap(address);
 block=FindChunk(owner,address,0);
 count=M2C_FIELD(block,u8 *,22);
 if(count<255)M2C_FIELD(block,u8 *,22)=count+1;
 osSendMesg((OSMesgQueue *)&heap_lock_queue,NULL,0);
}
