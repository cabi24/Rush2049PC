#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;

VIEW(s32, audio_lock_queue);
#define audio_lock_queue audio_lock_queue_view
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);

void audio_lock(void) {
    osRecvMesg((OSMesgQueue *) &audio_lock_queue, NULL, 1);
}

