/* The scheduler (libultra's sched.c, as the game has it): a thread that serves the retraces and the RSP's and RDP's
 * events, sends each client (the main thread, the model thread, the audio manager, the audio retrace thread, the
 * rumble thread) a message each retrace, and runs the tasks the game sends (graphics frames and audio) on the RSP and
 * RDP, audio first, swapping the frame buffer when a frame is done. On the PC port/ultra is the hardware underneath:
 * the events are its, the tasks run in its renderer and audio interpreter (docs/port/ULTRA.md 3 and 11). The records
 * (PcSched, OSScTask, OSScClient, OSScMsg) are include/game/threads.h's. */

#include "rom_tu.h"

/* The scheduler's data, and the frame timer's (timer.c). */
volatile OSTime __osScRetraceTime = 0;   /* when the current frame's graphics task started: its retrace */
PcSched gScheduler = {0};
s32 scheduler_spare_word = 0;
s16 gRenderInProgress = 0;               /* the frames sent and not yet swapped in (game/system.h) */
volatile OSTime __osScAudioStart = 0;    /* when the last audio task started */
volatile OSTime __osScFrameTime = 0;     /* the last retrace's time */
u64 scheduler_spare_time = 0;
f32 gSeconds = 0;
f32 gFrameSeconds = 0;
s32 gLastFrameCount = 0;
s32 gCountdownEnd = 0;
s32 gCountdownStart = 0;
volatile s32 gLastFrameCounter = 0;

extern OSViMode osViModeTable[];
extern void *__osScPendingSwap;          /* the frame buffer waiting to be shown, or NULL */
extern u32 __osScSwapCount;              /* the retrace count at the last swap */
extern s32 frame_cycle_count;            /* the RDP's time for the last frame, in ticks */
extern OSScTask *scheduler_audio_task;   /* the last audio task started */
extern void osCreateViManager(OSPri);
extern void osViSetMode(void *), osViBlack(s32);
extern void osViSetEvent(OSMesgQueue *, OSMesg, s32);
extern void *osViGetCurrentFramebuffer(void), *osViGetNextFramebuffer(void);
extern void osViSwapBuffer(void *), video_apply_pending_mode(void);
extern u32 osSetIntMask(u32);
extern void osWritebackDCacheAll(void);
extern void osSpTaskLoad(void *), osSpTaskStartGo(void *), osSpTaskYield(void);
extern s32 osSpTaskYielded(void *);
extern s32 osDpSetNextBuffer(void *, u64);
void sched_main(void *arg);
void sched_schedule(PcSched *scheduler);
void sched_handle_retrace(PcSched *scheduler);
void sched_handle_rsp(PcSched *scheduler);
void sched_handle_rdp(PcSched *scheduler);
OSScTask *sched_task_ready(PcSched *scheduler, OSScTask *task);
s32 sched_task_done(PcSched *scheduler, OSScTask *task);
void sched_append_task(PcSched *scheduler, OSScTask *task);
void sched_start_tasks(PcSched *scheduler, OSScTask *sp, OSScTask *dp);
void sched_yield_gfx(PcSched *scheduler);
s32 sched_choose_tasks(PcSched *scheduler, OSScTask **sp, OSScTask **dp, s32 availRCP);

/* The processors (availRCP): the RSP and the RDP. */
#define OS_SC_SP 0x2
#define OS_SC_DP 0x1

/* Makes the scheduler: no tasks, no clients, its event and task queues, the VI manager and the video mode (from
 * libultra's mode table), the screen black; the RSP, RDP and pre-NMI events, the schedule event and the retrace
 * (every numFields fields) as commands on its event queue; then its thread at priority. */
void sched_create(PcSched *scheduler, void *stack_top, s32 priority, u8 mode, u8 numFields)
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
    scheduler->retraceMsg.type = OS_SC_RETRACE_MSG;
    scheduler->prenmiMsg.type = OS_SC_PRE_NMI_MSG;
    queue_init(&scheduler->eventQueue, scheduler->eventMsgs, 8);
    queue_init(&scheduler->taskQueue, scheduler->taskMsgs, 8);
    osCreateViManager(254);
    osViSetMode(&osViModeTable[mode]);
    osViBlack(1);
    event_notify(EVENT_SP, &scheduler->eventQueue, MESG_NUMBER(SCHED_CMD_RSP_DONE));
    event_notify(EVENT_DP, &scheduler->eventQueue, MESG_NUMBER(SCHED_CMD_RDP_DONE));
    event_notify(EVENT_PRE_NMI, &scheduler->eventQueue, MESG_NUMBER(SCHED_CMD_PRE_NMI));
    event_notify(EVENT_SCHEDULE, &scheduler->eventQueue, MESG_NUMBER(SCHED_CMD_SCHEDULE));
    osViSetEvent(&scheduler->eventQueue, MESG_NUMBER(SCHED_CMD_RETRACE), numFields);
    thread_create(&scheduler->thread, THREAD_ID_SCHEDULER, sched_main, scheduler, stack_top, priority);
    thread_start(&scheduler->thread);
}

