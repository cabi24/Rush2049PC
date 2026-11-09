#include "game/game.h"

typedef struct FontHdr {
    /* 0x0 */ u8 pad0;
    /* 0x1 */ u8 mode;
    /* 0x2 */ u8 f2;
    /* 0x3 */ u8 f3;
    /* 0x4 */ u8 f4;
    /* 0x5 */ u8 pad5;
    /* 0x6 */ u8 fixedWidth;
    /* 0x7 */ u8 spaceWidth;
    /* 0x8 */ u8 f8;
    /* 0x9 */ u8 monospace;
    /* 0xA */ u8 kerning;
    /* 0xB */ u8 f11;
    /* 0xC */ u8 count;
} FontHdr;

/* PC: a font letter is 12 bytes as in the file, its kern slot a 4-byte window address (port/assets/font.c);
 * FONT_KERNS reads it as a pointer. */
typedef struct Glyph {
    /* 0x0 */ u32 kern;
    /* 0x4 */ u8 id;
    /* 0x5 */ u8 pad5;
    /* 0x6 */ u8 left;
    /* 0x7 */ u8 pad7;
    /* 0x8 */ u8 right;
    /* 0x9 */ u8 pad9[3];
} Glyph;
#define FONT_KERNS(k) ((u8 *)PC_N64P(k))

VIEW(FontHdr *, gFontHeader);
#define gFontHeader gFontHeader_view
VIEW(Glyph *, gFontLetters);
#define gFontLetters gFontLetters_view
VIEW(s16[256], gCharMap);
#define gCharMap gCharMap_view

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
    if (str[0] == 255) {
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
        if (gFontHeader->monospace) {
            width += gFontHeader->fixedWidth + gSpacing;
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
                    width += gFontLetters[ch].right - gFontLetters[ch].left + gSpacing + 1;
                    if (prev > 0 && gFontHeader->kerning) {
                        width -= FONT_KERNS(gFontLetters[ch].kern)[prev];
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
