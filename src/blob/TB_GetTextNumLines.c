#include "game/game.h"

s16 TB_GetTextNumLines(s16 width, u8 *text) {
    u8 *start;
    s16 lines;
    s32 step;
    u8 *p;
    s16 ch;
    s16 w;
    s16 n;
    s16 gap;
    u8 *space;
    s8 wide;

    wide = (*text == 0xFF);
    if (wide) {
        start = text + 1;
        step = 2;
    } else {
        start = text;
        step = 1;
    }
    gap = HAL_GetFontSpacing();
    w = 0;
    n = 0;
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
            if (ch == ' ' && w == 0) {
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
            w += HAL_GetCharWidth(ch);
            if (ch != ' ') {
                w += gap;
            } else if (space != p - step) {
                space = p;
            }
            p += step;
            if (width < w && space != 0) {
                p = space;
                break;
            }
            n++;
        }
        if (start) {
        }
        lines++;
        if (ch == 0 || lines == 0x7FFF) {
            break;
        }
        start = step + p;
        n = 0;
        w = 0;
        space = 0;
    }
    return lines;
}
