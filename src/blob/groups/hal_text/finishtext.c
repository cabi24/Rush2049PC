/* HAL_FinishText: draw the frame's buffered text (MAT3 HAL_text.c). */
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

VIEW(u8[800], gBuffer);
#define gBuffer gBuffer_view

/* Runs the text commands HAL_DrawText buffered since HAL_StartText (TEXT_CMD_*), drawing the strings with DrawText,
 * from the default font, colour and position; under the text lock. end becomes gFinished (HAL_StartText clears it
 * again). Nothing when no text was buffered. */
void HAL_FinishText(s8 end)
{
    s32 i;
    s32 length;
    u32 aLong;

    if (gBufferInited) {
        osRecvMesg(&text_lock_queue, (void **)0, 1);
        gFinished = end;
        SetDefaults((u8 *)&gFontColor, &gTrans, &gFontXpos, &gFontYpos, &gFontIndex, &gLeading, &gSpacing);
        HAL_SetFontInternal(gFontIndex);
        gFontAttribute &= BLIT_ATTR_TALL;
        for (i = 0; i < gBufLen;) {
            switch (gBuffer[i++]) {
                case TEXT_CMD_FONT:
                    HAL_SetFontInternal(gBuffer[i++]);
                    break;
                case TEXT_CMD_LEADING:
                    HAL_SetFontLeading(gBuffer[i++]);
                    break;
                case TEXT_CMD_SPACING:
                    HAL_SetFontSpacing(gBuffer[i++]);
                    break;
                case TEXT_CMD_POS:
                    HAL_SetTextPos(((s16)gBuffer[i] << 8) | gBuffer[i + 1],
                                  ((s16)gBuffer[i + 2] << 8) | gBuffer[i + 3]);
                    i += 4;
                    break;
                case TEXT_CMD_TRANSLUCENCY:
                    aLong = (gBuffer[i] << 24) | (gBuffer[i + 1] << 16) | (gBuffer[i + 2] << 8) | gBuffer[i + 3];
                    i += 4;
                    HAL_SetTextTranslucency(*(f32 *)&aLong);
                    break;
                case TEXT_CMD_COLOR:
                    HAL_SetTextColor(gBuffer[i], gBuffer[i + 1], gBuffer[i + 2], gBuffer[i + 3]);
                    i += 4;
                    break;
                case TEXT_CMD_TEXT:
                    length = gBuffer[i++];
                    DrawText(gBuffer + i, length);
                    i += length;
                    break;
            }
        }
        osSendMesg(&text_lock_queue, (void *)0, 0);
    }
}
