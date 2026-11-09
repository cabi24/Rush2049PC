#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */
#define OSTask OSTaskView   /* this file's own OSTask: include/game's OSTask is another type */

typedef struct OSMesgQueue OSMesgQueue;

typedef union {
    OSTask_t t;
    long long force_structure_alignment;
} OSTask;

typedef struct OSScTask_s {
    struct OSScTask_s *next;
    u32 state;
    u32 flags;
    void *framebuffer;
    OSTask list;
    OSMesgQueue *msgQ;
    OSMesg msg;
} OSScTask;

VIEW(OSMesgQueue, musyx_task_done_queue);
#define musyx_task_done_queue musyx_task_done_queue_view
extern OSMesgQueue scheduler_task_queue;
extern OSMesgQueue scheduler_interrupt_queue;
extern void *memcpy(void *, const void *, unsigned int);
extern int osSendMesg(OSMesgQueue *, OSMesg, int);

void musyx_submit_audio_task(OSTask *task)
{
    static OSScTask scTask;   /* 0x80155238 */

    scTask.next = 0;
    scTask.msgQ = &musyx_task_done_queue;
    scTask.msg = 0;
    scTask.flags = 2;
    memcpy(&scTask.list, task, sizeof(OSTask));
    osSendMesg(&scheduler_task_queue, (OSMesg)&scTask, 1);
    osSendMesg(&scheduler_interrupt_queue, (OSMesg)670, 1);
}
