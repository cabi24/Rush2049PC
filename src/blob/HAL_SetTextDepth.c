/* HAL_SetTextDepth: the depth of the text drawn next (MAT3 HAL_text.c). */
#include "game/game.h"

/* Draws the text that follows at depth value (BlitZDist, BLIT_ATTR_ZDIST), or without a depth when value <= 0. Text
 * with a depth is drawn at once rather than buffered (HAL_DrawText). */
void HAL_SetTextDepth(f32 value) {
    gDepth = value;
    if (value > 0.0f) {
        gFontAttribute |= BLIT_ATTR_ZDIST;
        BlitZDist((u16)(u32)value);
    } else {
        gFontAttribute &= ~BLIT_ATTR_ZDIST;
    }
}
