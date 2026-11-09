/* TB_DrawTextC: draw a string in one of the table's colours (MAT3 TB_text.c). */
#include "game/game.h"

/* Draws text at (x, y) in colour color (kTB_*Text, TB_SetTextColor). */
void TB_DrawTextC(s16 x, s16 y, s32 color, void *text) {
    TB_SetTextColor(color);
    TB_DrawText(x, (s32) y, text);
}
