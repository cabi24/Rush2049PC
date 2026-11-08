/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * ModelThread (0x800E7808, 268 B; historical label) = ModelThread@model.c, N64 thread form (Dreamcast
 * model.c ModelThread is exact for the per-tick body; MAT3 GC model.c 0x8002CA78). Created by the static main_thread_entry
 * as the scheduler-client thread (id 5, pri 7). The GC per-call sInited block runs once before the loop:
 * modelrun = 0; init_model_task(0) (init_model_task). Each tick: block on the client queue, drain the
 * queued messages, then `if (ControllerUpdate() && modelrun) model_iteration();` (func_800E73D8 / model_iteration).
 * SHAPING (disclosed, compiles to no code): the drain loop tests each message's scheduler type in a compiled-out
 * `if` (OS_SC_RETRACE_MSG 1 / OS_SC_PRE_NMI_MSG 4 of the SDK sched.h; any second value gives the same code). uopt
 * keeps the type load as a coloured web (retail's never-used s2 save) and the constant 1 as a register candidate
 * in the drain loop (retail's `li s1,1` / `move a2,s1`); ugen drops the empty branch and as1 deletes the dead loads
 * (retail's lhu t7: one ring pop for the deleted msg load). Without it: the constant 1 is no candidate and two
 * s-register webs are missing (30-33 rows, colour only). An `if (.) {}` around a debug print is the likely
 * original. No unused locals, no volatile.
 * Group packaging (layout only): init_controllers.c and stub_before_model_iteration.c are byte copies of locked singles that put
 * ModelThread at its retail offset modulo 32 for the `.align 5` pad of the unreachable epilogue.
 */
typedef unsigned char u8;
typedef signed char s8;
typedef short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef void *OSMesg;
typedef struct OSMesgQueue { u8 opaque[0x18]; } OSMesgQueue;
typedef struct OSScClient { struct OSScClient *next; OSMesgQueue *msgQ; } OSScClient;
typedef struct OSScMsg { s16 type; s8 misc[30]; } OSScMsg;

#define OS_SC_RETRACE_MSG 1
#define OS_SC_PRE_NMI_MSG 4

extern OSMesgQueue model_thread_queue;      /* model thread client queue */
extern OSMesg model_thread_msgs[8];        /* its messages */
extern u16 modelrun;
extern u8 gScheduler[];             /* scheduler */

extern void __setfpcsr(u32 value);
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
void osScAddClient(void *sc, OSScClient *c, OSMesgQueue *msgQ);
void init_controllers(void);           /* controller init (osSetEventMesg SI) */
void init_model_task(s32 arg0);
s32 ControllerUpdate(void);
void model_iteration(void);

void ModelThread(void *arg)
{
    OSScClient client;
    OSMesg msg;

    msg = 0;
    __setfpcsr(0x01000E00);
    osCreateMesgQueue(&model_thread_queue, model_thread_msgs, 8);
    init_controllers();
    modelrun = 0;
    init_model_task(0);
    osScAddClient(gScheduler, &client, &model_thread_queue);
    while (1) {
        osRecvMesg(&model_thread_queue, &msg, 1);
        do {
            if (((OSScMsg *)msg)->type == OS_SC_RETRACE_MSG || ((OSScMsg *)msg)->type == OS_SC_PRE_NMI_MSG) {}
        } while (osRecvMesg(&model_thread_queue, &msg, 0) != -1);
        if (ControllerUpdate() && modelrun) {
            model_iteration();
        }
    }
}
