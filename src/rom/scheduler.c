/* The scheduler (libultra's sched.c, as the game has it): a thread that serves the VI's retraces and the RSP's and
 * RDP's interrupts, sends each client (the main thread, the model thread, the audio manager) a message each
 * retrace, and runs the tasks the game sends (gfx frames and audio) on the RSP and RDP, audio first, swapping the
 * frame buffer when a frame is done. On the PC port/ultra implements the OS underneath: the interrupts are its
 * events, the tasks its renderer and audio interpreter (docs/port/ULTRA.md). */

#define __scAppendList pc_m2c_scAppendList   /* m2c_types.h's prototypes take its OSSched, not PcSched */
#define __scExec pc_m2c_scExec
#include "rom_tu.h"
#undef __scAppendList
#undef __scExec
/* PC: the scheduler keeps the N64 offsets the game reads it at (docs/port/ULTRA.md). The game takes the
 * queues' and the thread's addresses (scheduler_interrupt_queue, scheduler_task_queue, scheduler_thread) and reads retraceCount at 0x27C
 * (its frame clock: frameCount, tick, scheduler_retrace_count). port/ultra keys queues and threads by address and never
 * writes into them, so they are N64-sized storage here; the scheduler's own pointers follow at host size. */
typedef struct PcSched {
    s16 state;                         /* 0x000 */
    u8 pad02[0x20 - 0x02];
    s16 priority;                      /* 0x020 */
    u8 pad22[0x40 - 0x22];
    u8 cmdQueue[0x18];                 /* 0x040 */
    u8 cmdMsgs[0x20];                  /* 0x058 */
    u8 retQueue[0x18];                 /* 0x078: where the game sends its tasks */
    u8 retMsgs[0x20];                  /* 0x090 */
    u8 padB0[0x260 - 0xB0];            /* 0x0B0: the scheduler thread */
    u8 pad260[0x27C - 0x260];          /* N64: clientList .. curRDPTask, 4-byte pointers */
    s32 retraceCount;                  /* 0x27C */
    s32 doAudio;                       /* 0x280: an audio task waits */
    OSScClient *clientList;            /* 0x288 */
    OSScTask *audioListHead;
    OSScTask *gfxListHead;
    OSScTask *audioListTail;
    OSScTask *gfxListTail;
    OSScTask *curRSPTask;              /* 0x2B0 */
    OSScTask *curRDPTask;
} PcSched;
#define OSSched PcSched
typedef char pc_sched_layout_check[(sizeof(PcSched) == 0x2C0 && sizeof(void *) == 8) ? 1 : -1];

/* The scheduler's data, and the frame timer's (timer.c). Initialized, so that they are definitions the aliases
 * below can name (not common symbols). */
volatile OSTime __osScRetraceTime = 0;
OSSched gScheduler = {0};
s32 scheduler_spare_word = 0;
s16 gRenderInProgress = 0;
volatile OSTime __osScAudioStart = 0;
volatile OSTime __osScFrameTime = 0;
u64 scheduler_spare_time = 0;
f32 gSeconds = 0;
f32 gFrameSeconds = 0;
s32 gLastFrameCount = 0;
s32 gCountdownEnd = 0;
s32 gCountdownStart = 0;
volatile s32 gLastFrameCounter = 0;
/* The game's names for this storage, by N64 address (port/data leaves these to the libultra layer). */
#if defined(__APPLE__)
#define PC_SYM(name) "_" name
#else
#define PC_SYM(name) name
#endif
#define PC_ALIAS(name, target) ".globl " PC_SYM(name) "\n.set " PC_SYM(name) ", " PC_SYM(target) "\n"
__asm__(PC_ALIAS("scheduler_interrupt_queue", "gScheduler + 0x40")
        PC_ALIAS("scheduler_task_queue", "gScheduler + 0x78")
        PC_ALIAS("scheduler_thread", "gScheduler + 0xB0")
        PC_ALIAS("scheduler_retrace_count", "gScheduler + 0x27C")
        PC_ALIAS("scheduler_current_rsp_task", "gScheduler + 0x2B0"));  /* curRSPTask, host-sized: not at its N64 offset */
#include "scheduler_declarations.h"

