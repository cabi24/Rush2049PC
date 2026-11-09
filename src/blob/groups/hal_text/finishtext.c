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
        gFontAttribute &= 0x8000;
        for (i = 0; i < gBufLen;) {
            switch (gBuffer[i++]) {
                case 0:
                    HAL_SetFontInternal(gBuffer[i++]);
                    break;
                case 1:
                    HAL_SetFontLeading(gBuffer[i++]);
                    break;
                case 2:
                    HAL_SetFontSpacing(gBuffer[i++]);
                    break;
                case 3:
                    HAL_SetTextPos(((s16)gBuffer[i] << 8) | gBuffer[i + 1],
                                  ((s16)gBuffer[i + 2] << 8) | gBuffer[i + 3]);
                    i += 4;
                    break;
                case 4:
                    aLong = (gBuffer[i] << 24) | (gBuffer[i + 1] << 16) | (gBuffer[i + 2] << 8) | gBuffer[i + 3];
                    i += 4;
                    HAL_SetTextTranslucency(*(f32 *)&aLong);
                    break;
                case 5:
                    HAL_SetTextColor(gBuffer[i], gBuffer[i + 1], gBuffer[i + 2], gBuffer[i + 3]);
                    i += 4;
                    break;
                case 6:
                    length = gBuffer[i++];
                    DrawText(gBuffer + i, length);
                    i += length;
                    break;
            }
        }
        osSendMesg(&text_lock_queue, (void *)0, 0);
    }
}
