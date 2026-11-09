/* HAL_SetTextTranslucency: how translucent the text drawn next is (MAT3 HAL_text.c). */
#include "game/game.h"

/* Draws the text that follows translucent with alpha x (0..254: BlitXLU, BLIT_ATTR_XLU), or opaque for a negative x
 * or one of 255 or more. */
void HAL_SetTextTranslucency(float x) {
    gTrans = x;
    if (x < 0.0f || x >= 255.0f) {
        gFontAttribute &= ~BLIT_ATTR_XLU;
    } else {
        gFontAttribute |= BLIT_ATTR_XLU;
        BlitXLU((u32)x);
    }
}