/* The messages on the scheduler's command queue (libultra sched.c's names; the fifth is the game's). */
#define VIDEO_MSG 666       /* a retrace */
#define RSP_DONE_MSG 667
#define RDP_DONE_MSG 668
#define PRE_NMI_MSG 669     /* the reset button */
#define SCHEDULE_MSG 670    /* software interrupt 1: schedule now */
/* The events they come from (libultra's OS_EVENT_*). */
#define OS_EVENT_SW1 0
#define OS_EVENT_SP 4
#define OS_EVENT_DP 9
/* A task's type (OSTask.t.type: M_GFXTASK 1, M_AUDTASK 2), flags and state (sched.h's). */
#define OS_SC_NEEDS_RDP 0x1
#define OS_SC_NEEDS_RSP 0x2
#define OS_SC_RCP_MASK 0x3
#define OS_SC_TYPE_MASK 0x7
#define OS_SC_XBUS 0x3          /* both processors, the RDP reading from the RSP */
#define OS_SC_PARALLEL_TASK 0x10
#define OS_SC_LAST_TASK 0x20
#define OS_SC_SWAPBUFFER 0x40
#define OS_SC_YIELD 0x10
#define OS_SC_YIELDED 0x20
/* The processors (availRCP): the RSP and the RDP. */
#define OS_SC_SP 0x2
#define OS_SC_DP 0x1

/* Makes the scheduler: no tasks, no clients, its command and task queues, the VI manager and the video mode (from
 * libultra's mode table), the screen black; the RSP, RDP and pre-NMI interrupts and the retrace (every numFields
 * fields) as messages on its command queue; then its thread at priority. */
void osCreateScheduler(OSSched *scheduler, void *stack, OSPri priority, u8 mode, u8 numFields)
{
    gRenderInProgress = 0;
    scheduler->curRSPTask = 0;
    scheduler->curRDPTask = 0;
    scheduler->clientList = 0;
    scheduler->retraceCount = 0;
    scheduler->audioListHead = 0;
    scheduler->gfxListHead = 0;
    scheduler->audioListTail = 0;
    scheduler->gfxListTail = 0;
    scheduler->state = 1;
    scheduler->priority = 4;
    osCreateMesgQueue(&scheduler->cmdQueue, scheduler->cmdMsgs, 8);
    osCreateMesgQueue(&scheduler->retQueue, scheduler->retMsgs, 8);
    osCreateViManager(254);
    osViSetMode(&osViModeTable[mode]);
    osViBlack(1);
    osSetEventMesg(OS_EVENT_SP, &scheduler->cmdQueue, (OSMesg)RSP_DONE_MSG);
    osSetEventMesg(OS_EVENT_DP, &scheduler->cmdQueue, (OSMesg)RDP_DONE_MSG);
    osSetEventMesg(OS_EVENT_PRENMI, &scheduler->cmdQueue, (OSMesg)PRE_NMI_MSG);
    osSetEventMesg(OS_EVENT_SW1, &scheduler->cmdQueue, (OSMesg)SCHEDULE_MSG);
    osViSetEvent(&scheduler->cmdQueue, (OSMesg)VIDEO_MSG, numFields);
    osCreateThread((OSThread *)scheduler->padB0, 4, __scMain, scheduler, stack, priority);
    osStartThread((OSThread *)scheduler->padB0);
}

/* Adds client to the scheduler's clients, which get a message on queue each retrace. With interrupts off. */
void osScAddClient(OSSched *scheduler,OSScClient *client,OSMesgQueue *queue) {
 u32 mask;
 mask=osSetIntMask(1);
 client->msgQueue=queue;
 client->next=scheduler->clientList;
 scheduler->clientList=client;
 osSetIntMask(mask);
}

/* The scheduler's thread: forever, a message from the command queue: a retrace (the frame time, the clients told,
 * the next tasks started), the RSP done, the RDP done (the RDP's time for the frame), a request to schedule, or a
 * pre-NMI (every client gets the pre-NMI message). */
