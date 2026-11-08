/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * HAL_FinishText (0x800ED804, 712 B) = MAT3 HAL_text.c HAL_FinishText(signed char end): replay the
 * buffered text commands (gBuffer gBuffer, gBufLen gBufLen) under the font semaphore text_lock_queue: reset the
 * defaults (_SetDefaults = SetDefaults with the seven globals), _HAL_SetFont(gFontIndex) = HAL_SetFontInternal,
 * gFontAttribute &= 0x8000, then per opcode 0..6: font, HAL_SetFontLeading, HAL_SetFontSpacing
 * (HAL_SetFontSpacing), HAL_SetTextPos (HAL_SetTextPos, umerge-inlined), HAL_SetTextTranslucency from a
 * big-endian float, HAL_SetTextColor, _DrawText (DrawText, private IPA: text in s2,
 * length in s0).
 * : a direct typed port of the MAT3 text with the real callees; the earlier drafts (defaults
 * context) predate _DrawText's match and hand-inlined HAL_SetTextPos. No shaping.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct { s32 opaque[6]; } OSMesgQueue;
extern OSMesgQueue text_lock_queue;
extern s32 osRecvMesg(OSMesgQueue *, void **, s32);
extern s32 osSendMesg(OSMesgQueue *, void *, s32);
extern s32 HAL_SetFontInternal(s32 selection);                 /* _HAL_SetFont */
extern void SetDefaults(u8 *rgba, f32 *amount, s16 *x, s16 *y, s8 *bank, s8 *mode, s8 *variant); /* _SetDefaults */
extern void HAL_SetFontLeading(s16 leading);
extern void HAL_SetFontSpacing(s16 spacing);
extern void HAL_SetTextPos(s16 x, s16 y);
extern void HAL_SetTextTranslucency(f32 xlu);
extern void HAL_SetTextColor(u8 r, u8 g, u8 b, u8 a);
extern void DrawText(const u8 *text, u16 length); /* _DrawText */
extern s32 gFontColor;
extern f32 gTrans;
extern s16 gFontXpos, gFontYpos;   /* gFontXpos, gFontYpos */
extern s8 gFontIndex, gLeading, gSpacing;   /* gFontIndex, gLeading, gSpacing */
extern s32 gFontAttribute;
extern s8 gBufferInited, gFinished;   /* gBufferInited, gFinished */
extern s32 gBufLen;
extern u8 gBuffer[800];      /* gBuffer */

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
