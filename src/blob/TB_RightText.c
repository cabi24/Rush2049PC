/* TB_RightText: where to start a string to end it at x (MAT3 TB_text.c). */
#include "game/game.h"

/* The x at which text, drawn in the current font, ends at rightX. */
s16 TB_RightText(void *text, s16 rightX) {
    return (s16) (rightX - HAL_GetTextWidth(text, -1));
}
