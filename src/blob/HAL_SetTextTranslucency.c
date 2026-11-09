#include "game/game.h"

void HAL_SetTextTranslucency(float x) {
    gTrans = x;
    if (x < 0.0f || x >= 255.0f) {
        gFontAttribute &= ~0x20;
    } else {
        gFontAttribute |= 0x20;
        BlitXLU((u32)x);
    }
}
