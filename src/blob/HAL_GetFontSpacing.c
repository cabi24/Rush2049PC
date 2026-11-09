/* HAL_GetFontSpacing: the space between letters (MAT3 HAL_text.c). */
#include "game/game.h"

/* The current space between letters, in pixels (the font's default unless HAL_SetFontSpacing changed it). */
s8 HAL_GetFontSpacing(void) {
    return gSpacing;
}
