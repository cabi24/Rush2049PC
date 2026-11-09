#include "game/game.h"

typedef struct anon_version_type_10 {
    unsigned char version, type, ascent, descent, leading, totalHeight, maxWidth, spaceWidth;
    unsigned char spacing, nonProp, kern, numTextures, numChars, pad16[3];
} anon_version_type_10;
/* PC: a font letter is 12 bytes as in the file, its kern slot a 4-byte window address (port/assets/font.c);
 * FONT_KERNS reads it as a pointer. */
typedef struct anon_kerns_value_c {
    unsigned int kerns;
    unsigned char value, texIdx, uls, ult, lrs, lrt;
    signed short pad12;
} anon_kerns_value_c;
#define FONT_KERNS(k) ((unsigned char *)PC_N64P(k))
typedef struct anon_fore_4 { unsigned char rgba[4]; } anon_fore_4;
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

VIEW(anon_version_type_10 *, gFontHeader);
#define gFontHeader gFontHeader_view

VIEW(tTexture *[16], gFontTextures);
#define gFontTextures gFontTextures_view
VIEW(s16[256], gCharMap);
#define gCharMap gCharMap_view
VIEW(anon_fore_4, gFontColor);
#define gFontColor gFontColor_view

VIEW(s32, gFontAttribute);
#define gFontAttribute gFontAttribute_view

VIEW(void *, blitColors);
#define blitColors blitColors_view

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
    anon_kerns_value_c *l;
    SetupFontData(0);
    BlitScissor(0, 0, screen_width - 1, screen_height - 1);
    BlitSetAttribute(gFontAttribute);
    BlitClearAttribute(~gFontAttribute);
    blitColors = 0;
    if (gFontAttribute & 32)
        gFontColor.rgba[3] = gTrans;
    BlitColors(&gFontColor, font_image_format, font_texel_size, 0);
    lastPage = 0;
    blit_load_texture(gFontTextures[0]->image, font_image_format, font_texel_size, gFontTextures[0]->width, gFontTextures[0]->height, 0, 0, gFontTextures[0]->width - 1, gFontTextures[0]->height - 1, 0, 0);
    if (text[0] == 255) {
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
                    x0 -= FONT_KERNS(gFontLetters[ch].kerns)[chPrev];
                l = gFontLetters + ch;
                if (l->texIdx != lastPage) {
                    lastPage = l->texIdx;
                    blit_load_texture(gFontTextures[lastPage]->image, font_image_format, font_texel_size, gFontTextures[lastPage]->width, gFontTextures[lastPage]->height, 0, 0, gFontTextures[lastPage]->width - 1, gFontTextures[lastPage]->height - 1, 0, 0);
                }
                x = (gFontHeader->nonProp ? (gFontHeader->maxWidth - (l->lrs - l->uls + 1)) / 2 : 0) + x0;
                BlitDraw(x, y0, x + l->lrs - l->uls, y0 + l->lrt - l->ult, l->uls, l->ult);
                x0 += (gFontHeader->nonProp ? gFontHeader->maxWidth : l->lrs - l->uls + 1) + gSpacing;
            }
        }
        i += step;
        chPrev = ch;
    }
}
