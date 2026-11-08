/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * HAL_GetTextWidth (historical label): width in pixels of a text string in the current font.
 *   str[0] == 0xFF marks a 2-byte (wide) string; maxlen < 0 means unlimited. Monospace fonts add
 *   fixedWidth + gSpacing (letter spacing) per character; space or code >= 256 adds spaceWidth;
 *   '\n' stops; otherwise the glyph index gCharMap[ch] (-1 = missing -> spaceWidth) gives
 *   right - left + spacing + 1, minus the kerning byte glyph.kern[prev] when the font has kerning.
 *   Returns width - gSpacing (no spacing after the last character).
 * SetupFontData (locked, internal) refreshes the cached font bank first and preserves t1/t4 by
 * IPA, which is why str lives in t1 across the call: this matches only with the real sound closure
 * (group, or the whole-program unit), never as a lone ABI file.
 *
 * STRICT MATCH. Shaping quirks (compile-affecting):
 *   - `width = 0` is written AFTER the wide/narrow if/else, not next to `pos = 0`: that takes two
 *     blocks out of width's live range (uopt priority save = totalsave / blocks: 102/10 -> 102/8),
 *     so width is coloured before the cached s[pos] byte and takes a1 (byte a2) as in retail.
 * Found with the instrumented uopt globalcolor trace; replaces fake dead read.
 *   - the parameter `str` is reused as the character counter (`str = 0; ... (s32) str < maxlen; str++`):
 *     retail keeps both in t1; a separate `s32 i` costs 27-33 words;
 *   - `prev = ch` in the monospace arm reads `ch` before any write (retail homes ch at 48(sp));
 *   - `prev = ...` is assigned in each arm, not once after the if/else (merged form is 95 words off).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

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

extern FontHdr *gFontHeader;
extern Glyph *gFontLetters;
extern s16 gCharMap[256];
extern s8 gSpacing;

void SetupFontData(s32 force);

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
