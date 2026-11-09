/* HAL_SetFontFixedWidth, font_get_char_list and HAL_GetCharWidth: font metrics (MAT3 HAL_text.c; the character list
 * is the N64's own). */
#include "game/game.h"

/* Makes the current font fixed width (value 1) or proportional (0), changing the font's header itself, and returns
 * what it was. HAL_SetFont makes font 0 fixed width whenever it is chosen. */
s8 HAL_SetFontFixedWidth(s8 value) {
 s8 old;
 SetupFontData(0);
 old = gFontHeader->nonProp;
 gFontHeader->nonProp = value;
 return old;
}

/* Writes the characters the current font has, in its order, to out, ended by a zero. Nothing calls it. */
void font_get_char_list(u8 *out) {
 s32 i;
 SetupFontData(0);
 for (i = 0; i < gFontHeader->numChars; i++) {
  out[i] = gFontLetters[i].value;
 }
 out[i] = 0;
}

/* The width in pixels of character id in the current font, without the spacing: maxWidth in a fixed-width font, a
 * space's width for a space, a character over 255 or one the font lacks, 0 for a newline. */
s32 HAL_GetCharWidth(u16 id) {
 s16 index;
 SetupFontData(0);
 if ((u8)gFontHeader->nonProp != 0) return gFontHeader->maxWidth;
 if (id == 32 || id >= 256) return gFontHeader->spaceWidth;
 if (id == 10) return 0;
 index = gCharMap[id];
 if (index < 0) return gFontHeader->spaceWidth;
 return gFontLetters[index].lrs - gFontLetters[index].uls + 1;
}
