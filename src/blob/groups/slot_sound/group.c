#include "game/game.h"

typedef struct ResSlot {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ s8 state;
    /* 0x02 */ u8 active;
    /* 0x03 */ u8 pad3[9];
    /* 0x0C */ void *data;
    /* 0x10 */ s32 unk10;
} ResSlot; /* 0x14 */

VIEW(ResSlot[], gWaveInstance);
#define gWaveInstance gWaveInstance_view

#define N64A(x) PC_N64A(x)

void ValidateWaveInstance(s32 arg0, s32 arg1, s32 arg2)
{
  s32 t;
  if (arg0) {}
  if (arg1) {}
  t = !arg2;
  if (arg2 != 0)
  {
    if (t) {}
    if (t) {}
  }
  if (t) {}
  if (0) { switch (arg0 + arg1) { case 1: font_image_format = 1; break; case 2: font_image_format = 2; break; case 3: font_image_format = 3; break; } }
}

void *GetWaveInstanceAddr(s32 index)
{
    ValidateWaveInstance(index, 0, 0);
    return gWaveInstance[index].data;
}

void LockWaveInstance(s32 index)
{
    ResSlot *slot;

    ValidateWaveInstance(index, 0, 0);
    slot = &gWaveInstance[index];
    PurgeableMemory(N64A(slot->data), 0);
    do { slot->active = 1; } while (0);
}

s8 WaveInstanceReady(s32 index)
{
    ValidateWaveInstance(index, 0, 0);
    return gWaveInstance[index].state;
}

void UnlockWaveInstance(s32 index)
{
    ResSlot *slot;

    ValidateWaveInstance(index, 0, 0);
    slot = &gWaveInstance[index];
    PurgeableMemory(N64A(slot->data), 1);
    do { slot->active = 0; } while (0);
}

typedef struct Bank {
    /* 0x0 */ u8 pad0;
    /* 0x1 */ u8 mode;
    /* 0x2 */ u8 f2;
    /* 0x3 */ u8 f3;
    /* 0x4 */ u8 f4;
    /* 0x5 */ u8 pad5;
    /* 0x6 */ u8 f6;
    /* 0x7 */ u8 f7;
    /* 0x8 */ u8 f8;
    /* 0x9 */ s8 f9;
    /* 0xA */ u8 f10;
    /* 0xB */ u8 f11;
    /* 0xC */ u8 f12;
} Bank;

typedef struct Entry { u32 ptr; u8 id; u8 pad[7]; } Entry;
#define FONT_SLOT(p) PC_N64A(p)
#define WAVE_HANDLE_BLOCK(h) ((u8 *)PC_N64P(PC_SLOT_N64(h)))

VIEW(Entry *, gFontLetters);
#define gFontLetters gFontLetters_view
VIEW(s16[256], gCharMap);
#define gCharMap gCharMap_view
VIEW(intptr_t[], gFontTextures);
#define gFontTextures gFontTextures_view
/* PC: the gTexDefPtrs pairs are {tTexture *, s32}, 16 bytes, as SetupWaveModel declares them. first is the window
 * address of the slot's TXHD records, widened without sign extension. */
typedef struct { uintptr_t first; s32 second; } TexDefPair;
#define TEXDEF_PAIR TexDefPair
VIEW(TEXDEF_PAIR[], gTexDefPtrs);
#define gTexDefPtrs gTexDefPtrs_view
VIEW(u8, gLeading);
#define gLeading gLeading_view
VIEW(u8, gSpacing);
#define gSpacing gSpacing_view

void SetupFontData(s32 force)
{
    u8 *p;
    s32 i;

    p = WAVE_HANDLE_BLOCK(GetWaveInstanceAddr(gFontDatWaveInst));
    if (force || p != (u8 *)gFontHeader) {
        gFontHeader = (Bank *)p;
        p += 16;
        gFontLetters = (Entry *)p;
        p += gFontHeader->f12 * 12;
        if (gFontHeader->f10 != 0) {
            for (i = 0; i < gFontHeader->f12; i++) {
                gFontLetters[i].ptr = FONT_SLOT(p);
                p += gFontHeader->f12;
            }
        }
        for (i = 0; i < 256; i++) {
            gCharMap[i] = -1;
        }
        for (i = 0; i < gFontHeader->f12; i++) {
            gCharMap[gFontLetters[i].id] = i;
        }
    }
    if (force) {
        for (i = 0; i < gFontHeader->f11; i++) {
            gFontTextures[i] = gTexDefPtrs[gFontTexWaveInst].first + i * 36;
        }
        gLeading = gFontHeader->f4;
        gSpacing = gFontHeader->f8;
    }
    font_image_format = (gFontHeader->mode == 1) ? 4 : 3;
    font_texel_size = (gFontHeader->mode == 1) ? 0 : 1;
}

void HAL_SetFontLeading(s16 a) {
 if (a < 0) { SetupFontData(0); gLeading = gFontHeader->f4; }
 else gLeading = a;
}
u8 HAL_GetFontAscent(void) { SetupFontData(0); return *((u8 *)gFontHeader + 2); }
u8 HAL_GetFontDescent(void) { SetupFontData(0); return *((u8 *)gFontHeader + 3); }

void HAL_SetFontSpacing(s16 a) { if (a < 0) { SetupFontData(0); gSpacing=gFontHeader->f8; } else gSpacing=a; }
