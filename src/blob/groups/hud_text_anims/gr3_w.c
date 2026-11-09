/* HAL_StartText: begin a frame's text (MAT3 HAL_text.c). */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct { s32 opaque[6]; } OSMesgQueue;
VIEW(OSMesgQueue, text_lock_queue);
#define text_lock_queue text_lock_queue_view
extern s32 osRecvMesg(OSMesgQueue *, void **, s32);
extern s32 osSendMesg(OSMesgQueue *, void *, s32);

VIEW(s32, gFontColor);
#define gFontColor gFontColor_view

VIEW(s32, gFontAttribute);
#define gFontAttribute gFontAttribute_view

/* Starts a frame's text, under the text lock: the default state (SetDefaults) in the current font, the blitter
 * attributes cleared but for BLIT_ATTR_TALL on a screen taller than 220 lines, the buffer empty. */
void HAL_StartText(void)
{
    osRecvMesg(&text_lock_queue, (void *)0, 1);
    SetDefaults((u8 *)&gFontColor, &gTrans, &gFontXpos, &gFontYpos, &gFontIndex, &gLeading, &gSpacing);
    HAL_SetFontInternal(gFontIndex);
    gFontAttribute = 0;
    if (screen_height > 220) {
        gFontAttribute |= BLIT_ATTR_TALL;
    }
    gBufferInited = 0;
    gFinished = 0;
    osSendMesg(&text_lock_queue, (void *)0, 0);
}

/* stand-in caller (context, kept): keeps SetDefaults out of line as in retail (three callers there) */
