/* HAL_StartText: begin a frame's text (MAT3 HAL_text.c). */
#include "game/game.h"

/* Starts a frame's text, under the text lock: the default state (SetDefaults) in the current font, the blitter
 * attributes cleared but for BLIT_ATTR_TALL on a screen taller than 220 lines, the buffer empty. */
void HAL_StartText(void)
{
    lock_take(&text_lock_queue);
    SetDefaults((u8 *)&gFontColor, &gTrans, &gFontXpos, &gFontYpos, &gFontIndex, &gLeading, &gSpacing);
    HAL_SetFontInternal(gFontIndex);
    gFontAttribute = 0;
    if (screen_height > 220) {
        gFontAttribute |= BLIT_ATTR_TALL;
    }
    gBufferInited = 0;
    gFinished = 0;
    lock_give(&text_lock_queue);
}

/* stand-in caller (context, kept): keeps SetDefaults out of line as in retail (three callers there) */
