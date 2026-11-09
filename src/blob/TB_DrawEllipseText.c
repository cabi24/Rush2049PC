/* TB_DrawEllipseText: draw a string cut to a width with "..." (MAT3 TB_text.c). */
#include "game/game.h"

/* Draws text at (x, y), shortened with an ellipsis when it is wider than width pixels (TB_EllipseText; strings up to
 * 127 bytes). */
void TB_DrawEllipseText(s16 x, s16 y, s16 width, void *text) {
    u8 buf[0x80];

    TB_EllipseText(buf, text, width);
    TB_DrawText(x, (s32) y, buf);
}
