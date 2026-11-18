/* include/game/threads.h: the game's threads, message queues, events and scheduler (docs/port/ULTRA.md 1 and 11).
 *
 * The game runs on threads that pass messages through queues. On the PC each thread is a host thread, and port/ultra
 * (sched.c) runs one of them at a time: the holder of the baton runs game code, the others wait. The baton moves
 * only inside the calls below, by the game's priorities (THREAD_PRI_*): the highest-priority thread that can run
 * runs, equal priorities in the order they became ready, and a call that wakes a higher-priority thread hands it the
 * baton at once. So a run is the same every time (docs/port/LOCKSTEP.md).
 *
 * A queue (OSMesgQueue) and a thread (OSThread) are records of the game's; port/ultra keeps their state in its own
 * tables, under the record's address, and never reads or writes the record. A message is a pointer-sized value: a
 * pointer to a record, or a number (MESG_NUMBER). Each queue's comment says which.
 *
 * Both the game's files (game/game.h, which declares OSMesgQueue, OSThread and OSTask_t in game/types.h) and the boot
 * code's (src/rom, whose rom_tu.h declares them) include this header. */
#ifndef GAME_THREADS_H
#define GAME_THREADS_H

/* ---- Threads */

/* The game's threads (src/rom/boot.c): ids, and priorities (the higher runs first). */
#define THREAD_ID_IDLE 1        /* main's first thread: starts the PI manager and the main thread, then idles */
#define THREAD_ID_RUMBLE 2      /* the rumble thread (paksysdc's rumble_thread_entry) */
#define THREAD_ID_SAMPLE_DMA 3  /* the sound library's sample loader (sample_dma_thread_entry) */
#define THREAD_ID_SCHEDULER 4   /* the scheduler (sched_main) */
#define THREAD_ID_MODEL 5       /* the car simulation (ModelThread) */
#define THREAD_ID_MAIN 6        /* start-up, then the scheduler's client that starts and ends frames */
#define THREAD_ID_GAME 7        /* GameLoop forever */
#define THREAD_ID_BG_LOAD 8     /* the background wave loader (BGLoadWaveMain) */
#define THREAD_ID_AUDIO_RETRACE 9   /* the sound library's clock (audio_retrace_thread_entry) */
#define THREAD_ID_AUDIO_MANAGER 10  /* the audio manager (audio_manager_thread_entry) */
#define THREAD_PRI_IDLE 0       /* the idle thread once it has started the main thread */
#define THREAD_PRI_IDLE_START 2 /* the idle thread until then */
#define THREAD_PRI_BG_LOAD 3
#define THREAD_PRI_MAIN 4
#define THREAD_PRI_GAME 5
#define THREAD_PRI_RUMBLE 6
#define THREAD_PRI_MODEL 7
#define THREAD_PRI_AUDIO_RETRACE 9
#define THREAD_PRI_AUDIO_MANAGER 10
#define THREAD_PRI_SAMPLE_DMA 10
#define THREAD_PRI_SCHEDULER 12
#define THREAD_PRI_PI_MANAGER 150

/* ---- libultra's interface underneath (port/ultra/sched.c and timer.c). The libraries the game links call it as
 * libultra's C does: libultra's own (the controllers and the pak file system, src/rom/libultra), the MusyX sound
 * driver (src/rom/sound) and the speech decoder (src/rom/speech). The game's C calls the API below, which says the
 * same things by name: each of its calls is one of these, with the same blocking, waking and baton. */
#ifndef OS_MESG_BLOCK
#define OS_MESG_NOBLOCK 0
#define OS_MESG_BLOCK 1
#endif
void osCreateThread(OSThread *thread, OSId id, void (*entry)(void *), void *arg, void *sp, OSPri priority);
void osStartThread(OSThread *thread);
void osStopThread(OSThread *thread);
void osSetThreadPri(OSThread *thread, OSPri priority);
OSId osGetThreadId(OSThread *thread);
void osYieldThread(void);
void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);    /* -1: OS_MESG_NOBLOCK and the queue is full */
s32 osJamMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);   /* -1: OS_MESG_NOBLOCK and the queue is empty */
void osSetEventMesg(u32 event, OSMesgQueue *mq, OSMesg msg);
OSTime osGetTime(void);

/* Makes thread a thread that will run entry(arg) with that id and priority, stopped until thread_start. stack_top
 * is the top of the stack the N64 gave it; on the PC each thread has a host stack of its own, so it is not used. */
static inline void thread_create(OSThread *thread, s32 id, void (*entry)(void *), void *arg, void *stack_top,
                                 s32 priority)
{
    osCreateThread(thread, id, entry, arg, stack_top, priority);
}
/* Lets a stopped thread run (it takes the baton at once if its priority is higher than the caller's). The boot
 * context's first call never returns. */
static inline void thread_start(OSThread *thread) { osStartThread(thread); }
/* Stops thread until thread_start; NULL: the calling thread. A thread stopped while it waits on a queue looks at
 * the queue again when it is started. */
