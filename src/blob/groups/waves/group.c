#include "game/game.h"
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */
#define OSThread_s OSThread_sView   /* this file's own OSThread_s: include/game's OSThread_s is another type */
#define __OSThreadContext __OSThreadContextView   /* this file's own __OSThreadContext: include/game's __OSThreadContext is another type */

/* : resource-loader records, hand-derived from the retail words */
typedef struct ResSlot {         /* gWaveInstance[], 0x14 bytes */
    s8 f0;
    s8 loaded;                   /* 0x01 */
    s8 f2;
    s8 f3;                       /* 0x03 */
    s8 f4;
    s8 f5;                       /* 0x05 */
    u8 type;                     /* 0x06 */
    u8 pad07;
    s32 pad08;
    u8 **p0C;                    /* 0x0C */
    u8 *w10;                     /* 0x10 */
} ResSlot;

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

extern void osSetThreadPri(OSThread *thread, OSPri priority);

VIEW(s32, bg_load_thread);
#define bg_load_thread bg_load_thread_view

VIEW(s32, bg_load_idle_queue);
#define bg_load_idle_queue bg_load_idle_queue_view

VIEW(u8, gNumModels);
#define gNumModels gNumModels_view

VIEW(s32, bg_load_request_queue);
#define bg_load_request_queue bg_load_request_queue_view

VIEW(ResSlot[], gWaveInstance);
#define gWaveInstance gWaveInstance_view

/* BGLoadWaveMain (0x800AC75C, 196 B; historical label) = BGLoadWaveMain@waves.c (GC: DWARF "Erased static void
 * BGLoadWaveMain()"; no Dreamcast counterpart): the N64 background wave-load thread (thread id 8, created by the
 * static main_thread_entry). It waits for LoadWave's background request (_BGLoadWave posts on bg_load_request_queue), reads the
 * requested wave (_ReadWave = ReadWave, whose IPA convention takes the index in s8 and leaves s0-s7 unsaved:
 * hence the nine saves) and signals completion on bg_load_idle_queue (InitWaves' inlined _BGLoadWaveCheck waits on it).
 * : promoted from context to member. Its unreachable epilogue after the endless loop is `.align 5`
 * padded, so the body only matches at its retail offset modulo 32: NewWorld.c (byte copy of the locked
 * single, retail 0x800AC668) is context that puts it there in this object; the unit needs an `align` override. */
void BGLoadWaveMain(void *arg) {
    OSMesg msg;

    osCreateMesgQueue((OSMesgQueue *) &bg_load_request_queue, (void **) &bg_load_request_messages, 1);
    osSetThreadPri((OSThread *) &bg_load_thread, 3);
    for (;;) {
        osRecvMesg((OSMesgQueue *) &bg_load_request_queue, &msg, 1);
        ReadWave(bg_load_request);
        osSendMesg((OSMesgQueue *) &bg_load_idle_queue, NULL, 1);
    }
}

void SetWaveInstanceModel(s32 arg0) {
    if ((gWaveInstance[arg0].f3 = wave_is_model[gWaveInstance[arg0].type]) != 0) {
        if (!(arg0 < gNumModels)) {
            gNumModels = arg0 + 1;
        }
    }
}

