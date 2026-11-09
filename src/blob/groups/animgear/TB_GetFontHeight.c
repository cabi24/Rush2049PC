/* TB_GetFontHeight and TB_GetFontLineHeight: the current font's height (MAT3 TB_text.c). */
#include "game/game.h"

/* The current font's height in pixels: ascent and descent. */
s16 TB_GetFontHeight(void) { u8 ascent; ascent=HAL_GetFontAscent(); return (s16)(ascent + HAL_GetFontDescent()); }

/* The distance from one line of text to the next: ascent, descent and leading (HAL_GetFontAscent and
 * HAL_GetFontDescent, inline). */
s16 TB_GetFontLineHeight(void) {
 u8 descent;
 SetupFontData(0);
 descent = gFontHeader->descent;
 SetupFontData(0);
 return (s16)(gLeading + gFontHeader->ascent + descent);
}
