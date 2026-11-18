/* musyx_submit_audio_task: one of the MusyX library's platform services (sound_set_services): runs an audio task
 * on the RSP through the scheduler. On the PC the scheduler hands the task to port/audio, which interprets its
 * subframe descriptors (docs/port/AUDIO.md). */
#include "game/game.h"

extern void *memcpy(void *, const void *, unsigned int);

/* Copies task (the library's RSP task, an OSTask) into the one scheduler task and sends it as an audio task (it needs
 * the RSP) that reports to musyx_task_done_queue when done (musyx_wait_audio_task waits for it), then wakes the
 * scheduler to look at it now. */
void musyx_submit_audio_task(void *task)
{
    static OSScTask scTask;

    scTask.next = 0;
    scTask.msgQueue = &musyx_task_done_queue;
    scTask.msg = 0;
    scTask.flags = OS_SC_NEEDS_RSP;
    memcpy(&scTask.list, task, sizeof(OSTask));
    queue_send(&gScheduler.taskQueue, &scTask);
    queue_send(&gScheduler.eventQueue, MESG_NUMBER(SCHED_CMD_SCHEDULE));
}
