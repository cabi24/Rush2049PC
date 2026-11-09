#include "game/game.h"

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