extern OSViMode osViModeTable[];
extern s32 __osScPendingSwap,frame_cycle_count;
extern u32 __osScSwapCount;
extern OSScTask *scheduler_audio_task;
extern void osCreateMesgQueue(OSMesgQueue *,OSMesg *,s32);
extern s32 osRecvMesg(OSMesgQueue *,OSMesg *,s32);
extern s32 osJamMesg(OSMesgQueue *,OSMesg,s32),osSendMesg(OSMesgQueue *,OSMesg,s32);
extern void osCreateThread(OSThread *,s32,void (*)(void *),void *,void *,OSPri);
extern void osStartThread(OSThread *);
extern void osCreateViManager(OSPri);
extern void osViSetMode(void *),osViBlack(s32);
extern u32 osSetIntMask(u32);
extern void osViSetEvent(OSMesgQueue *,OSMesg,s32);
extern OSTime osGetTime(void);
extern void *osViGetCurrentFramebuffer(void),*osViGetNextFramebuffer(void);
extern void osViSwapBuffer(void *),video_apply_pending_mode(void);
extern s32 osSpTaskYielded(void *);
extern void osWritebackDCacheAll(void),osSpTaskLoad(void *),osSpTaskStartGo(void *),osSpTaskYield(void);
extern s32 osDpSetNextBuffer(void *,u64);
void osCreateScheduler(OSSched *,void *,OSPri,u8,u8);
void osScAddClient(OSSched *,OSScClient *,OSMesgQueue *);
void __scMain(void *);
void __scSchedule(OSSched *);
void __scHandleRetrace(OSSched *),__scHandleRSP(OSSched *),__scHandleRDP(OSSched *);
OSScTask *__scTaskReady(OSSched *,OSScTask *);
s32 __scExecTask(OSSched *,OSScTask *);
void __scAppendList(OSSched *,OSScTask *),__scExec(OSSched *,OSScTask *,OSScTask *),__scHandlePreNMI(OSSched *);
s32 __scScheduleCore(OSSched *,OSScTask **,OSScTask **,s32);
void __scMain(void *arg) {
 OSMesg message;
 OSSched *scheduler=(OSSched *)arg;
 OSScClient *client;
 while(1) {
  osRecvMesg(&scheduler->cmdQueue,&message,OS_MESG_BLOCK);
  switch((s32)message) {
   case VIDEO_MSG:
    __osScFrameTime=osGetTime();
    __scHandleRetrace(scheduler);
    __scSchedule(scheduler);
    break;
   case RSP_DONE_MSG:
    __scHandleRSP(scheduler);
    break;
   case RDP_DONE_MSG:
    frame_cycle_count=(s32)(osGetTime()-__osScRetraceTime);
    __scHandleRDP(scheduler);
    break;
   case SCHEDULE_MSG:
    __scSchedule(scheduler);
    break;
   case PRE_NMI_MSG:
    for(client=scheduler->clientList;client!=0;client=client->next)
     osJamMesg(client->msgQueue,(OSMesg)&scheduler->priority,OS_MESG_BLOCK);
    break;
  }
 }
}

/* Takes the tasks the game has sent into the scheduler's lists, then starts what the RSP and RDP can run: when an
 * audio task waits while a graphics task runs, the graphics task is asked to yield. */
void __scSchedule(OSSched *scheduler)
{
    OSMesg message;
    s32 state;
    OSScTask *sp = 0;
    OSScTask *dp = 0;
    while (osRecvMesg(&scheduler->retQueue, &message, OS_MESG_NOBLOCK) != -1)
        __scAppendList(scheduler, (OSScTask *)message);
    if (scheduler->doAudio && scheduler->audioListHead && scheduler->curRSPTask) {
        __scHandlePreNMI(scheduler);
    } else {
        state = ((scheduler->curRSPTask == 0) << 1) | (scheduler->curRDPTask == 0);
        if (__scScheduleCore(scheduler, &sp, &dp, state) != state)
            __scExec(scheduler, sp, dp);
    }
}

/* A retrace: counts it; a frame buffer waiting to be shown is swapped in once PC_SWAP_RETRACES have passed since
 * the last swap (the pending video mode with it); and every client gets the retrace message. */
extern s32 __osScPendingSwap;
extern u32 __osScSwapCount;
extern void osViSwapBuffer(void *mode);
/* The pending swap keeps the frame buffer as a 32-bit N64 address: (s32)PC_N64A(p) is a window pointer's, PC_N64P(a)
 * the pointer at one (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
extern void video_apply_pending_mode(void);
extern s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);
void __scHandleRetrace(OSSched *scheduler) {
    OSScTask *rspTask;
    OSScClient *client;
    s32 i;
    OSScTask *sp = 0;
    OSScTask *dp = 0;
    s32 state;

    scheduler->retraceCount++;
    if (__osScPendingSwap && scheduler->retraceCount - __osScSwapCount >= PC_SWAP_RETRACES) {
        osViSwapBuffer(((void *)PC_N64P((u32)__osScPendingSwap)));
        video_apply_pending_mode();
        __osScSwapCount = scheduler->retraceCount;
        __osScPendingSwap = 0;
    } else {
    }
    for (client = scheduler->clientList; client != 0; client = client->next) {
        osSendMesg(client->msgQueue, (OSMesg)scheduler, 0);
    }
}

/* The RSP finished or yielded: a yielded graphics task goes back to the head of the graphics list (to run on), a
 * finished one is done with the RSP (and is done if the RDP is too); then the next tasks. */
