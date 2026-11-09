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
    PurgeableMemory(slot->ptr, 0);
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
    PurgeableMemory(slot->ptr, 1);
    do { slot->locked = 0; } while (0);
}

/* A handle wave's block: the address its master slot holds (the heap may move the block and update the slot). */
#define WAVE_HANDLE_BLOCK(h) (*(u8 **)(h))


/* Points the font globals at the current font's file (gFontDatWaveInst, a handle wave) when it has moved or force is
 * set: the header (the file's first 16 bytes, FontHeader), the letters with their kerning rows, the character map
 * (gCharMap: a letter's index by character, -1 for none). With force it also takes the font's textures
 * (gFontTexWaveInst's) and its default leading and spacing. Then sets the texture format the text draw uses: 4-bit
 * intensity for a font of type 1, else 8-bit intensity-alpha (G_IM_FMT_I or G_IM_FMT_IA, G_IM_SIZ_4b or
 * G_IM_SIZ_8b).
 * PC: the letters are the font's converter's, built beside the file with a pointer to each letter's kerning row
 * (pc_font_letters, port/assets/font.c). On the N64 they were the file's, after its header, and this wrote each row's
 * 32-bit address into its letter, again whenever the heap had moved the file; the converter's follow the file. */
void SetupFontData(s32 force)
{
    u8 *p;
    s32 i;

    p = WAVE_HANDLE_BLOCK(GetWaveInstanceAddr(gFontDatWaveInst));
    if (force || p != (u8 *)gFontHeader) {
        gFontHeader = (FontHeader *)p;
        gFontLetters = pc_font_letters(p);
        for (i = 0; i < 256; i++) {
            gCharMap[i] = -1;
        }
        for (i = 0; i < gFontHeader->numChars; i++) {
            gCharMap[gFontLetters[i].value] = i;
        }
    }
    if (force) {
        for (i = 0; i < gFontHeader->numTextures; i++) {
            gFontTextures[i] = &gTexDefPtrs[gFontTexWaveInst].texDefs[i];
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
