/* HAL_InitText: start the text system (MAT3 HAL_text.c). */
#include "game/game.h"

/* Makes the text lock once (a one-message queue holding its one message: text_lock takes it, text_unlock gives it
 * back; MAT3's semaphore) and forgets the current font, so the next HAL_SetFont loads one. */
void HAL_InitText(void)
{
    const s8 initialized = 1;

    if (!text_lock_inited) {
        text_lock_inited = initialized;
        queue_init(&text_lock_queue, text_lock_msg, 1);
        lock_give(&text_lock_queue);
    }
    gFontIndex = -1;
}
