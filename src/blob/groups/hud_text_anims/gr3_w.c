typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef float f32;
typedef struct { s32 opaque[6]; } OSMesgQueue;
extern OSMesgQueue text_lock_queue;
extern s32 osRecvMesg(OSMesgQueue *, void **, s32);
extern s32 osSendMesg(OSMesgQueue *, void *, s32);
extern s32 HAL_SetFontInternal(s32 selection);

extern s32 gFontColor;
extern f32 gTrans;
extern s16 gFontXpos, gFontYpos;
extern s8 gFontIndex, gLeading, gSpacing;
extern s32 gFontAttribute;
extern s32 screen_height;
extern s8 gBufferInited, gFinished;

void SetDefaults(u8 *rgba,f32 *amount,s16 *x,s16 *y,s8 *bank,s8 *mode,s8 *variant);

void HAL_StartText(void)
{
    osRecvMesg(&text_lock_queue, (void *)0, 1);
    SetDefaults((u8 *)&gFontColor, &gTrans, &gFontXpos, &gFontYpos, &gFontIndex, &gLeading, &gSpacing);
    HAL_SetFontInternal(gFontIndex);
    gFontAttribute = 0;
    if (screen_height > 220) {
        gFontAttribute |= 0x8000;
    }
    gBufferInited = 0;
    gFinished = 0;
    osSendMesg(&text_lock_queue, (void *)0, 0);
}

/* stand-in caller (context, kept): keeps SetDefaults out of line as in retail (three callers there) */
