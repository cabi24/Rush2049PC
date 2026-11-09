#include "game/game.h"

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
