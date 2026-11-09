/* HAL_GetFontLeading: the space between lines (MAT3 HAL_text.c). */
#include "game/game.h"

/* The current space between lines of text, in pixels (the font's default unless HAL_SetFontLeading changed it). */
s8 HAL_GetFontLeading(void) {
    return gLeading;
}
