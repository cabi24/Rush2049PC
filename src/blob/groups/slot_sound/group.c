/*
 * Resource-slot views and the sound-bank refresh. Real bodies only, no stand-ins.
 *
 * D_80156D38[]: 0x14-byte resource slots { u8 ?, s8 state, u8 active, ..., void *data @0xC, ... }.
 * ValidateWaveInstance(index, 0, 0): empty debug/validation hook (retail: `beqz a2,+8; nop; jr ra; nop`), called
 *   by jal from 11 functions. Body = the locked src/blob/func_80096288.c text plus a dead `if (0) switch`
 *   so that umerge does not inline it (plan M4 "inline blocker"); it must be INTERNAL (not in keep): its
 *   callers then keep the index in a3 across the call. Both additions are compile-affecting quirks.
 * GetWaveInstanceAddr(index): returns slot->data. Two statements, so umerge inlines it into
 *   SetupFontData (which is why that function has its own jal to ValidateWaveInstance and copies the
 *   index to a3).
 * display_list_alloc(index) / slot_deactivate(index): audio_loop_control(slot->data, 0 / 1) (lock-count
 *   down / up on the heap block), then slot->active = 1 / 0. The store is written `do { } while (0)`
 *   (macro-shaped): that keeps it out of the `jr ra` delay slot. Quirk, may not be the original spelling.
 * WaveInstanceReady(index): returns slot->state.
 * SetupFontData(force): internal, `force` arrives in t0. Refreshes the cached bank
 *   gFontHeader = *(Bank **)GetWaveInstanceAddr(gFontDatWaveInst) when forced or changed: entry table gFontLetters =
 *   bank + 16, per-entry data pointers after the table, id -> entry index map gCharMap[256]; when forced
 *   also gFontTextures[i] = gTexDefPtrs[gFontTexWaveInst].first + i * 36 and the two mode bytes. One `u8 *p` carries
 *   the bank pointer, the table and the running data pointer (retail reuses a1 the same way); the arrays
 *   are indexed, not walked by pointer (pointer walks force reloads of gFontHeader).
 * HAL_SetFontLeading / HAL_GetFontAscent / HAL_GetFontDescent / HAL_SetFontSpacing: already locked
 * (src/blob/groups/sound_channel), unchanged, real callers of SetupFontData.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

typedef struct ResSlot {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ s8 state;
    /* 0x02 */ u8 active;
    /* 0x03 */ u8 pad3[9];
    /* 0x0C */ void *data;
    /* 0x10 */ s32 unk10;
} ResSlot; /* 0x14 */

extern ResSlot gWaveInstance[];
/* PurgeableMemory's definition takes the block's 32-bit N64 address: N64A(p) is a window pointer's (pc_prelude.h,
 * docs/port/NOFIXED.md 2.3). */
void PurgeableMemory(u32 ptr, s32 dec);
#define N64A(x) PC_N64A(x)

extern s32 font_image_format;
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

/* PC: a font letter is 12 bytes as in the file, its kern slot a 4-byte window address (port/assets/font.c).
 * A wave handle points to a heap slot that holds the block's address (PC_SLOT_N64; pc_prelude.h,
 * docs/port/NOFIXED.md 2.3). */
typedef struct { u32 ptr; u8 id; u8 pad[7]; } Entry;
#define FONT_SLOT(p) PC_N64A(p)
#define WAVE_HANDLE_BLOCK(h) ((u8 *)PC_N64P(PC_SLOT_N64(h)))
typedef struct { s32 first; s32 second; } Pair;

extern Bank *gFontHeader;
extern s32 gFontDatWaveInst;
extern s32 gFontTexWaveInst;
extern Entry *gFontLetters;
extern s16 gCharMap[256];
extern intptr_t gFontTextures[];
/* PC: the gTexDefPtrs pairs are {tTexture *, s32}, 16 bytes, as SetupWaveModel declares them. first is the window
 * address of the slot's TXHD records, widened without sign extension. */
typedef struct { uintptr_t first; s32 second; } TexDefPair;
#define TEXDEF_PAIR TexDefPair
extern TEXDEF_PAIR gTexDefPtrs[];
extern u8 gLeading;
extern u8 gSpacing;
extern s32 font_texel_size;

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