void __scHandleRSP(OSSched *scheduler)
{
    OSScTask *task, *sp = 0, *dp = 0;
    s32 state;
    if (!scheduler->curRSPTask) { }
    task = scheduler->curRSPTask;
    scheduler->curRSPTask = 0;
    if ((task->state & OS_SC_YIELD) && osSpTaskYielded(&task->type)) {
        task->state |= OS_SC_YIELDED;
        if ((task->flags & OS_SC_TYPE_MASK) == OS_SC_XBUS) {
            task->next = scheduler->gfxListHead;
            scheduler->gfxListHead = task;
            if (scheduler->gfxListTail == 0)
                scheduler->gfxListTail = task;
        }
    } else {
        /* (sched.c's debug checks of the task here are empty in the retail library; the test stays so that the
         * code stays the base's) */
        if (task->flags & OS_SC_SWAPBUFFER) { }
        else if (task->type == M_AUDTASK) { }
        task->state &= ~OS_SC_SP;
        __scExecTask(scheduler, task);
    }
    state = ((scheduler->curRSPTask == 0) << 1) | (scheduler->curRDPTask == 0);
    if (__scScheduleCore(scheduler, &sp, &dp, state) != state)
        __scExec(scheduler, sp, dp);
}

/* The RDP finished its task: it is done with the RDP (and is done if the RSP is too); then the next tasks. */
void __scHandleRDP(OSSched *scheduler)
{
    OSScTask *task, *sp = 0, *dp = 0;
    s32 state;
    task = scheduler->curRDPTask;
    scheduler->curRDPTask = 0;
    task->state &= ~OS_SC_DP;
    __scExecTask(scheduler, task);
    state = ((scheduler->curRSPTask == 0) << 1) | (scheduler->curRDPTask == 0);
    if (__scScheduleCore(scheduler, &sp, &dp, state) != state)
        __scExec(scheduler, sp, dp);
}

/* Whether graphics task task can run: the last frame buffer swapped is on screen, and no swap waits. */
OSScTask *__scTaskReady(OSSched *scheduler, OSScTask *task) {
    void *current;
    void *next;
    if (task != 0) {
        if ((current = osViGetCurrentFramebuffer()) !=
            (next = osViGetNextFramebuffer())) return 0;
        if (__osScPendingSwap != 0 && scheduler->retraceCount - __osScSwapCount < PC_SWAP_RETRACES)
            return 0;
        return task;
    }
    return 0;
}

/* If task has finished on both processors: tells the game (its message), and for a graphics frame counts it out
 * of gRenderInProgress and swaps its frame buffer, now or (too soon after the last) later. Returns 1 then, else 0. */
s32 __scExecTask(OSSched *scheduler, OSScTask *task)
{
    int rv;
    if ((task->state & (OS_SC_SP | OS_SC_DP)) == 0) {
        rv = osSendMesg(task->msgQueue, task->msg, OS_MESG_BLOCK);
        if (task->type == M_GFXTASK) {
            gRenderInProgress--;
            if ((task->flags & OS_SC_SWAPBUFFER) && (task->flags & OS_SC_LAST_TASK)) {
                if (scheduler->retraceCount - __osScSwapCount >= PC_SWAP_RETRACES) {
                    __osScSwapCount = scheduler->retraceCount;
                    osViSwapBuffer(task->framebuffer);
                    video_apply_pending_mode();
                } else {
                    __osScPendingSwap = (s32)PC_N64A(task->framebuffer);
                }
            }
        }
        return 1;
    }
    return 0;
}

/* Appends task to the audio list or the graphics list, by its type (an audio task makes audio pending), with its
 * state from its flags. */
void __scAppendList(OSSched *scheduler, OSScTask *task)
{
    long type = task->type;

    if (type == M_AUDTASK) {
        if (scheduler->audioListTail)
            scheduler->audioListTail->next = task;
        else
            scheduler->audioListHead = task;
        scheduler->audioListTail = task;
        scheduler->doAudio = 1;
    } else {
        if (scheduler->gfxListTail)
            scheduler->gfxListTail->next = task;
        else
            scheduler->gfxListHead = task;
        scheduler->gfxListTail = task;
    }
    task->next = 0;
    task->state = task->flags & OS_SC_RCP_MASK;
}

