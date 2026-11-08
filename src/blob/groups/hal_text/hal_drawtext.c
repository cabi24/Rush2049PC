/* hal_drawtext.c: HAL_DrawText = HAL_DrawText and _TextLen = TextLen,
 * MAT3 HAL_text.c ports (GameSource/src/HAL_text.c). The last-state variables are MAT3's function-local
 * statics (own .bss, verified by address: 0x80153F60..0x8015419C); the semaphore helpers are MAT3's
 * _GetSemaphore/_ReleaseSemaphore, non-static and not on keep (convention, as countdown), inlined.
 * N64 differences from the GameCube text:
 * no halving of spacing/position, and every computed byte stored into gBuffer is masked `& 255`.
 * DISCLOSED shaping: the empty `else ;` on the first size test (a zero-footprint uopt block; it lowers the
 * gFontIndex address web's save below the gLeading/gSpacing webs, giving retail's t4/t5 assignment).
 * flags: -g0 -O3 -mips2 -G 0 -non_shared */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef void *OSMesg;
typedef struct OSMesgQueue { s32 w[6]; } OSMesgQueue;
extern s32 osRecvMesg(OSMesgQueue *, OSMesg *, s32);
extern s32 osSendMesg(OSMesgQueue *, OSMesg, s32);
extern void *memcpy(void *, const void *, u32);
extern void game_strcpy(void *, void *);          /* strcpy */
extern void game_strncpy(void *, void *, s32);     /* strncpy */
extern void DrawText(void *text, u16 length);  /* _DrawText */
extern void SetDefaults(u32 *, f32 *, s16 *, s16 *, s8 *, s8 *, s8 *);  /* _SetDefaults */

extern f32 gDepth;
extern s8 gFinished;
extern f32 gTrans;
extern s8 gBufferInited;
extern OSMesgQueue text_lock_queue;
extern u32 gFontColor;
extern s8 gLeading;
extern s8 gSpacing;
extern s16 gFontXpos;
extern s16 gFontYpos;
extern s8 gFontIndex;
extern s32 gBufLen;
extern u8 gBuffer[800];  /* gBuffer */

void _GetSemaphore_DrawText(void) { osRecvMesg(&text_lock_queue, 0, 1); }
void _ReleaseSemaphore_DrawText(void) { osSendMesg(&text_lock_queue, 0, 0); }

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
    _GetSemaphore_DrawText();
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
        _ReleaseSemaphore_DrawText();
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
    _ReleaseSemaphore_DrawText();
}
