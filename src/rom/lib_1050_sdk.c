/* GENERATED ROM-aligned TU — segment 0x1050 (rom/lib_1050_sdk)
 * layout map c8f0857aebf4ea1ed90dde16bb4db378a8281234d10bd7a742394c75635cc457.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */

#ifdef TARGET_PC
#define __scAppendList pc_m2c_scAppendList   /* m2c_types.h's prototypes take its OSSched, not PcSched */
#define __scExec pc_m2c_scExec
#endif
#include "rom_tu.h"
#ifdef TARGET_PC
#undef __scAppendList
#undef __scExec
/* PC: the scheduler keeps the N64 offsets the game reads it at (docs/port/ULTRA.md). The game takes the
 * queues' and the thread's addresses (D_8002E928, D_8002E960, D_8002E998) and reads retraceCount at 0x27C
 * (its frame clock: frameCount, tick, gViTickCounter). port/ultra keys queues and threads by address and never
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
    s32 audioListPending;              /* 0x280 */
    OSScClient *clientList;            /* 0x288 */
    OSScTask *rspTaskHead;
    OSScTask *rspTaskTail;
    OSScTask *rdpTaskHead;
    OSScTask *rdpTaskTail;
    OSScTask *curRSPTask;              /* 0x2B0 */
    OSScTask *curRDPTask;
} PcSched;
#define OSSched PcSched
typedef char pc_sched_layout_check[(sizeof(PcSched) == 0x2C0 && sizeof(void *) == 8) ? 1 : -1];

/* owned .bss of __scMain (__scMain, __scExec): C definitions in place of the companion. Initialized, so that
 * they are definitions the aliases below can name (not common symbols). */
volatile OSTime __osScRetraceTime = 0;
OSSched gScheduler = {0};
s32 D_8002EB6C = 0;
s16 __osScTaskCount = 0;
volatile OSTime __osScAudioStart = 0;
volatile OSTime __osScFrameTime = 0;
u64 D_8002EB88 = 0;
f32 __osScElapsedTime = 0;
f32 __osScDeltaTime = 0;
s32 gViFrameDelta = 0;
s32 gViAccumTime = 0;
s32 gViAccumTotal = 0;
volatile s32 gViLastTick = 0;
/* The game's names for this storage, by N64 address (port/data leaves these to the libultra layer). */
#if defined(__APPLE__)
#define PC_SYM(name) "_" name
#else
#define PC_SYM(name) name
#endif
#define PC_ALIAS(name, target) ".globl " PC_SYM(name) "\n.set " PC_SYM(name) ", " PC_SYM(target) "\n"
__asm__(PC_ALIAS("D_8002E8E8", "gScheduler")
        PC_ALIAS("D_8002E928", "gScheduler + 0x40")
        PC_ALIAS("D_8002E960", "gScheduler + 0x78")
        PC_ALIAS("D_8002E998", "gScheduler + 0xB0")
        PC_ALIAS("gViTickCounter", "gScheduler + 0x27C")
        PC_ALIAS("D_8002EB64", "gScheduler + 0x27C")
        PC_ALIAS("D_8002EB5C", "gScheduler + 0x2B0")   /* curRSPTask, host-sized: not at its N64 offset */
        PC_ALIAS("D_8002EB70", "__osScTaskCount")
        PC_ALIAS("D_8002EB90", "__osScElapsedTime")
        PC_ALIAS("D_8002EB94", "__osScDeltaTime")
        PC_ALIAS("D_8002EB98", "gViFrameDelta"));
#else
/* owned .bss of __scMain (__scMain, __scExec): C definitions in place of the companion */
volatile OSTime __osScRetraceTime;
OSSched gScheduler;
s32 D_8002EB6C;
s16 __osScTaskCount;
volatile OSTime __osScAudioStart;
volatile OSTime __osScFrameTime;
u64 D_8002EB88;
f32 __osScElapsedTime;
f32 __osScDeltaTime;
s32 gViFrameDelta;
s32 gViAccumTime;
s32 gViAccumTotal;
volatile s32 gViLastTick;
#endif
#include "scheduler_declarations.h"


