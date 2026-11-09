#include "game/game.h"

void HAL_SetTextDepth(f32 value) {
    gDepth = value;
    if (value > 0.0f) {
        gFontAttribute |= 0x10;
        BlitZDist((u16)(u32)value);
    } else {
        gFontAttribute &= ~0x10;
    }
}
