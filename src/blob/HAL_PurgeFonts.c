/* HAL_PurgeFonts: let the fonts' waves go (MAT3 HAL_text.c). */
#include "game/game.h"

/* Unlocks the data and texture waves of every font that is loaded, so the wave loader may reuse their memory. */
void HAL_PurgeFonts(void) {
    s32 wave;
    s32 wave2;
    s32 font;

    font = 0;
    do {
        wave = FindWaveInstanceInternal(FONT_DATA_WAVE(font), -1);
        if (wave >= 0) {
            UnlockWaveInstance(wave);
        }
        wave2 = FindWaveInstanceInternal(FONT_TEXTURE_WAVE(font), -1);
        if (wave2 >= 0) {
            UnlockWaveInstance(wave2);
        }
        font += 1;
    } while (font != NUM_FONTS);
}
