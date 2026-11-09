/* DrawText: draw a string with the blitter, glyph by glyph (MAT3 HAL_text.c's _DrawText). */
#include "game/game.h"

/* PC: a font texture is a TXHD record of the font's WAD, which keeps its N64 layout (port/assets/wad.c): image is a
 * 32-bit RDRAM-window address. */
#define TXHD_IMAGE unsigned int image
typedef struct tTexture {
    unsigned char pad00[16];
    unsigned short width;           /* 0x10 */
    unsigned short height;          /* 0x12 */
    unsigned char pad14[4];
    TXHD_IMAGE;                     /* 0x18 */
} tTexture;

/* Draws length bytes of text (a single- or double-byte string, its mark and ending zeros counted) at the text position
 * in the current font, colour, translucency and depth, now: the blitter's state set from gFontAttribute, the scissor
 * box the whole screen, the font's colours loaded, then a BlitDraw per glyph, loading each texture page as the glyphs
 * need it. A newline goes to the next line (ascent + descent + leading below, back to the start's x); a space, a
 * character over 255 or one the font lacks advances by a space; a fixed-width font centres each glyph in maxWidth;
 * a proportional one with a kerning table pulls a letter towards the one before it. HAL_FinishText calls it for the
 * buffered text, HAL_DrawText for text with a depth. */
void DrawText(const unsigned char *text, unsigned short length)
{
    int i;
    int x;
    int x0;
    int y0;
    int step;
    int ch;
    int chPrev;
    int lastPage;
    const unsigned char *t;
    int dbl;
    FontLetter *letter;
    SetupFontData(0);
    BlitScissor(0, 0, screen_width - 1, screen_height - 1);
    BlitSetAttribute(gFontAttribute);
    BlitClearAttribute(~gFontAttribute);
    blitColors = 0;
    if (gFontAttribute & BLIT_ATTR_XLU)
        gFontColor.c[3] = gTrans;
    BlitColors(&gFontColor, font_image_format, font_texel_size, 0);
    lastPage = 0;
    blit_load_texture(gFontTextures[0]->image, font_image_format, font_texel_size, gFontTextures[0]->width, gFontTextures[0]->height, 0, 0, gFontTextures[0]->width - 1, gFontTextures[0]->height - 1, 0, 0);
    if (text[0] == TEXT_DOUBLE_BYTE) {
        t = text + 1;
        dbl = 1;
        step = 2;
        length -= 2;
    } else {
        t = text;
        dbl = 0;
        step = 1;
        length -= 1;
    }
    x0 = gFontXpos;
    y0 = gFontYpos;
    i = 0;
    chPrev = -1;
    for (; i < length;) {
        if (dbl)
            ch = (t[i] << 8) | t[i + 1];
        else
            ch = t[i];
        if (ch == 10) {
            y0 += gFontHeader->ascent + gFontHeader->descent + gLeading;
            x0 = gFontXpos;
            ch = -1;
        } else {
            if (ch == 32 || ch > 255) {
            space:
                x0 += gFontHeader->nonProp ? gFontHeader->maxWidth + gSpacing : gFontHeader->spaceWidth;
                ch = -1;
            } else {
                ch = gCharMap[ch];
                if (ch < 0)
                    goto space;
                if (chPrev > 0 && gFontHeader->kern && !gFontHeader->nonProp)
                    x0 -= ((unsigned char *)PC_N64P(gFontLetters[ch].kerns))[chPrev];
                letter = gFontLetters + ch;
                if (letter->texIdx != lastPage) {
                    lastPage = letter->texIdx;
                    blit_load_texture(gFontTextures[lastPage]->image, font_image_format, font_texel_size, gFontTextures[lastPage]->width, gFontTextures[lastPage]->height, 0, 0, gFontTextures[lastPage]->width - 1, gFontTextures[lastPage]->height - 1, 0, 0);
                }
                x = (gFontHeader->nonProp ? (gFontHeader->maxWidth - (letter->lrs - letter->uls + 1)) / 2 : 0) + x0;
                BlitDraw(x, y0, x + letter->lrs - letter->uls, y0 + letter->lrt - letter->ult, letter->uls, letter->ult);
                x0 += (gFontHeader->nonProp ? gFontHeader->maxWidth : letter->lrs - letter->uls + 1) + gSpacing;
            }
        }
        i += step;
        chPrev = ch;
    }
}
