/* The options screen's text: its options and values, the chosen one flashing (MAT3 optionscr.c). */
#include "game/game.h"

/* A colour (MAT3 tRGBA): include/game's TextColor, the flashing colours' type. */
typedef TextColor Color;   /* the file's name for include/game's TextColor */

#define TEXT_OFF_ON 16            /* gText.tab[]: the options' values: off, on ... */

typedef struct ButtonInfo {
    u8 pad0[8];
    f32 ang;
    u8 padC[48 - 12];
    f32 pos[3];
    u8 alpha;
    u8 pad3D[3];
} ButtonInfo;


VIEW(ButtonInfo[], options_buttonInfo);
#define options_buttonInfo options_buttonInfo_view





/* Draws option item's name and value at (x, y): the chosen one flashing (with the 3D buttons, the one facing the
 * viewer), the others white. */
void options_printOption(s32 x, s32 y, s32 item, char *name, char *value)
{
    if (options_buttonsInited == 1) {
        if (options_buttonInfo[item].ang < 1.5707964f) {
            TB_FlashTextColors(flashColors[0], flashColors[1]);
            TB_SetTextColor(1);
        } else {
            TB_FlashTextColors(flashColors[0], flashColors[1]);
            TB_SetTextColor(22);
        }
    } else {
        TB_SetTextColor(item == options_gSelectedOption ? 22 : 1);
    }
    TB_DrawText(x - 105, y, name);
    TB_DrawText(x + 40, y, value);
}

#define OPT_VALUE(k) gText.taba[gText.tab[TEXT_OFF_ON] + gOptions[k]]

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
    str = gText.single[50];
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
            options_printOption(x, y, 1, gText.single[13], OPT_VALUE(10));
            break;
        case 2:
            options_printOption(x, y, 2, gText.single[15], OPT_VALUE(0));
            break;
        case 3:
            options_printOption(x, y, 3, gText.single[16], OPT_VALUE(1));
            break;
        case 4:
            options_printOption(x, y, 4, gText.single[17], OPT_VALUE(2));
            break;
        case 5:
            options_printOption(x, y, 5, gText.single[18], OPT_VALUE(3));
            break;
        case 6:
            options_printOption(x, y, 6, gText.single[19], OPT_VALUE(4));
            break;
        case 7:
            options_printOption(x, y, 7, gText.single[20], OPT_VALUE(5));
            break;
        case 8:
            options_printOption(x, y, 8, gText.single[21], OPT_VALUE(6));
            break;
        case 9:
            options_printOption(x, y, 9, gText.single[22], OPT_VALUE(7));
            break;
        case 10:
            options_printOption(x, y, 10, gText.single[23], OPT_VALUE(8));
            break;
        case 11:
            options_printOption(x, y, 11, gText.single[24], OPT_VALUE(9));
            break;
        case 12:
            options_printOption(x, y, 12, gText.single[26], OPT_VALUE(18));
            break;
        case 13:
            options_printOption(x, y, 13, gText.single[14], OPT_VALUE(11));
            break;
        case 0:
            options_printOption(x, y, 0, gText.single[25], cStrLanguageNames[gLang]);
            break;
        }
    }
    ZeroCameras();
    TB_SetJust(0, 3);
    HAL_SetTextTranslucency(-1.0f);
    TB_FlashTextColors(flashColors[0], flashColors[1]);
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
        TB_Sprintf(str1, "CONTROLLER %d %s", playerInPause + 1, gText.single[233]);
        TB_DrawText(TB_CenterText(str1, 160), 10, str1);
    } else {
        str = gText.single[50];
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
            options_printOption(160, y, 1, gText.single[13], OPT_VALUE(10));
            break;
        case 2:
            options_printOption(160, y, 2, gText.single[15], OPT_VALUE(0));
            break;
        case 3:
            options_printOption(160, y, 3, gText.single[16], OPT_VALUE(1));
            break;
        case 4:
            options_printOption(160, y, 4, gText.single[17], OPT_VALUE(2));
            break;
        case 5:
            options_printOption(160, y, 5, gText.single[18], OPT_VALUE(3));
            break;
        case 6:
            options_printOption(160, y, 6, gText.single[19], OPT_VALUE(4));
            break;
        case 7:
            options_printOption(160, y, 7, gText.single[20], OPT_VALUE(5));
            break;
        case 8:
            options_printOption(160, y, 8, gText.single[21], OPT_VALUE(6));
            break;
        case 9:
            options_printOption(160, y, 9, gText.single[22], OPT_VALUE(7));
            break;
        case 10:
            options_printOption(160, y, 10, gText.single[23], OPT_VALUE(8));
            break;
        case 11:
            options_printOption(160, y, 11, gText.single[24], OPT_VALUE(9));
            break;
        case 12:
            options_printOption(160, y, 12, gText.single[26], OPT_VALUE(18));
            break;
        case 13:
            options_printOption(160, y, 13, gText.single[14], OPT_VALUE(11));
            break;
        case 0:
            options_printOption(160, y, 0, gText.single[25], cStrLanguageNames[gLang]);
            break;
        }
        y += TB_GetFontLineHeight();
        itemsDrawn++;
    }
    HAL_SetTextTranslucency(-1.0f);
    TB_FlashTextColors(flashColors[0], flashColors[1]);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
