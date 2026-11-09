/* HAL_SetTextPos: where the text drawn next starts (MAT3 HAL_text.c). */
#include "game/game.h"

/* Moves the text position to (x, y), the top left of the next string, in screen pixels; TEXT_POS_KEEP for either
 * leaves it. (MAT3 doubles both, for its 640x448 screen.) */
void HAL_SetTextPos(s16 x, s16 y) {
    if (x != TEXT_POS_KEEP) {
        gFontXpos = x;
    }
    if (y != TEXT_POS_KEEP) {
        gFontYpos = y;
    }
}
