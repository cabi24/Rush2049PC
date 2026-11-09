#include "game/game.h"

extern void *memcpy(void *, const void *, u32);

VIEW(u32, gFontColor);
#define gFontColor gFontColor_view

VIEW(u8[800], gBuffer);
#define gBuffer gBuffer_view


u32 TextLen(const u8 *text, s16 numChars)      /* _TextLen (MAT3 HAL_text.c) */
{
    u32 len;
    s32 nc;
    nc = 0;
    if (text[0] == 255) {
        len = 1;
        do {
            len += 2;
            nc++;
        } while ((text[len - 1] || text[len - 2]) && (numChars < 0 || nc <= numChars));
    } else {
        len = 0;
        do {
            len++;
            nc++;
        } while (*text++ && (numChars < 0 || nc <= numChars));
    }
    return len;
}

void HAL_DrawText(void *text, s16 numChars)
{
    s32 size;
    s32 length;
    u8 str[256];
    static u32 lastFontColor;    /* 0x80153F60 */
    static f32 lastTrans;    /* 0x80153F80 */
    static s16 lastFontXpos;    /* 0x80153FD0 */
    static s16 lastFontYpos;    /* 0x80154180 */
    static s8 lastFontIndex;    /* 0x80154184 */
    static s8 lastLeading;    /* 0x80154194 */
    static s8 lastSpacing;    /* 0x8015419C */

    if (gDepth >= 0.0f) {
        DrawText(text, TextLen(text, numChars));
        return;
    }
    if (gFinished) {
        if (numChars < 0) game_strcpy(str, text);
        else game_strncpy(str, text, numChars);
    }
    text_lock();
    if (!gBufferInited) {
        gBufferInited = 1;
        gBufLen = 0;
        SetDefaults(&lastFontColor, &lastTrans, &lastFontXpos, &lastFontYpos, &lastFontIndex, &lastLeading, &lastSpacing);
    }
    size = 0;
    if (lastFontIndex != gFontIndex) size = 2; else ;
    if (lastLeading != gLeading) size += 2;
    if (lastSpacing != gSpacing) size += 2;
    if (lastFontXpos != gFontXpos || lastFontYpos != gFontYpos) size += 5;
    if (lastTrans != gTrans) size += 5;
    if (lastFontColor != gFontColor) size += 5;
    length = TextLen(text, numChars);
    size += length + 2;
    if (gBufLen + size >= 800) {
        text_unlock();
        return;
    }
    if (lastFontIndex != gFontIndex) {
        lastFontIndex = gFontIndex;
        gBuffer[gBufLen++] = 0;
        gBuffer[gBufLen++] = gFontIndex;
    }
    if (lastLeading != gLeading) {
        lastLeading = gLeading;
        gBuffer[gBufLen++] = 1;
        gBuffer[gBufLen++] = gLeading;
    }
    if (lastSpacing != gSpacing) {
        lastSpacing = gSpacing;
        gBuffer[gBufLen++] = 2;
        gBuffer[gBufLen++] = gSpacing;
    }
    if (lastFontXpos != gFontXpos || lastFontYpos != gFontYpos) {
        lastFontXpos = gFontXpos;
        lastFontYpos = gFontYpos;
        gBuffer[gBufLen++] = 3;
        gBuffer[gBufLen++] = (lastFontXpos >> 8) & 255;
        gBuffer[gBufLen++] = lastFontXpos & 255;
        gBuffer[gBufLen++] = (lastFontYpos >> 8) & 255;
        gBuffer[gBufLen++] = lastFontYpos & 255;
    }
    if (lastTrans != gTrans) {
        lastTrans = gTrans;
        gBuffer[gBufLen++] = 4;
        gBuffer[gBufLen++] = (*(u32 *)&gTrans >> 24) & 255;
        gBuffer[gBufLen++] = (*(u32 *)&gTrans >> 16) & 255;
        gBuffer[gBufLen++] = (*(u32 *)&gTrans >> 8) & 255;
        gBuffer[gBufLen++] = (*(u32 *)&gTrans) & 255;
    }
    if (lastFontColor != gFontColor) {
        lastFontColor = gFontColor;
        gBuffer[gBufLen++] = 5;
        memcpy(gBuffer + gBufLen, &gFontColor, 4);
        gBufLen += 4;
    }
    gBuffer[gBufLen++] = 6;
    gBuffer[gBufLen++] = length;
    memcpy(gBuffer + gBufLen, text, length);
    gBufLen += length;
    text_unlock();
}
