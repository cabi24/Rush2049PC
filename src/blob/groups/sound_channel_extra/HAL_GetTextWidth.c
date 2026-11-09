/* HAL_GetTextWidth: how wide a string is in the current font (MAT3 HAL_text.c). */
#include "game/game.h"

/* A letter's row of the kerning table: its kerns field holds a window address (port/assets/font.c). */
#define FONT_KERNS(k) ((u8 *)PC_N64P(k))

VIEW(FontHeader *, gFontHeader);
#define gFontHeader gFontHeader_view
VIEW(s16[256], gCharMap);
#define gCharMap gCharMap_view

/* The width in pixels of the first maxlen characters of str (all of it when negative; it stops at a newline), in the
 * current font: each letter's width and the spacing between letters, less the kerning, a space's width for spaces,
 * characters over 255 and characters the font lacks, maxWidth plus spacing for each character of a fixed-width font;
 * without the spacing after the last. (The counter of characters reuses the parameter str.) */
s32 HAL_GetTextWidth(u8 *str, s16 maxlen)
{
    s32 width;
    s32 pos;
    s32 wide;
    s32 ch;
    s32 step;
    s32 prev;
    u8 *s;

    SetupFontData(0);
    pos = 0;
    if (str[0] == TEXT_DOUBLE_BYTE) {
        s = str + 1;
        wide = 1;
        step = 2;
    } else {
        s = str;
        wide = 0;
        step = 1;
    }
    width = 0;
    str = 0;
    prev = -1;
    while ((maxlen < 0 || (intptr_t) str < maxlen) && (s[pos] != 0 || (wide && s[pos + 1] != 0))) {
        if (gFontHeader->nonProp) {
            width += gFontHeader->maxWidth + gSpacing;
            prev = ch;
        } else {
            if (wide) {
                ch = (s[pos] << 8) | s[pos + 1];
            } else {
                ch = s[pos];
            }
            if (ch == 32 || ch >= 256) {
                width += gFontHeader->spaceWidth;
                ch = -1;
                prev = -1;
            } else {
                if (ch == 10) {
                    break;
                }
                ch = gCharMap[ch];
                if (ch < 0) {
                    width += gFontHeader->spaceWidth;
                    prev = ch;
                } else {
                    width += gFontLetters[ch].lrs - gFontLetters[ch].uls + gSpacing + 1;
                    if (prev > 0 && gFontHeader->kern) {
                        width -= FONT_KERNS(gFontLetters[ch].kerns)[prev];
                    }
                    prev = ch;
                }
            }
        }
        pos += step;
        str++;
    }
    return width - gSpacing;
}