/* Adds client to the scheduler's clients, which get a message on queue each retrace. With interrupts off. */
void sched_add_client(PcSched *scheduler, OSScClient *client, OSMesgQueue *queue)
{
    u32 mask;
    mask = osSetIntMask(1);
    client->msgQueue = queue;
    client->next = scheduler->clientList;
    scheduler->clientList = client;
    osSetIntMask(mask);
}

/* The scheduler's thread: forever, a command from its event queue: a retrace (the frame time, the clients told,
 * the next tasks started), the RSP done, the RDP done (the RDP's time for the frame), a request to schedule, or a
 * pre-NMI (every client gets the pre-NMI message, at the front of its queue). */
void sched_main(void *arg)
{
    PcSched *scheduler = (PcSched *)arg;
    OSScClient *client;
    while (1) {
        switch (MESG_TO_NUMBER(queue_recv(&scheduler->eventQueue))) {
        case SCHED_CMD_RETRACE:
            __osScFrameTime = time_ticks();
            sched_handle_retrace(scheduler);
            sched_schedule(scheduler);
            break;
        case SCHED_CMD_RSP_DONE:
            sched_handle_rsp(scheduler);
            break;
        case SCHED_CMD_RDP_DONE:
            frame_cycle_count = (s32)(time_ticks() - __osScRetraceTime);
            sched_handle_rdp(scheduler);
            break;
        case SCHED_CMD_SCHEDULE:
            sched_schedule(scheduler);
            break;
        case SCHED_CMD_PRE_NMI:
            for (client = scheduler->clientList; client != 0; client = client->next)
                queue_jam(client->msgQueue, &scheduler->prenmiMsg);
            break;
        }
    }
}

/* Takes the tasks the game has sent into the scheduler's lists, then starts what the RSP and RDP can run: when an
 * audio task waits while a graphics task runs, the graphics task is asked to yield. */
void sched_schedule(PcSched *scheduler)
{
    OSMesg message;
    s32 state;
    OSScTask *sp = 0;
    OSScTask *dp = 0;
    while (queue_try_recv(&scheduler->taskQueue, &message))
        sched_append_task(scheduler, (OSScTask *)message);
    if (scheduler->doAudio && scheduler->audioListHead && scheduler->curRSPTask) {
        sched_yield_gfx(scheduler);
    } else {
        state = ((scheduler->curRSPTask == 0) << 1) | (scheduler->curRDPTask == 0);
        if (sched_choose_tasks(scheduler, &sp, &dp, state) != state)
            sched_start_tasks(scheduler, sp, dp);
    }
}

/* A retrace: counts it; a frame buffer waiting to be shown is swapped in once PC_SWAP_RETRACES have passed since
 * the last swap (the pending video mode with it); and every client gets the retrace message (when its queue has
 * room). */
void sched_handle_retrace(PcSched *scheduler)
{
    OSScClient *client;

    scheduler->retraceCount++;
    if (__osScPendingSwap && scheduler->retraceCount - __osScSwapCount >= PC_SWAP_RETRACES) {
        osViSwapBuffer(__osScPendingSwap);
        video_apply_pending_mode();
        __osScSwapCount = scheduler->retraceCount;
        __osScPendingSwap = NULL;
    }
    for (client = scheduler->clientList; client != 0; client = client->next) {
        queue_try_send(client->msgQueue, &scheduler->retraceMsg);
    }
}

/* The RSP finished or yielded: a yielded graphics task goes back to the head of the graphics list (to run on), a
 * finished one is done with the RSP (and is done if the RDP is too); then the next tasks. */
void sched_handle_rsp(PcSched *scheduler)
{
    OSScTask *task, *sp = 0, *dp = 0;
    s32 state;
    task = scheduler->curRSPTask;
    scheduler->curRSPTask = 0;
    if ((task->state & OS_SC_YIELD) && osSpTaskYielded(&task->list)) {
        task->state |= OS_SC_YIELDED;
        if ((task->flags & OS_SC_TYPE_MASK) == OS_SC_XBUS) {
            task->next = scheduler->gfxListHead;
            scheduler->gfxListHead = task;
            if (scheduler->gfxListTail == 0)
                scheduler->gfxListTail = task;
        }
    } else {
        task->state &= ~OS_SC_SP;
        sched_task_done(scheduler, task);
    }
    state = ((scheduler->curRSPTask == 0) << 1) | (scheduler->curRDPTask == 0);
    if (sched_choose_tasks(scheduler, &sp, &dp, state) != state)
        sched_start_tasks(scheduler, sp, dp);
}

