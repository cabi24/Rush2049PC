#include "game/game.h"

typedef struct { s32 h, v; } Just;
VIEW(Just, gJust);
#define gJust gJust_view

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
    if (gJust.v != 3) {
        numLines = TB_GetTextNumLines(width, text);
        if (gJust.v == 1) top = y - (numLines * fontHeight) / 2;
        else top = y - numLines * fontHeight;
    }
    str = text;
    bottom = top + height;
    dbl = *str == 255;
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
                        str[-1] = 255;
                    }
                    tw = HAL_GetTextWidth(str - (dbl ? 1 : 0), len);
                    HAL_SetTextPos((gJust.h == 1) ? x - (tw >> 1) : (gJust.h == 2) ? x - tw : x, top);
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