/* PROMOTED 2026-10-02 — osCreateScheduler
 * Source:   module_campaign_20261002 SDK publication
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void osCreateScheduler(OSSched *scheduler, void *stack, OSPri priority, u8 mode, u8 numFields)
{
    __osScTaskCount = 0;
    scheduler->curRSPTask = 0;
    scheduler->curRDPTask = 0;
    scheduler->clientList = 0;
    scheduler->retraceCount = 0;
    scheduler->rspTaskHead = 0;
    scheduler->rspTaskTail = 0;
    scheduler->rdpTaskHead = 0;
    scheduler->rdpTaskTail = 0;
    scheduler->state = 1;
    scheduler->priority = 4;
    osCreateMesgQueue(&scheduler->cmdQueue, scheduler->cmdMsgs, 8);
    osCreateMesgQueue(&scheduler->retQueue, scheduler->retMsgs, 8);
    osCreateViManager(254);
    osViSetMode(&gViModeTableBase[mode]);
    osViBlack(1);
    osSetEventMesg(4, &scheduler->cmdQueue, (OSMesg)667);
    osSetEventMesg(9, &scheduler->cmdQueue, (OSMesg)668);
    osSetEventMesg(14, &scheduler->cmdQueue, (OSMesg)669);
    osSetEventMesg(0, &scheduler->cmdQueue, (OSMesg)670);
    osViSetEvent(&scheduler->cmdQueue, (OSMesg)666, numFields);
    osCreateThread((OSThread *)scheduler->padB0, 4, __scMain, scheduler, stack, priority);
    osStartThread((OSThread *)scheduler->padB0);
}

/* PROMOTED 2026-10-02 — osScAddClient
 * Source:   module_campaign_20261002 SDK publication
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void osScAddClient(OSSched *scheduler,OSScClient *client,OSMesgQueue *queue) {
 u32 mask;
 mask=osSetIntMask(1);
 client->msgQueue=queue;
 client->next=scheduler->clientList;
 scheduler->clientList=client;
 osSetIntMask(mask);
}

/* PROMOTED 2026-10-12 — __scMain
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern OSViMode gViModeTableBase[];
extern s32 __osScPendingSwap,__osScFrameTimeResult;
extern u32 __osScSwapCount;
extern OSScTask *__osScCurAudioTask;
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
extern void osViSwapBuffer(void *),display_mode_tick(void);
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
#define OS_MESG_NOBLOCK 0
#define OS_MESG_BLOCK 1
void __scMain(void *arg) {
 OSMesg message;
 OSSched *scheduler=(OSSched *)arg;
 OSScClient *client;
 while(1) {
  osRecvMesg(&scheduler->cmdQueue,&message,OS_MESG_BLOCK);
  switch((s32)message) {
   case 666:
    __osScFrameTime=osGetTime();
    __scHandleRetrace(scheduler);
    __scSchedule(scheduler);
    break;
   case 667:
    __scHandleRSP(scheduler);
    break;
   case 668:
    __osScFrameTimeResult=(s32)(osGetTime()-__osScRetraceTime);
    __scHandleRDP(scheduler);
    break;
   case 670:
    __scSchedule(scheduler);
    break;
   case 669:
    for(client=scheduler->clientList;client!=0;client=client->next)
     osJamMesg(client->msgQueue,(OSMesg)&scheduler->priority,OS_MESG_BLOCK);
    break;
  }
 }
}

/* PROMOTED 2026-10-02 — __scSchedule
 * Source:   module_campaign_20261002 SDK publication
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void __scSchedule(OSSched *scheduler)
{
    OSMesg message;
    s32 state;
    OSScTask *sp = 0;
    OSScTask *dp = 0;
    while (osRecvMesg(&scheduler->retQueue, &message, OS_MESG_NOBLOCK) != -1)
        __scAppendList(scheduler, (OSScTask *)message);
    if (scheduler->audioListPending && scheduler->rspTaskHead && scheduler->curRSPTask) {
        __scHandlePreNMI(scheduler);
    } else {
        state = ((scheduler->curRSPTask == 0) << 1) | (scheduler->curRDPTask == 0);
        if (__scScheduleCore(scheduler, &sp, &dp, state) != state)
            __scExec(scheduler, sp, dp);
    }
}

/* PROMOTED 2026-10-12 — __scHandleRetrace
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern s32 __osScPendingSwap;
extern u32 __osScSwapCount;
extern void osViSwapBuffer(void *mode);
extern void display_mode_tick(void);
extern s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);
void __scHandleRetrace(OSSched *scheduler) {
    OSScTask *rspTask;
    OSScClient *client;
    s32 i;
    OSScTask *sp = 0;
    OSScTask *dp = 0;
    s32 state;

    scheduler->retraceCount++;
    if (__osScPendingSwap && scheduler->retraceCount - __osScSwapCount >= 2U) {
        osViSwapBuffer((void *)(u32)__osScPendingSwap);
        display_mode_tick();
        __osScSwapCount = scheduler->retraceCount;
        __osScPendingSwap = 0;
    } else {
    }
    for (client = scheduler->clientList; client != 0; client = client->next) {
        osSendMesg(client->msgQueue, (OSMesg)scheduler, 0);
    }
}

/* PROMOTED 2026-10-02 — __scHandleRSP
 * Source:   module_campaign_20261002 SDK publication
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void __scHandleRSP(OSSched *scheduler)
{
    OSScTask *task, *sp = 0, *dp = 0;
    s32 state;
    if (!scheduler->curRSPTask) { }
    task = scheduler->curRSPTask;
    scheduler->curRSPTask = 0;
    if ((task->state & 0x10) && osSpTaskYielded(&task->type)) {
        task->state |= 0x20;
        if ((task->flags & 7) == 3) {
            task->next = scheduler->rspTaskTail;
            scheduler->rspTaskTail = task;
            if (scheduler->rdpTaskTail == 0)
                scheduler->rdpTaskTail = task;
        }
    } else {
        if (task->flags & 0x40) { }
        else if (task->type == 2) { }
        task->state &= ~2;
        __scExecTask(scheduler, task);
    }
    state = ((scheduler->curRSPTask == 0) << 1) | (scheduler->curRDPTask == 0);
    if (__scScheduleCore(scheduler, &sp, &dp, state) != state)
        __scExec(scheduler, sp, dp);
}

/* PROMOTED 2026-10-02 — __scHandleRDP
 * Source:   module_campaign_20261002 SDK publication
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void __scHandleRDP(OSSched *scheduler)
{
    OSScTask *task, *sp = 0, *dp = 0;
    s32 state;
    if (!scheduler->curRDPTask) { }
    if (scheduler->curRDPTask->type != 1) { }
    task = scheduler->curRDPTask;
    scheduler->curRDPTask = 0;
    task->state &= ~1;
    __scExecTask(scheduler, task);
    state = ((scheduler->curRSPTask == 0) << 1) | (scheduler->curRDPTask == 0);
    if (__scScheduleCore(scheduler, &sp, &dp, state) != state)
        __scExec(scheduler, sp, dp);
}

/* PROMOTED 2026-10-02 — __scTaskReady
 * Source:   module_campaign_20261002 SDK publication
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
OSScTask *__scTaskReady(OSSched *scheduler, OSScTask *task) {
    void *current;
    void *next;
    if (task != 0) {
        if ((current = osViGetCurrentFramebuffer()) !=
            (next = osViGetNextFramebuffer())) return 0;
        if (__osScPendingSwap != 0 && scheduler->retraceCount - __osScSwapCount < 2U)
            return 0;
        return task;
    }
    return 0;
}

/* PROMOTED 2026-10-02 — __scExecTask
 * Source:   module_campaign_20261002 SDK publication
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
s32 __scExecTask(OSSched *scheduler, OSScTask *task)
{
    int rv;
    if ((task->state & 3) == 0) {
        if (!task->msgQueue) { }
        rv = osSendMesg(task->msgQueue, task->msg, OS_MESG_BLOCK);
        if (task->type == 1) {
            __osScTaskCount--;
            if ((task->flags & 0x40) && (task->flags & 0x20)) {
                if (scheduler->retraceCount - __osScSwapCount >= 2U) {
                    __osScSwapCount = scheduler->retraceCount;
                    osViSwapBuffer(task->framebuffer);
                    display_mode_tick();
                } else {
                    __osScPendingSwap = (s32)task->framebuffer;
                }
            }
        }
        return 1;
    }
    return 0;
}

/* PROMOTED 2026-10-12 — __scAppendList
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void __scAppendList(OSSched *scheduler, OSScTask *task)
{
    long type = task->type;

    if (!((type == 2) || (type == 1))) { }

    if (type == 2) {
        if (scheduler->rdpTaskHead)
            scheduler->rdpTaskHead->next = task;
        else
            scheduler->rspTaskHead = task;
        scheduler->rdpTaskHead = task;
        if (scheduler->audioListPending) { }
        scheduler->audioListPending = 1;
        ;
    } else {
        if (scheduler->rdpTaskTail)
            scheduler->rdpTaskTail->next = task;
        else
            scheduler->rspTaskTail = task;
        scheduler->rdpTaskTail = task;
    }
    task->next = 0;
    task->state = task->flags & 3;
}

/* PROMOTED 2026-10-12 — __scExec
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#define OS_MESG_NOBLOCK 0
#define OS_MESG_BLOCK 1
void __scExec(OSSched *scheduler,OSScTask *sp,OSScTask *dp) {
 s32 rv;
 if(scheduler->curRSPTask) { }
 if(sp) {
  if(sp->type==2) {
   __osScAudioStart=osGetTime();
   __osScCurAudioTask=sp;
  } else if(!(sp->state&0x20)) {
   __osScRetraceTime=__osScFrameTime;
  }
  osWritebackDCacheAll();
  sp->state&=~0x30;
  osSpTaskLoad(&sp->type);
  osSpTaskStartGo(&sp->type);
  scheduler->curRSPTask=sp;
  if(sp==dp)scheduler->curRDPTask=dp;
 }
 if(dp && dp!=sp) {
  if(!dp->unk38) { }
  rv=osDpSetNextBuffer(dp->unk38,*(u64 *)dp->unk3C);
  if(rv) { }
  scheduler->curRDPTask=dp;
 }
}

/* PROMOTED 2026-10-02 — __scHandlePreNMI
 * Source:   module_campaign_20261002 SDK publication
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void __scHandlePreNMI(OSSched *scheduler) {
 if(scheduler->curRSPTask->type==1) {
  scheduler->curRSPTask->state|=0x10;
  osSpTaskYield();
 } else {
 }
}

/* PROMOTED 2026-10-02 — __scScheduleCore
 * Source:   module_campaign_20261002 scheduler core table audit
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
s32 __scScheduleCore(OSSched *scheduler, OSScTask **sp, OSScTask **dp, s32 availRCP)
{
    s32 avail = availRCP;
    OSScTask *gfx = scheduler->rspTaskTail;
    OSScTask *audio = scheduler->rspTaskHead;
    if (scheduler->audioListPending && (avail & 2)) {
        if (gfx && (gfx->flags & 0x10)) {
            *sp = gfx;
            avail &= ~2;
        } else {
            *sp = audio;
            avail &= ~2;
            scheduler->audioListPending = 0;
            scheduler->rspTaskHead = scheduler->rspTaskHead->next;
            if (scheduler->rspTaskHead == 0)
                scheduler->rdpTaskHead = 0;
        }
    } else {
        if (__scTaskReady(scheduler, gfx)) {
            switch (gfx->flags & 7) {
                case 3:
                    if (gfx->state & 0x20) {
                        if (avail & 2) {
                            *sp = gfx;
                            avail &= ~2;
                            if (gfx->state & 1) {
                                *dp = gfx;
                                avail &= ~1;
                                if ((avail & 1) == 0)
                                    if (scheduler->curRDPTask != gfx) { }
                            }
                            scheduler->rspTaskTail = scheduler->rspTaskTail->next;
                            if (scheduler->rspTaskTail == 0)
                                scheduler->rdpTaskTail = 0;
                        }
                    } else {
                        if (avail == 3) {
                            *sp = *dp = gfx;
                            avail &= ~3;
                            scheduler->rspTaskTail = scheduler->rspTaskTail->next;
                            if (scheduler->rspTaskTail == 0)
                                scheduler->rdpTaskTail = 0;
                        }
                    }
                    break;
                case 7:
                case 6:
                case 2:
                    if (gfx->state & 2) {
                        if (avail & 2) {
                            *sp = gfx;
                            avail &= ~2;
                        }
                    } else if (gfx->state & 1) {
                        if (avail & 1) {
                            *dp = gfx;
                            avail &= ~1;
                            scheduler->rspTaskTail = scheduler->rspTaskTail->next;
                            if (scheduler->rspTaskTail == 0)
                                scheduler->rdpTaskTail = 0;
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
