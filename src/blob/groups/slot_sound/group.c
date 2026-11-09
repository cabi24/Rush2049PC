/* Wave instance accessors (MAT3 waves.c: GetWaveInstanceAddr, LockWaveInstance, WaveInstanceReady,
 * UnlockWaveInstance) and the current font's data (MAT3 HAL_text.c _SetupFontData and the font metric setters and
 * getters), which reads the font's wave through them. waves/group.c has the waves. */
#include "game/game.h"

/* MAT3's _ValidateWaveInstance(idx, loaded, ready), a debug check of a wave instance's state: empty in the retail
 * game (the decompiler's body for it computed nothing it kept). */
void ValidateWaveInstance(s32 index, s32 loaded, s32 ready)
{
}

/* Wave instance index's block, or its handle for a handle wave. */
void *GetWaveInstanceAddr(s32 index)
{
    ValidateWaveInstance(index, 0, 0);
    return gWaveInstance[index].ptr;
}

/* Keeps wave instance index over InitWaves (locked), and locks its block against compaction. */
void LockWaveInstance(s32 index)
{
    anon_loaded_ready_14 *slot;

    ValidateWaveInstance(index, 0, 0);
    slot = &gWaveInstance[index];
    PurgeableMemory(PC_N64A(slot->ptr), 0);
    do { slot->locked = 1; } while (0);
}

/* Whether wave instance index has been read and set up. */
s8 WaveInstanceReady(s32 index)
{
    ValidateWaveInstance(index, 0, 0);
    return gWaveInstance[index].ready;
}

/* Undoes LockWaveInstance. */
void UnlockWaveInstance(s32 index)
{
    anon_loaded_ready_14 *slot;

    ValidateWaveInstance(index, 0, 0);
    slot = &gWaveInstance[index];
    PurgeableMemory(PC_N64A(slot->ptr), 1);
    do { slot->locked = 0; } while (0);
}

/* A font file's header (MAT3 anon_version_type_10), 16 bytes, then its letters, then each letter's kerning row when
 * the font has kerning. The file keeps its N64 layout in the window. */
typedef struct Bank {
    u8 version;     /* 0x0 */
    u8 type;        /* 0x1: 1 for a 4-bit font */
    u8 ascent;      /* 0x2 */
    u8 descent;     /* 0x3 */
    u8 leading;     /* 0x4 */
    u8 totalHeight; /* 0x5 */
    u8 maxWidth;    /* 0x6 */
    u8 spaceWidth;  /* 0x7 */
    u8 spacing;     /* 0x8 */
    s8 nonProp;     /* 0x9 */
    u8 kern;        /* 0xA: the letters have kerning rows */
    u8 numTextures; /* 0xB */
    u8 numChars;    /* 0xC */
} Bank;

/* A letter (MAT3 anon_kerns_value_c, N64 layout, 12 bytes): its kerning row's window address, and its character. */
typedef struct Entry { u32 kerns; u8 value; u8 pad[7]; } Entry;
#define FONT_SLOT(p) PC_N64A(p)
#define WAVE_HANDLE_BLOCK(h) ((u8 *)PC_N64P(PC_SLOT_N64(h)))
/* A texture record of the font's texture wave (tTexture, N64 layout). */
#define TEXTURE_RECORD_SIZE 36

VIEW(Entry *, gFontLetters);
#define gFontLetters gFontLetters_view
VIEW(intptr_t[], gFontTextures);
#define gFontTextures gFontTextures_view
/* PC: the gTexDefPtrs pairs are {tTexture *, s32}, 16 bytes, as SetupWaveModel declares them. first is the window
 * address of the instance's TXHD records, widened without sign extension. */
typedef struct { uintptr_t first; s32 second; } TexDefPair;
VIEW(TexDefPair[], gTexDefPtrs);
#define gTexDefPtrs gTexDefPtrs_view
VIEW(u8, gLeading);
#define gLeading gLeading_view
VIEW(u8, gSpacing);
#define gSpacing gSpacing_view

/* Points the font globals at the current font's file (gFontDatWaveInst, a handle wave) when it has moved or force is
 * set: the header, the letters, the kerning rows, the character map (gCharMap: a letter's index by character, -1
 * for none). With force it also takes the font's textures (gFontTexWaveInst) and its default leading and spacing.
 * Then sets the texture format the text draw uses: 4-bit intensity for a font of type 1, else 8-bit
 * intensity-alpha (G_IM_FMT_I or G_IM_FMT_IA, G_IM_SIZ_4b or G_IM_SIZ_8b). */
void SetupFontData(s32 force)
{
    u8 *p;
    s32 i;

    p = WAVE_HANDLE_BLOCK(GetWaveInstanceAddr(gFontDatWaveInst));
    if (force || p != (u8 *)gFontHeader) {
        gFontHeader = (Bank *)p;
        p += 16;
        gFontLetters = (Entry *)p;
        p += gFontHeader->numChars * 12;
        if (gFontHeader->kern != 0) {
            for (i = 0; i < gFontHeader->numChars; i++) {
                gFontLetters[i].kerns = FONT_SLOT(p);
                p += gFontHeader->numChars;
            }
        }
        for (i = 0; i < 256; i++) {
            gCharMap[i] = -1;
        }
        for (i = 0; i < gFontHeader->numChars; i++) {
            gCharMap[gFontLetters[i].value] = i;
        }
    }
    if (force) {
        for (i = 0; i < gFontHeader->numTextures; i++) {
            gFontTextures[i] = gTexDefPtrs[gFontTexWaveInst].first + i * TEXTURE_RECORD_SIZE;
        }
        gLeading = gFontHeader->leading;
        gSpacing = gFontHeader->spacing;
    }
    font_image_format = (gFontHeader->type == 1) ? 4 : 3;
    font_texel_size = (gFontHeader->type == 1) ? 0 : 1;
}

/* Sets the gap between lines of text, or the font's own for a negative value. */
void HAL_SetFontLeading(s16 leading) {
 if (leading < 0) { SetupFontData(0); gLeading = gFontHeader->leading; }
 else gLeading = leading;
}
/* The current font's ascent and descent, in pixels. */
u8 HAL_GetFontAscent(void) { SetupFontData(0); return gFontHeader->ascent; }
u8 HAL_GetFontDescent(void) { SetupFontData(0); return gFontHeader->descent; }

/* Sets the gap between letters, or the font's own for a negative value. */
void HAL_SetFontSpacing(s16 spacing) { if (spacing < 0) { SetupFontData(0); gSpacing=gFontHeader->spacing; } else gSpacing=spacing; }
