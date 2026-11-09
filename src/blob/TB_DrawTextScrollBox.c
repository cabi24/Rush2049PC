/* TB_DrawTextScrollBox: word-wrapped text in a box, one page of it (MAT3 TB_text.c). */
#include "game/game.h"

/* Draws text word-wrapped to width pixels in a box height pixels tall at (x, y), from page page and line line of it:
 * the lines that fit in the box make a page (the pages before page hold one line fewer); a negative page draws from
 * the start with one more line's room. Each line is placed by the
 * justification (gJust: across against x; down, the whole text against y unless kTB_JustTop). A line breaks at a
 * carriage return, a newline or the last space that keeps it within width; spaces that start a line are skipped.
 * Double-byte text is drawn a line at a time by putting a 0xFF before the line in place, then the byte back. */
void TB_DrawTextScrollBox(s16 x, s16 y, s16 width, s16 height, s16 page, s16 line, void *text)
{
    s32 tw;
    s16 top, bottom, fontHeight, spacing, charSize, numLines, currentLine, currentPage, len, overlap, currWidth, ch;
    const u8 *s, *lastSpace;
    u8 *str;
    u8 saveChar;
    s8 dbl;

    fontHeight = TB_GetFontLineHeight();
    spacing = HAL_GetFontSpacing();
    top = y;
    if (gJust.v != kTB_JustTop) {
        numLines = TB_GetTextNumLines(width, text);
        if (gJust.v == kTB_JustCenter) top = y - (numLines * fontHeight) / 2;
        else top = y - numLines * fontHeight;
    }
    str = text;
    bottom = top + height;
    dbl = *str == TEXT_DOUBLE_BYTE;
    if (dbl) {
        str++;
        charSize = 2;
    } else charSize = 1;
    currentLine = 0;
    if (page < 0) {
        currentPage = page;
        bottom += fontHeight;
    } else currentPage = 0;
    overlap = currentPage < page ? fontHeight : (s16)0;
    do {
        s = str;
        len = 0;
        currWidth = 0;
        lastSpace = 0;
        for (;;) {
            if (dbl) ch = (s[0] << 8) | s[1];
            else ch = *s;
            if (ch == ' ' && currWidth == 0) {
                str += charSize;
                s = str;
                continue;
            }
            if (!ch || ch == 13 || ch == 10) break;
            currWidth += HAL_GetCharWidth(ch);
            if (ch != ' ') currWidth += spacing;
            else if (lastSpace != s - charSize) lastSpace = s;
            if (currWidth > width && lastSpace) {
                if (dbl) len -= (s - lastSpace) >> 1;
                else len -= s - lastSpace;
                s = lastSpace;
                break;
            }
            s += charSize;
            len++;
        }
        if (currentPage >= page) {
            if (currentLine >= line) {
                if (len > 0) {
                    if (dbl) {
                        saveChar = str[-1];
                        str[-1] = TEXT_DOUBLE_BYTE;
                    }
                    tw = HAL_GetTextWidth(str - (dbl ? 1 : 0), len);
                    HAL_SetTextPos((gJust.h == kTB_JustCenter) ? x - (tw >> 1) : (gJust.h == kTB_JustRight) ? x - tw : x, top);
                    HAL_DrawText(str - (dbl ? 1 : 0), len);
                    if (dbl) str[-1] = saveChar;
                }
                top += fontHeight;
            }
            currentLine++;
        } else top += fontHeight;
        if (!ch) break;
        str = (u8 *)s + charSize;
        if (top > bottom - fontHeight - overlap) {
            currentPage++;
            top = y;
            if (currentPage > page) break;
            if (currentPage == page) overlap = 0;
        }
    } while (1);
}