/* Starts sp on the RSP (an audio task's start time noted, a fresh graphics task's retrace time noted) and dp's
 * output on the RDP. */
void __scExec(OSSched *scheduler,OSScTask *sp,OSScTask *dp) {
 s32 rv;
 if(sp) {
  if(sp->type==M_AUDTASK) {
   __osScAudioStart=osGetTime();
   scheduler_audio_task=sp;
  } else if(!(sp->state&OS_SC_YIELDED)) {
   __osScRetraceTime=__osScFrameTime;
  }
  osWritebackDCacheAll();
  sp->state&=~(OS_SC_YIELD|OS_SC_YIELDED);
  osSpTaskLoad(&sp->type);
  osSpTaskStartGo(&sp->type);
  scheduler->curRSPTask=sp;
  if(sp==dp)scheduler->curRDPTask=dp;
 }
 if(dp && dp!=sp) {
  rv=osDpSetNextBuffer(dp->output_buff,*(u64 *)dp->output_buff_size);
  scheduler->curRDPTask=dp;
 }
}

/* Asks the graphics task on the RSP to yield (for the audio task, or a reset). */
void __scHandlePreNMI(OSSched *scheduler) {
 if(scheduler->curRSPTask->type==M_GFXTASK) {
  scheduler->curRSPTask->state|=OS_SC_YIELD;
  osSpTaskYield();
 }
}

/* Chooses the next RSP and RDP tasks for the processors free in availRCP (bit 1 the RSP, bit 0 the RDP): a
 * pending audio task first (unless a yielded graphics task must finish), else the graphics list's head when its
 * frame buffer may be drawn, on the processors its flags and state call for; repeats while it finds more. Returns
 * the processors still free. */
s32 __scScheduleCore(OSSched *scheduler, OSScTask **sp, OSScTask **dp, s32 availRCP)
{
    s32 avail = availRCP;
    OSScTask *gfx = scheduler->gfxListHead;
    OSScTask *audio = scheduler->audioListHead;
    if (scheduler->doAudio && (avail & OS_SC_SP)) {
        if (gfx && (gfx->flags & OS_SC_PARALLEL_TASK)) {
            *sp = gfx;
            avail &= ~OS_SC_SP;
        } else {
            *sp = audio;
            avail &= ~OS_SC_SP;
            scheduler->doAudio = 0;
            scheduler->audioListHead = scheduler->audioListHead->next;
            if (scheduler->audioListHead == 0)
                scheduler->audioListTail = 0;
        }
    } else {
        if (__scTaskReady(scheduler, gfx)) {
            switch (gfx->flags & OS_SC_TYPE_MASK) {
                case OS_SC_XBUS:
                    if (gfx->state & OS_SC_YIELDED) {
                        if (avail & OS_SC_SP) {
                            *sp = gfx;
                            avail &= ~OS_SC_SP;
                            if (gfx->state & OS_SC_DP) {
                                *dp = gfx;
                                avail &= ~OS_SC_DP;
                                /* (an empty debug check, kept so that the code stays the base's) */
                                if ((avail & OS_SC_DP) == 0)
                                    if (scheduler->curRDPTask != gfx) { }
                            }
                            scheduler->gfxListHead = scheduler->gfxListHead->next;
                            if (scheduler->gfxListHead == 0)
                                scheduler->gfxListTail = 0;
                        }
                    } else {
                        if (avail == (OS_SC_SP | OS_SC_DP)) {
                            *sp = *dp = gfx;
                            avail &= ~(OS_SC_SP | OS_SC_DP);
                            scheduler->gfxListHead = scheduler->gfxListHead->next;
                            if (scheduler->gfxListHead == 0)
                                scheduler->gfxListTail = 0;
                        }
                    }
                    break;
                case 7:
                case 6:
                case 2:
                    if (gfx->state & OS_SC_SP) {
                        if (avail & OS_SC_SP) {
                            *sp = gfx;
                            avail &= ~OS_SC_SP;
                        }
                    } else if (gfx->state & OS_SC_DP) {
                        if (avail & OS_SC_DP) {
                            *dp = gfx;
                            avail &= ~OS_SC_DP;
                            scheduler->gfxListHead = scheduler->gfxListHead->next;
                            if (scheduler->gfxListHead == 0)
                                scheduler->gfxListTail = 0;
                        }
                    }
                    break;
                case 5:
                case 1:
                default:
                    break;
            }
        }
    }
    if (avail != availRCP)
        avail = __scScheduleCore(scheduler, sp, dp, avail);
    return avail;
}
