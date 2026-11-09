/* HAL_InitText: start the text system (MAT3 HAL_text.c). */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct {
    void *mtqueue;
    void *fullqueue;
    s32 validCount;
    s32 first;
    s32 msgCount;
    OSMesg *msg;
} OSMesgQueue;

void osCreateMesgQueue(OSMesgQueue *, OSMesg *, s32);
s32 osSendMesg(OSMesgQueue *, OSMesg, s32);

/* Makes the text lock once (a one-message queue holding its one message: text_lock takes it, text_unlock gives it
 * back; MAT3's semaphore) and forgets the current font, so the next HAL_SetFont loads one. */
void HAL_InitText(void)
{
    const s8 initialized = 1;

    if (!text_lock_inited) {
        text_lock_inited = initialized;
        osCreateMesgQueue(&text_lock_queue, &text_lock_msg, 1);
        osSendMesg(&text_lock_queue, (OSMesg)0, 0);
    }
    gFontIndex = -1;
}
