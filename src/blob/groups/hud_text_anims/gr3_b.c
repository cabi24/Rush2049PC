typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct { u32 opaque[6]; } OSMesgQueue;   /* 24-byte libultra queue */
extern OSMesgQueue text_lock_queue;
s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32 selection);
void SetupFontData(s32);
s8 HAL_SetFontFixedWidth(s8);
s32 FindWaveInstanceInternal(s32, s32);
extern s32 gFontDatWaveInst;
extern s32 gFontTexWaveInst;
extern s8 gFontIndex;
extern void *gFontHeap;
s32 LoadWave(s32, s32, s32, s32, void *);
void LockWaveInstance(s32);
s32 HAL_SetFontInternal(s32 selection) {
    s8 old;
    old = gFontIndex;
    gFontIndex = selection;
    if (selection != -1) {
        if ((gFontDatWaveInst = FindWaveInstanceInternal(gFontIndex + 0x26, -1)) < 0) {
            LockWaveInstance(gFontDatWaveInst = LoadWave(gFontIndex + 0x26, 0, 0, 1, 0));
        }
        do {} while (0); /* : compiled-out block boundary, */
        if ((gFontTexWaveInst = FindWaveInstanceInternal(gFontIndex + 0x16, -1)) < 0) {
            LockWaveInstance(gFontTexWaveInst = LoadWave(gFontIndex + 0x16, 0, 0, 0, gFontHeap));
        }
        SetupFontData(old != selection);
    }
    if (selection == 0) {
        HAL_SetFontFixedWidth(1);
    }
    return old;
}

typedef struct { u32 first, second; } Pair;
extern Pair gJust;
