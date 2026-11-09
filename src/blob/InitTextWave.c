/* InitTextWave: load the game's strings (MAT3 text.c). */
#include "game/game.h"

/* Loads the game's text, once: first the largest language's wave (gLargestTextWave), locked, so that its block fits
 * any language, with gText's tables pointed into it (AdjustTextWavePointers); then LoadTextWave puts language 0's
 * strings in its place. */
void InitTextWave(void)
{
    const s8 initialized = 1;

    if (text_wave_inited == 0) {
        text_wave_inited = initialized;
        gLang = 0;
        gTextWave = gLargestTextWave;
        gTextWaveInst = LoadWave(gLargestTextWave, 0, 0, 0, 0);
        LockWaveInstance(gTextWaveInst);
        AdjustTextWavePointers();
        LoadTextWave();
    }
}
