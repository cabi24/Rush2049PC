/* HAL_SetFontInternal: choose a font, loading it if need be (MAT3 HAL_text.c's _HAL_SetFont). */
#include "game/game.h"

/* Makes font fontID (0..15; -1 none) the current one: its data wave and its texture wave are found or loaded and
 * locked (the textures into gFontHeap), and SetupFontData reads its header, again in full when the font changed.
 * Font 0 is made fixed width. Returns the font that was current. The caller holds the text lock. */
s32 HAL_SetFontInternal(s32 fontID) {
    s8 old;
    old = gFontIndex;
    gFontIndex = fontID;
    if (fontID != -1) {
        if ((gFontDatWaveInst = FindWaveInstanceInternal(FONT_DATA_WAVE(gFontIndex), -1)) < 0) {
            LockWaveInstance(gFontDatWaveInst = LoadWave(FONT_DATA_WAVE(gFontIndex), 0, 0, 1, 0));
        }
        if ((gFontTexWaveInst = FindWaveInstanceInternal(FONT_TEXTURE_WAVE(gFontIndex), -1)) < 0) {
            LockWaveInstance(gFontTexWaveInst = LoadWave(FONT_TEXTURE_WAVE(gFontIndex), 0, 0, 0, gFontHeap));
        }
        SetupFontData(old != fontID);
    }
    if (fontID == 0) {
        HAL_SetFontFixedWidth(1);
    }
    return old;
}

