/* The options screen's text: its options and values, the flashing colours (MAT3 optionscr.c and TB_text.c). */
#include "game/game.h"

/* A colour (MAT3 tRGBA): include/game's TextColor, the flashing colours' type. */
typedef TextColor Color;

typedef struct TextHdr {
    char *str[64];
} TextHdr;

typedef struct LangInfo {
    u8 pad0[32];
    u16 base;
} LangInfo;

typedef struct Text {
    u8 pad0[4];
    TextHdr *hdr;
    u8 pad8[4];
    LangInfo *info;
    char **strings;
} Text;

typedef struct ButtonInfo {
    u8 pad0[8];
    f32 ang;
    u8 padC[48 - 12];
    f32 pos[3];
    u8 alpha;
    u8 pad3D[3];
} ButtonInfo;

VIEW(Text, gText);
#define gText gText_view

VIEW(ButtonInfo[], options_buttonInfo);
#define options_buttonInfo options_buttonInfo_view

VIEW(Color, gFlashColor1);
#define gFlashColor1 gFlashColor1_view
VIEW(Color, gFlashColor2);
#define gFlashColor2 gFlashColor2_view

VIEW(s8[], gOptions);
#define gOptions gOptions_view

VIEW(s32[14], options_validOption);
#define options_validOption options_validOption_view

/* Sets the two colours flashing text moves between (MAT3 TB_text.c's TB_FlashTextColors). */
void TB_FlashTextColors_AEAC(Color a, Color b)
{
    gFlashColor1.rgba = a.rgba;
    gFlashColor2.rgba = b.rgba;
}



/* Draws option item's name and value at (x, y): the chosen one flashing (with the 3D buttons, the one facing the
 * viewer), the others white. */
void options_printOption(s32 x, s32 y, s32 item, char *name, char *value)
{
    if (options_buttonsInited == 1) {
        if (options_buttonInfo[item].ang < 1.5707964f) {
            TB_FlashTextColors_AEAC(flashColors[0], flashColors[1]);
            TB_SetTextColor(1);
        } else {
            TB_FlashTextColors_AEAC(flashColors[0], flashColors[1]);
            TB_SetTextColor(22);
        }
    } else {
        TB_SetTextColor(item == options_gSelectedOption ? 22 : 1);
    }
    TB_DrawText(x - 105, y, name);
    TB_DrawText(x + 40, y, value);
}

#define OPT_VALUE(k) gText.strings[gText.info->base + gOptions[k]]

/* Draws the options screen's title over its 3D buttons (projected from the title button's position). */
void options_animButtonText(void)
{
    s32 i;
    s32 x;
    s32 y;
    char *str;
    s16 screenPos[2];

    HAL_SetTextDepth(0.0f);
    TB_SetJust(0, 1);
    HAL_SetTextDepth(32320.0f);
    ZOID_ProjectWindow(0, options_buttonInfo[31].pos, gCamera, 0, screenPos);   /* the title's position */
    x = screenPos[0];
    y = screenPos[1];
    HAL_SetFont(13);
    TB_SetTextColor(1);
    str = gText.hdr->str[50];
    TB_DrawText(TB_CenterText(str, x), y, str);
    HAL_SetFont(11);
    for (i = 0; i < 14; i++) {
        if ((0.69813168f < options_buttonInfo[i].ang && options_buttonInfo[i].ang < 2.443461f) || !options_buttonInfo[i].alpha) {
            continue;
        }
        HAL_SetTextTranslucency(options_buttonInfo[i].alpha);
        HAL_SetTextDepth(32384.0f);
        ZOID_ProjectWindow(0, options_buttonInfo[i].pos, gCamera, 0, screenPos);
        x = screenPos[0] + 50;
        y = screenPos[1];
        switch (i) {
        case 1:
            options_printOption(x, y, 1, gText.hdr->str[13], OPT_VALUE(10));
            break;
        case 2:
            options_printOption(x, y, 2, gText.hdr->str[15], OPT_VALUE(0));
            break;
        case 3:
            options_printOption(x, y, 3, gText.hdr->str[16], OPT_VALUE(1));
            break;
        case 4:
            options_printOption(x, y, 4, gText.hdr->str[17], OPT_VALUE(2));
            break;
        case 5:
            options_printOption(x, y, 5, gText.hdr->str[18], OPT_VALUE(3));
            break;
        case 6:
            options_printOption(x, y, 6, gText.hdr->str[19], OPT_VALUE(4));
            break;
        case 7:
            options_printOption(x, y, 7, gText.hdr->str[20], OPT_VALUE(5));
            break;
        case 8:
            options_printOption(x, y, 8, gText.hdr->str[21], OPT_VALUE(6));
            break;
        case 9:
            options_printOption(x, y, 9, gText.hdr->str[22], OPT_VALUE(7));
            break;
        case 10:
            options_printOption(x, y, 10, gText.hdr->str[23], OPT_VALUE(8));
            break;
        case 11:
            options_printOption(x, y, 11, gText.hdr->str[24], OPT_VALUE(9));
            break;
        case 12:
            options_printOption(x, y, 12, gText.hdr->str[26], OPT_VALUE(18));
            break;
        case 13:
            options_printOption(x, y, 13, gText.hdr->str[14], OPT_VALUE(11));
            break;
        case 0:
            options_printOption(x, y, 0, gText.hdr->str[25], cStrLanguageNames[gLang]);
            break;
        }
    }
    ZeroCameras();
    TB_SetJust(0, 3);
    HAL_SetTextTranslucency(-1.0f);
    TB_FlashTextColors_AEAC(flashColors[0], flashColors[1]);
    HAL_SetTextDepth(-1.0f);
}

