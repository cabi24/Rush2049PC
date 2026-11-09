/* HAL_DrawText: draw a string, or buffer it for the frame's end (MAT3 HAL_text.c); and TextLen. */
#include "game/game.h"

extern void *memcpy(void *, const void *, u32);

VIEW(u32, gFontColor);
#define gFontColor gFontColor_view


/* The number of bytes of text that numChars characters take (all of it when numChars < 0), its double-byte mark and
 * its ending zero (or zeros) included. (MAT3's _TextLen.) */
u32 TextLen(const u8 *text, s16 numChars)
{
    u32 len;
    s32 nc;
    nc = 0;
    if (text[0] == TEXT_DOUBLE_BYTE) {
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

/* Draws numChars characters of text (all when negative) at the text position in the current font and colour. Text
 * with a depth (gDepth >= 0) is drawn at once (DrawText); otherwise it is appended to gBuffer as commands
 * (TEXT_CMD_*), only the state that changed since the last string buffered before it, and HAL_FinishText draws it
 * all at the frame's end; when it doesn't fit in the TEXT_BUFFER_SIZE bytes it is dropped. With gFinished set the
 * string is first copied to a local buffer that nothing reads (MAT3 alike). Under the text lock. */
void HAL_DrawText(void *text, s16 numChars)
{
    s32 size;
    s32 length;
    u8 str[256];
    /* The state the buffer's commands have reached: a command goes in only for what differs. */
    static u32 lastFontColor;
    static f32 lastTrans;
    static s16 lastFontXpos;
    static s16 lastFontYpos;
    static s8 lastFontIndex;
    static s8 lastLeading;
    static s8 lastSpacing;

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
    if (gBufLen + size >= TEXT_BUFFER_SIZE) {
        text_unlock();
        return;
    }
    if (lastFontIndex != gFontIndex) {
        lastFontIndex = gFontIndex;
        gBuffer[gBufLen++] = TEXT_CMD_FONT;
        gBuffer[gBufLen++] = gFontIndex;
    }
    if (lastLeading != gLeading) {
        lastLeading = gLeading;
        gBuffer[gBufLen++] = TEXT_CMD_LEADING;
        gBuffer[gBufLen++] = gLeading;
    }
    if (lastSpacing != gSpacing) {
        lastSpacing = gSpacing;
        gBuffer[gBufLen++] = TEXT_CMD_SPACING;
        gBuffer[gBufLen++] = gSpacing;
    }
    if (lastFontXpos != gFontXpos || lastFontYpos != gFontYpos) {
        lastFontXpos = gFontXpos;
        lastFontYpos = gFontYpos;
        gBuffer[gBufLen++] = TEXT_CMD_POS;
        gBuffer[gBufLen++] = (lastFontXpos >> 8) & 255;
        gBuffer[gBufLen++] = lastFontXpos & 255;
        gBuffer[gBufLen++] = (lastFontYpos >> 8) & 255;
        gBuffer[gBufLen++] = lastFontYpos & 255;
    }
    if (lastTrans != gTrans) {
        lastTrans = gTrans;
        gBuffer[gBufLen++] = TEXT_CMD_TRANSLUCENCY;
        gBuffer[gBufLen++] = (*(u32 *)&gTrans >> 24) & 255;
        gBuffer[gBufLen++] = (*(u32 *)&gTrans >> 16) & 255;
        gBuffer[gBufLen++] = (*(u32 *)&gTrans >> 8) & 255;
        gBuffer[gBufLen++] = (*(u32 *)&gTrans) & 255;
    }
    if (lastFontColor != gFontColor) {
        lastFontColor = gFontColor;
        gBuffer[gBufLen++] = TEXT_CMD_COLOR;
        memcpy(gBuffer + gBufLen, &gFontColor, 4);
        gBufLen += 4;
    }
    gBuffer[gBufLen++] = TEXT_CMD_TEXT;
    gBuffer[gBufLen++] = length;
    memcpy(gBuffer + gBufLen, text, length);
    gBufLen += length;
    text_unlock();
}
