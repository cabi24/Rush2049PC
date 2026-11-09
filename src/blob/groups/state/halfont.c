#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct { u32 opaque[6]; } OSMesgQueue;
VIEW(OSMesgQueue, text_lock_queue);
#define text_lock_queue text_lock_queue_view
s32 osRecvMesg(OSMesgQueue *mq, void **msg, s32 flag);
s32 osSendMesg(OSMesgQueue *mq, void *msg, s32 flag);

s32 HAL_SetFont(s32 fontID)           /* HAL_SetFont */
{
    s32 old;
    text_lock();
    old = HAL_SetFontInternal(fontID);
    text_unlock();
    return old;
}

void text_lock(void)
{
    osRecvMesg(&text_lock_queue, (void **)0, 1);
}

void text_unlock(void)
{
    osSendMesg(&text_lock_queue, (void *)0, 0);
}