/* The options screen's text blit's draw function: the title and every option with its value (or the 3D buttons'
 * text). Returns 1. */
s32 options_animText(void *item)
{
    u32 gs;
    s32 u0;
    s32 i;
    s32 y;
    char *str;
    s16 itemsDrawn;
    char str1[40];

    if (options_buttonsInited == 1) {
        options_animButtonText();
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(13);
    TB_SetTextColor(1);
    gs = gGameState;
    if ((gs & 0x7C0000) && options_gControllerRemoved) {
        TB_Sprintf(str1, "CONTROLLER %d %s", playerInPause + 1, gText.hdr->str[233]);
        TB_DrawText(TB_CenterText(str1, 160), 10, str1);
    } else {
        str = gText.hdr->str[50];
        TB_DrawText(TB_CenterText(str, 160), 10, str);
    }
    y = TB_GetFontLineHeight() * 2 + 10;
    HAL_SetFont(11);
    itemsDrawn = 0;
    for (i = 0; i < 14; i++) {
        if (!options_validOption[i]) {
            continue;
        }
        if (i < options_gFirstOption) {
            continue;
        }
        if (itemsDrawn >= gOptionMenuSize) {
            continue;
        }
        switch (i) {
        case 1:
            options_printOption(160, y, 1, gText.hdr->str[13], OPT_VALUE(10));
            break;
        case 2:
            options_printOption(160, y, 2, gText.hdr->str[15], OPT_VALUE(0));
            break;
        case 3:
            options_printOption(160, y, 3, gText.hdr->str[16], OPT_VALUE(1));
            break;
        case 4:
            options_printOption(160, y, 4, gText.hdr->str[17], OPT_VALUE(2));
            break;
        case 5:
            options_printOption(160, y, 5, gText.hdr->str[18], OPT_VALUE(3));
            break;
        case 6:
            options_printOption(160, y, 6, gText.hdr->str[19], OPT_VALUE(4));
            break;
        case 7:
            options_printOption(160, y, 7, gText.hdr->str[20], OPT_VALUE(5));
            break;
        case 8:
            options_printOption(160, y, 8, gText.hdr->str[21], OPT_VALUE(6));
            break;
        case 9:
            options_printOption(160, y, 9, gText.hdr->str[22], OPT_VALUE(7));
            break;
        case 10:
            options_printOption(160, y, 10, gText.hdr->str[23], OPT_VALUE(8));
            break;
        case 11:
            options_printOption(160, y, 11, gText.hdr->str[24], OPT_VALUE(9));
            break;
        case 12:
            options_printOption(160, y, 12, gText.hdr->str[26], OPT_VALUE(18));
            break;
        case 13:
            options_printOption(160, y, 13, gText.hdr->str[14], OPT_VALUE(11));
            break;
        case 0:
            options_printOption(160, y, 0, gText.hdr->str[25], cStrLanguageNames[gLang]);
            break;
        }
        y += TB_GetFontLineHeight();
        itemsDrawn++;
    }
    HAL_SetTextTranslucency(-1.0f);
    TB_FlashTextColors_AEAC(flashColors[0], flashColors[1]);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
