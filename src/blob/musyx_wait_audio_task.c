/* musyx_wait_audio_task: the MusyX sound library's service that waits for its audio task (one of its eight
 * services, docs/code/r5.md "MusyX"; N64 only, no MAT3 counterpart). */
#include "game/game.h"

s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);

/* Block until the scheduler reports the audio task musyx_submit_audio_task sent done (a message on
 * musyx_task_done_queue). */
void musyx_wait_audio_task(void) {
    osRecvMesg((OSMesgQueue *) &musyx_task_done_queue, NULL, 1);
}
