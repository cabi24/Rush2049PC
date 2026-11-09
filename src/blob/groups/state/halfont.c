/* HAL_SetFont and the text lock (MAT3 HAL_text.c; its _GetSemaphore and _ReleaseSemaphore are text_lock and
 * text_unlock). */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct { u32 opaque[6]; } OSMesgQueue;
s32 osRecvMesg(OSMesgQueue *mq, void **msg, s32 flag);
s32 osSendMesg(OSMesgQueue *mq, void *msg, s32 flag);

/* Makes fontID the current font (loading its waves if need be: HAL_SetFontInternal), under the text lock, and returns
 * the font that was current. */
s32 HAL_SetFont(s32 fontID)
{
    s32 old;
    text_lock();
    old = HAL_SetFontInternal(fontID);
    text_unlock();
    return old;
}

/* Takes the text lock: waits for the one message of text_lock_queue (HAL_InitText). */
void text_lock(void)
{
    osRecvMesg(&text_lock_queue, (void **)0, 1);
}

/* Gives the text lock back. */
void text_unlock(void)
{
    osSendMesg(&text_lock_queue, (void *)0, 0);
}
