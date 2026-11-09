/* TB_GetTextNumLines: how many lines a string word-wraps to (MAT3 TB_text.c). */
#include "game/game.h"

/* The number of lines text takes word-wrapped to width pixels in the current font, broken as TB_DrawTextScrollBox
 * breaks them (at a carriage return, a newline, or the last space that keeps the line within width); at most
 * 0x7FFF. */
s16 TB_GetTextNumLines(s16 width, u8 *text) {
    u8 *start;
    s16 lines;
    s32 step;
    u8 *p;
    s16 ch;
    s16 lineWidth;
    s16 count;
    s16 spacing;
    u8 *space;
    s8 wide;

    wide = (*text == TEXT_DOUBLE_BYTE);
    if (wide) {
        start = text + 1;
        step = 2;
    } else {
        start = text;
        step = 1;
    }
    spacing = HAL_GetFontSpacing();
    lineWidth = 0;
    count = 0;
    lines = 0;
    space = 0;
    for (;;) {
        p = start;
        for (;;) {
            if (wide) {
                ch = (p[0] << 8) | p[1];
            } else {
                ch = *p;
            }
            if (ch == ' ' && lineWidth == 0) {
                start += step;
                p = start;
                continue;
            }
            if (ch == 0) {
                break;
            }
            if (ch == 13) {
                break;
            }
            if (ch == 10) {
                break;
            }
            lineWidth += HAL_GetCharWidth(ch);
            if (ch != ' ') {
                lineWidth += spacing;
            } else if (space != p - step) {
                space = p;
            }
            p += step;
            if (width < lineWidth && space != 0) {
                p = space;
                break;
            }
            count++;
        }
        if (start) {
        }
        lines++;
        if (ch == 0 || lines == 0x7FFF) {
            break;
        }
        start = step + p;
        count = 0;
        lineWidth = 0;
        space = 0;
    }
    return lines;
}
