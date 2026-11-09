/* musyx_submit_audio_task: one of the MusyX library's platform services (sound_set_services): runs an audio task
 * on the RSP through the scheduler. On the PC the scheduler hands the task to port/audio, which interprets its
 * subframe descriptors (docs/port/AUDIO.md). N64-shaped: the OSScTask and the scheduler's queues. */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */
#define OSTask OSTaskView   /* this file's own OSTask: include/game's OSTask is another type */

typedef struct OSMesgQueue OSMesgQueue;

typedef union {
    OSTask_t t;
    long long force_structure_alignment;
} OSTask;

/* The scheduler's task record (libultra's sched.h). */
typedef struct OSScTask_s {
    struct OSScTask_s *next;
    u32 state;
    u32 flags;
    void *framebuffer;
    OSTask list;
    OSMesgQueue *msgQ;
    OSMesg msg;
} OSScTask;

extern OSMesgQueue scheduler_task_queue;
extern OSMesgQueue scheduler_interrupt_queue;
extern void *memcpy(void *, const void *, unsigned int);
extern int osSendMesg(OSMesgQueue *, OSMesg, int);

/* The scheduler task's flags: an audio task (libultra's OS_SC_NEEDS_RSP). */
#define SC_TASK_AUDIO 2
/* The message that asks the scheduler to look at its task queue now. */
#define SCHEDULER_MSG_TASK_QUEUED (OSMesg)670

/* Copies task into the one scheduler record and queues it as an audio task that reports to
 * musyx_task_done_queue when done (musyx_wait_audio_task waits for it), then wakes the scheduler. */
void musyx_submit_audio_task(OSTask *task)
{
    static OSScTask scTask;

    scTask.next = 0;
    scTask.msgQ = &musyx_task_done_queue;
    scTask.msg = 0;
    scTask.flags = SC_TASK_AUDIO;
    memcpy(&scTask.list, task, sizeof(OSTask));
    osSendMesg(&scheduler_task_queue, (OSMesg)&scTask, 1);
    osSendMesg(&scheduler_interrupt_queue, SCHEDULER_MSG_TASK_QUEUED, 1);
}