/* The RDP finished its task: it is done with the RDP (and is done if the RSP is too); then the next tasks. */
void sched_handle_rdp(PcSched *scheduler)
{
    OSScTask *task, *sp = 0, *dp = 0;
    s32 state;
    task = scheduler->curRDPTask;
    scheduler->curRDPTask = 0;
    task->state &= ~OS_SC_DP;
    sched_task_done(scheduler, task);
    state = ((scheduler->curRSPTask == 0) << 1) | (scheduler->curRDPTask == 0);
    if (sched_choose_tasks(scheduler, &sp, &dp, state) != state)
        sched_start_tasks(scheduler, sp, dp);
}

/* Whether graphics task task can run: the last frame buffer swapped is on screen, and no swap waits. */
OSScTask *sched_task_ready(PcSched *scheduler, OSScTask *task)
{
    if (task != 0) {
        if (osViGetCurrentFramebuffer() != osViGetNextFramebuffer())
            return 0;
        if (__osScPendingSwap != 0 && scheduler->retraceCount - __osScSwapCount < PC_SWAP_RETRACES)
            return 0;
        return task;
    }
    return 0;
}

/* If task has finished on both processors: tells the game (its message), and for a graphics frame counts it out
 * of gRenderInProgress and swaps its frame buffer, now or (too soon after the last) later. Returns 1 then, else 0. */
s32 sched_task_done(PcSched *scheduler, OSScTask *task)
{
    if ((task->state & (OS_SC_SP | OS_SC_DP)) == 0) {
        queue_send(task->msgQueue, task->msg);
        if (task->list.type == M_GFXTASK) {
            gRenderInProgress--;
            if ((task->flags & OS_SC_SWAPBUFFER) && (task->flags & OS_SC_LAST_TASK)) {
                if (scheduler->retraceCount - __osScSwapCount >= PC_SWAP_RETRACES) {
                    __osScSwapCount = scheduler->retraceCount;
                    osViSwapBuffer(task->framebuffer);
                    video_apply_pending_mode();
                } else {
                    __osScPendingSwap = task->framebuffer;
                }
            }
        }
        return 1;
    }
    return 0;
}

/* Appends task to the audio list or the graphics list, by its type (an audio task makes audio pending), with its
 * state from its flags. */
void sched_append_task(PcSched *scheduler, OSScTask *task)
{
    if (task->list.type == M_AUDTASK) {
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
void sched_start_tasks(PcSched *scheduler, OSScTask *sp, OSScTask *dp)
{
    if (sp) {
        if (sp->list.type == M_AUDTASK) {
            __osScAudioStart = time_ticks();
            scheduler_audio_task = sp;
        } else if (!(sp->state & OS_SC_YIELDED)) {
            __osScRetraceTime = __osScFrameTime;
        }
        osWritebackDCacheAll();
        sp->state &= ~(OS_SC_YIELD | OS_SC_YIELDED);
        osSpTaskLoad(&sp->list);
        osSpTaskStartGo(&sp->list);
        scheduler->curRSPTask = sp;
        if (sp == dp)
            scheduler->curRDPTask = dp;
    }
    if (dp && dp != sp) {
        osDpSetNextBuffer(dp->list.output_buff, *dp->list.output_buff_size);
        scheduler->curRDPTask = dp;
    }
}

/* Asks the graphics task on the RSP to yield (for the audio task waiting). */
void sched_yield_gfx(PcSched *scheduler)
{
    if (scheduler->curRSPTask->list.type == M_GFXTASK) {
        scheduler->curRSPTask->state |= OS_SC_YIELD;
        osSpTaskYield();
    }
}

/* Chooses the next RSP and RDP tasks for the processors free in availRCP (bit 1 the RSP, bit 0 the RDP): a
 * pending audio task first (unless a yielded graphics task must finish), else the graphics list's head when its
 * frame buffer may be drawn, on the processors its flags and state call for; repeats while it finds more. Returns
 * the processors still free. */
s32 sched_choose_tasks(PcSched *scheduler, OSScTask **sp, OSScTask **dp, s32 availRCP)
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
        if (sched_task_ready(scheduler, gfx)) {
            switch (gfx->flags & OS_SC_TYPE_MASK) {
                case OS_SC_XBUS:
                    if (gfx->state & OS_SC_YIELDED) {
                        if (avail & OS_SC_SP) {
                            *sp = gfx;
                            avail &= ~OS_SC_SP;
                            if (gfx->state & OS_SC_DP) {
                                *dp = gfx;
                                avail &= ~OS_SC_DP;
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
        avail = sched_choose_tasks(scheduler, sp, dp, avail);
    return avail;
}