static inline void thread_stop(OSThread *thread) { osStopThread(thread); }
/* Sets thread's priority (NULL: the calling thread's); the baton moves if that makes another thread first. */
static inline void thread_set_priority(OSThread *thread, s32 priority) { osSetThreadPri(thread, priority); }
/* thread's id (NULL: the calling thread's). */
static inline s32 thread_id(OSThread *thread) { return osGetThreadId(thread); }
/* Gives the baton to the next thread of the caller's priority, if one can run. */
static inline void thread_yield(void) { osYieldThread(); }

/* ---- Message queues */

/* A message: a pointer-sized value. A number travels as MESG_NUMBER(n) and comes back with MESG_TO_NUMBER(m). */
#define MESG_NUMBER(n) ((OSMesg)(intptr_t)(n))
#define MESG_TO_NUMBER(m) ((intptr_t)(m))

/* Makes queue empty, with room for count messages. slots is the game's array for them; port/ultra keeps the
 * messages in its own (a slot is 8 bytes on the PC, 4 on the N64), so it is not used. */
static inline void queue_init(OSMesgQueue *queue, OSMesg *slots, s32 count) { osCreateMesgQueue(queue, slots, count); }
/* Puts message at the back of queue, waiting while the queue is full; a thread waiting on it is woken. */
static inline void queue_send(OSMesgQueue *queue, OSMesg message) { osSendMesg(queue, message, OS_MESG_BLOCK); }
/* Puts message at the back of queue if it has room: 1, else 0 (the queue is full; nothing is sent). */
static inline s32 queue_try_send(OSMesgQueue *queue, OSMesg message)
{
    return osSendMesg(queue, message, OS_MESG_NOBLOCK) != -1;
}
/* Puts message at the front of queue (it is the next one taken), waiting while the queue is full. */
static inline void queue_jam(OSMesgQueue *queue, OSMesg message) { osJamMesg(queue, message, OS_MESG_BLOCK); }
/* Takes the message at the front of queue, waiting while the queue is empty. */
static inline OSMesg queue_recv(OSMesgQueue *queue)
{
    OSMesg message = 0;
    osRecvMesg(queue, &message, OS_MESG_BLOCK);
    return message;
}
/* Takes the message at the front of queue into *message (if message isn't NULL): 1, else 0 (the queue is empty). */
static inline s32 queue_try_recv(OSMesgQueue *queue, OSMesg *message)
{
    return osRecvMesg(queue, message, OS_MESG_NOBLOCK) != -1;
}

/* ---- Locks: a queue of one message, the token, which is in the queue while the lock is free (lock_give puts it
 * there first). Taking the lock takes the token, waiting for it; giving it back sends it again. */
static inline void lock_take(OSMesgQueue *lock) { osRecvMesg(lock, NULL, OS_MESG_BLOCK); }
/* Takes the lock if it is free: 1, else 0. */
static inline s32 lock_try(OSMesgQueue *lock) { return osRecvMesg(lock, NULL, OS_MESG_NOBLOCK) != -1; }
/* Gives the lock back: the token, if the queue has room (a lock given twice still holds one). */
static inline void lock_give(OSMesgQueue *lock) { osSendMesg(lock, NULL, OS_MESG_NOBLOCK); }

/* ---- Events: port/ultra's devices and the scheduler's software interrupt. event_notify(event, queue, message)
 * sends message to queue each time event happens (a later call for the event replaces the queue and message). */
enum {
    EVENT_SCHEDULE = 0,    /* the software interrupt the scheduler is woken by (libultra's OS_EVENT_SW1) */
    EVENT_SP = 4,          /* the RSP finished or yielded its task */
    EVENT_SI = 5,          /* the serial bus (the controllers and their paks) finished a transfer */
    EVENT_AI = 6,          /* the audio interface wants its next buffer */
    EVENT_DP = 9,          /* the RDP finished its task */
    EVENT_PRE_NMI = 14     /* the reset button: the console resets in half a second */
};
static inline void event_notify(s32 event, OSMesgQueue *queue, OSMesg message)
{
    osSetEventMesg(event, queue, message);
}

/* ---- Time: the N64's clock, 46.875 MHz (half its CPU's), since start-up (the virtual clock with --fast). */
static inline u64 time_ticks(void) { return osGetTime(); }

/* ---- The scheduler (src/rom/scheduler.c: libultra's sched.c, as the game has it). Its thread serves the retraces
 * and the RSP's and RDP's events, sends each client a message each retrace, and runs the tasks the game sends it
 * (graphics frames and audio) on the RSP and the RDP, audio first, swapping the frame buffer when a frame is done. */

/* A scheduler message: its type. The scheduler sends its clients its retraceMsg each retrace and its prenmiMsg on
 * the reset button; a finished graphics task sends the main thread its frame's doneMsg; the game thread sends the
 * main thread one asking it to end the frame (UpdateFrame). A client reads the type and nothing more. */
