/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * TB_DrawTextScrollBox (0x800BE078, 1084 B) = MAT3 TB_text.c TB_DrawTextScrollBox (GC 0x8004fd1c): draws `text`
 * word-wrapped into a box, starting at `page`/`line` (the historical label is wrong). , .
 * Direct typed port of the MAT3 body; callees (N64 names): TB_GetFontLineHeight = TB_GetFontLineHeight,
 * TB_GetTextNumLines = TB_GetTextNumLines (locked), camera_shake_update = HAL_GetCharWidth, HAL_GetTextWidth =
 * HAL_GetTextWidth, HAL_DrawText = HAL_DrawText; HAL_GetFontSpacing = HAL_GetFontSpacing and HAL_SetTextPos =
 * HAL_SetTextPos (N64 version keeps the -32768 "unchanged" sentinel) are the locked kept getters, inlined by -O3
 * and therefore defined here verbatim (same precedent as TB_GetTextNumLines.c's HAL_GetFontSpacing).
 * Data: gJust = gJust {h, v}; gSpacing gSpacing; gFontXpos/gFontYpos gFontXpos/gFontYpos.
 * N64 differences from the GC C (all from the retail words, no devices):
 *  - `tw` is a function-scope s32 declared first (its slot is the frame's top word; GC has a block `short tw`),
 *    and the justified x is passed straight to HAL_SetTextPos as one conditional expression (GC: `xpos` if/else);
 *    the if/else forms colour the two arms the other way round.
 *  - `str = (u8 *)s + charSize` (GC `(unsigned char *)(s + charSize)`) gives retail's addu operand order.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

typedef struct { s32 h, v; } Just;
extern Just gJust;  /* gJust */
extern s8 gSpacing;    /* gSpacing */
extern s16 gFontXpos;   /* gFontXpos */
extern s16 gFontYpos;   /* gFontYpos */
extern s16 TB_GetFontLineHeight(void);          /* TB_GetFontLineHeight */
extern s16 TB_GetTextNumLines(s16 width, void *text);  /* TB_GetTextNumLines */
extern u32 HAL_GetCharWidth(u16 ch);             /* HAL_GetCharWidth */
extern u32 HAL_GetTextWidth(void *text, s16 numChars);  /* HAL_GetTextWidth */
extern void HAL_DrawText(void *text, s16 numChars);    /* HAL_DrawText */

s8 HAL_GetFontSpacing(void);

void HAL_SetTextPos(s16 x, s16 y);

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