typedef struct OSScMsg {
    s16 type;              /* OS_SC_*_MSG, GAME_MSG_END_FRAME */
    s8 unused[30];         /* (libultra's room for more) */
} OSScMsg;
#define OS_SC_RETRACE_MSG 1
#define OS_SC_DONE_MSG 2
#define OS_SC_PRE_NMI_MSG 4
#define GAME_MSG_END_FRAME 0xABE    /* 2750: the game thread asks the main thread to end the frame (UpdateFrame) */
#define GAME_MSG_START_FRAME 0xABF  /* frame_start_msg: the main thread tells the game thread a frame has begun */

/* A client: a queue the scheduler sends its OSScMsg pointers to, in a list the scheduler keeps. */
typedef struct OSScClient_s {
    struct OSScClient_s *next;
    OSMesgQueue *msgQueue;
} OSScClient;

/* A task: what the RSP (and the RDP) is to run, and whom to tell when it is done. The game builds two kinds: a frame
 * (FrameRecord's, EndGraphics) and the sound library's audio task (musyx_submit_audio_task). */
typedef struct OSScTask_s {
    struct OSScTask_s *next;   /* in the scheduler's audio or graphics list */
    u32 state;                 /* OS_SC_NEEDS_RSP and _RDP while those processors still owe it; _YIELD, _YIELDED */
    u32 flags;                 /* OS_SC_NEEDS_RSP, _RDP, _PARALLEL_TASK, _LAST_TASK, _SWAPBUFFER */
    void *framebuffer;         /* a frame: the frame buffer it draws into, swapped in when it is done */
    OSTask_t list;             /* the RSP task itself (port/ultra's rcp.c runs it) */
    OSMesgQueue *msgQueue;     /* when done, msg is sent here */
    OSMesg msg;
} OSScTask;
/* A task's flags and state (OSScTask.flags, .state). */
#define OS_SC_NEEDS_RDP 0x1
#define OS_SC_NEEDS_RSP 0x2
#define OS_SC_RCP_MASK 0x3
#define OS_SC_TYPE_MASK 0x7
#define OS_SC_XBUS 0x3            /* both processors, the RDP reading from the RSP */
#define OS_SC_PARALLEL_TASK 0x10
#define OS_SC_LAST_TASK 0x20
#define OS_SC_SWAPBUFFER 0x40
#define OS_SC_YIELD 0x10
#define OS_SC_YIELDED 0x20

/* The scheduler's commands: the numbers its event queue carries (MESG_NUMBER). */
enum {
    SCHED_CMD_RETRACE = 666,    /* a retrace (the video interface's event) */
    SCHED_CMD_RSP_DONE = 667,   /* EVENT_SP */
    SCHED_CMD_RDP_DONE = 668,   /* EVENT_DP */
    SCHED_CMD_PRE_NMI = 669,    /* EVENT_PRE_NMI */
    SCHED_CMD_SCHEDULE = 670    /* EVENT_SCHEDULE, or a task's sender: look at the task queue now */
};

/* The scheduler (gScheduler; libultra's OSSched). */
typedef struct PcSched {
    OSScMsg retraceMsg;              /* sent to every client each retrace (type OS_SC_RETRACE_MSG) */
    OSScMsg prenmiMsg;               /* put at the front of every client's queue on the reset button */
    OSMesgQueue eventQueue;          /* the scheduler thread's: SCHED_CMD_* numbers */
    OSMesg eventMsgs[8];
    OSMesgQueue taskQueue;           /* the tasks the game sends (OSScTask pointers) */
    OSMesg taskMsgs[8];
    OSThread thread;                 /* the scheduler's thread (THREAD_ID_SCHEDULER) */
    OSScClient *clientList;
    OSScTask *audioListHead;         /* the audio tasks waiting for the RSP */
    OSScTask *gfxListHead;           /* the graphics tasks waiting for the RSP or the RDP */
    OSScTask *audioListTail;
    OSScTask *gfxListTail;
    OSScTask *volatile curRSPTask;   /* the task on the RSP (the audio retrace thread waits for its audio task) */
    OSScTask *curRDPTask;            /* the task on the RDP */
    volatile s32 retraceCount;       /* the retraces since start-up: the game's frame clock */
    s32 doAudio;                     /* an audio task waits */
} PcSched;

/* Makes the scheduler: its queues, the screen black (mode, an index into osViModeTable, set the N64 video interface's
 * mode, which the PC doesn't have), a retrace command every numFields retraces, the RSP's, RDP's, pre-NMI and schedule events as commands; then its thread at
 * priority, with stack_top unused (thread_create). The game sends a task with queue_send(&gScheduler.taskQueue,
 * task), and asks for it to be looked at now with SCHED_CMD_SCHEDULE on gScheduler.eventQueue. */
void sched_create(PcSched *scheduler, void *stack_top, s32 priority, u8 mode, u8 numFields);
/* Adds client to the scheduler's clients: queue gets the scheduler's messages from the next retrace on. */
void sched_add_client(PcSched *scheduler, OSScClient *client, OSMesgQueue *queue);

#endif /* GAME_THREADS_H */
