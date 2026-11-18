/* The video screen's text and test pattern (MAT3 videoscr.c animText, animButtonText): the screen offsets as a
 * flat menu or beside the 3D buttons, the grey ramp and colour bars that help to set them, and the test and
 * confirm panes of a refresh-rate change. */
#include "game/game.h"

typedef struct ButtonInfo { u8 pad[8]; f32 angle; u8 pad12[36]; u8 pos[16]; } ButtonInfo;   /* 64 B */
#define TEXT_VIDEO_OPTIONS 41     /* gText.tab[]: the video screen's options */

VIEW(ButtonInfo[], video_buttonInfo);
#define video_buttonInfo video_buttonInfo_view

/* The confirm pane's text width: 160. */
void vs_setWidth(s16 *width)
{
    *width = 160;
}

/* HAL_SetFont without its result. */
void vs_SetFont(s32 font)
{
    HAL_SetFont(font);
}
/* Whether line item of the video screen is offered (video_optionValid). */
s8 vs_validOption(s32 item)
{
    s8 valid = video_optionValid[item];
    return valid;
}

/* Draws the test pattern into the blit display list: a white block, a 32-step grey ramp and a black block (y 140-160),
 * then white over black blocks with red, green and blue bars between them (y 160-200). */
void vs_drawColorBars(void)
{
    s32 x;
    s32 c;
    s32 i;

    gDPPipeSync(blit_dl++);
    gDPSetCombineMode(blit_dl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimDepth(blit_dl++, 0x10, 0);
    gDPSetPrimColor(blit_dl++, 0, 0, 255, 255, 255, 255);
    gDPFillRectangle(blit_dl++, 24, 140, 32, 160);
    x = 32;
    for (c = 0; c < 32; c++) {
        gDPSetPrimColor(blit_dl++, 0, 0, c * 8, c * 8, c * 8, 255);
        gDPFillRectangle(blit_dl++, x, 140, x + 8, 160);
        x += 8;
    }
    gDPSetPrimColor(blit_dl++, 0, 0, 0, 0, 0, 255);
    gDPFillRectangle(blit_dl++, x, 140, x + 8, 160);
    gDPSetPrimColor(blit_dl++, 0, 0, 255, 255, 255, 255);
    gDPFillRectangle(blit_dl++, 24, 160, 56, 180);
    gDPSetPrimColor(blit_dl++, 0, 0, 0, 0, 0, 255);
    gDPFillRectangle(blit_dl++, 24, 180, 56, 200);
    x = 56;
    for (i = 0; i < 3; i++) {
        gDPSetPrimColor(blit_dl++, 0, 0, i == 0 ? 255 : 0, i == 1 ? 255 : 0, i == 2 ? 255 : 0, 255);   /* red, green, blue */
        gDPFillRectangle(blit_dl++, x, 160, x + 48, 200);
        x += 48;
        gDPSetPrimColor(blit_dl++, 0, 0, 255, 255, 255, 255);
        gDPFillRectangle(blit_dl++, x, 160, x + 32, 180);
        gDPSetPrimColor(blit_dl++, 0, 0, 0, 0, 0, 255);
        gDPFillRectangle(blit_dl++, x, 180, x + 32, 200);
        x += 32;
    }
    gDPPipeSync(blit_dl++);
}

/* The video screen's text, an animation function: the test pattern; with the 3D buttons up,
 * video_animButtonText; else the title (or "CONTROLLER n removed" when a controller came out during a race's
 * set-up) and the offered lines as a menu, the vertical and horizontal offsets with their signs. Returns 1. */
s32 video_animText(s32 unused)
{
    s32 i;
    s32 x;
    s32 y;
    char *str;
    char str1[40];

    vs_drawColorBars();
    if (video_buttonsInited == 1) {
        video_animButtonText();
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    vs_SetFont(13);
    TB_SetTextColor(1);
    if ((gGameState & GS_RACE_MASK) && video_gControllerRemoved) {
        TB_Sprintf(str1, "CONTROLLER %d %s", playerInPause + 1, gText.single[233]);
        TB_DrawText(TB_CenterText(str1, 160), 10, str1);
    } else {
        str = gText.single[53];
        TB_DrawText(TB_CenterText(str, 160), 10, str);
    }
    y = TB_GetFontLineHeight() * 2 + 10;
    HAL_SetFont(11);
    x = video_menuRect.cx + 5;
    for (i = 0; i < 4; i++) {
        if (!video_optionValid[i]) continue;
        TB_SetTextColor(i == gVideoMenuSelect ? 22 : 1);
        str = gText.taba[gText.tab[TEXT_VIDEO_OPTIONS] + i];
        TB_DrawText(TB_RightText(str, video_menuRect.cx - 5), y, str);
        switch (i) {
        case 0:
            TB_Sprintf(str1, "%s%d", gVertCenter > 0 ? "+" : "", gVertCenter);
            TB_DrawText(x, y, str1);
            break;
        case 1:
            TB_Sprintf(str1, "%s%d", gHorzCenter > 0 ? "+" : "", gHorzCenter);
            TB_DrawText(x, y, str1);
            break;
        }
        y += TB_GetFontLineHeight();
    }
    HAL_SetTextTranslucency(-1.0f);
    TB_FlashTextColors(flashColors[0], flashColors[1]);
    HAL_SetTextDepth(-1.0f);
    return 1;
}

/* The text of the video screen's 3D buttons: the title and each offered line's name and value at its buttons'
 * projected places, turned-away buttons skipped, turning ones half translucent; then the panes of the refresh
 * test (message 217) and its confirmation (218) while they are asked for (gTestFrequency, gConfirmFrequency). */
void video_animButtonText(void)
{
    s32 i;
    s32 j;
    s32 x;
    s32 y;
    char *str;
    char str1[40];
    s16 screenPos[2];
    static s16 testWidth;
    static s16 confirmWidth;
    s16 lines;

    vs_setWidth(&confirmWidth);
    testWidth = 160;
    HAL_SetTextDepth(0.0f);
    TB_SetJust(0, 1);
    HAL_SetFont(13);
    TB_SetTextColor(1);
    ZOID_ProjectWindow(0, video_buttonInfo[11].pos, gCamera, 0, screenPos);   /* the title's position */
    x = screenPos[0];
    y = screenPos[1];
    str = gText.single[53];
    TB_DrawText(TB_CenterText(str, x), y, str);
    HAL_SetFont(11);
    for (i = 0; i < 4; i++) {
        if (vs_validOption(i)) {
            if (video_buttonInfo[i].angle > 0.69813168f && video_buttonInfo[i].angle < 2.443461f) continue;
            if (video_buttonInfo[i].angle > 0.34906584f && video_buttonInfo[i].angle < 2.7925267f) HAL_SetTextTranslucency(128.0f);
            else HAL_SetTextTranslucency(-1.0f);
            if (video_buttonInfo[i].angle < 1.5707964f) {
                TB_FlashTextColors(flashColors[0], flashColors[1]);
                TB_SetTextColor(1);
            } else {
                TB_FlashTextColors(flashColors[0], flashColors[1]);
                TB_SetTextColor(22);
            }
            ZOID_ProjectWindow(0, video_buttonInfo[i].pos, gCamera, 0, screenPos);
            x = screenPos[0] - 55;
            y = screenPos[1];
            str = gText.taba[gText.tab[TEXT_VIDEO_OPTIONS] + i];
            TB_DrawText(x, y, str);
            ZOID_ProjectWindow(0, video_buttonInfo[i + 4].pos, gCamera, 0, screenPos);
            x = screenPos[0] - 35;
            y = screenPos[1];
            switch (i) {
            case 0:
                TB_Sprintf(str1, "%s%d", gVertCenter > 0 ? "+" : "", gVertCenter);
                TB_DrawText(x, y, str1);
                break;
            case 1:
                TB_Sprintf(str1, "%s%d", gHorzCenter > 0 ? "+" : "", gHorzCenter);
                TB_DrawText(x, y, str1);
                break;
            }
        }
    }
    TB_SetJust(1, 3);
    HAL_SetTextTranslucency(-1.0f);
    TB_FlashTextColors(flashColors[0], flashColors[1]);
    HAL_SetTextDepth(-1.0f);
    if (gTestFrequency == 1) {
        static s16 height = 1;
        HAL_SetFont(10);
        if (gTestPane == 0) {
            lines = TB_GetTextNumLines(testWidth, gText.single[217]);
            height = TB_GetFontLineHeight() * lines;
            gTestPane = InitPane(152 - testWidth / 2, 51, testWidth / 2 + 168, height + 59, 176, 0, 0, 0);
        }
        TB_DrawTextScrollBox(160, 55, testWidth, height, -1, 0, gText.single[217]);
    } else if (gTestPane != 0) {
        RemovePane(gTestPane);
        gTestPane = 0;
    }
    if (gConfirmFrequency == 1) {
        static s16 height = 1;
        HAL_SetFont(10);
        if (gConfirmPane == 0) {
            lines = TB_GetTextNumLines(confirmWidth, gText.single[218]) + 1;
            height = TB_GetFontLineHeight() * lines;
            gConfirmPane = InitPane(152 - confirmWidth / 2, 51, confirmWidth / 2 + 168, height + 59, 176, 0, 0, 0);
        }
        TB_DrawTextScrollBox(160, 55, confirmWidth, height, -1, 0, gText.single[218]);
        y = height - TB_GetFontLineHeight() + 55;
        x = 160 - confirmWidth / 4;
        str = gText.taba[gText.tab[TEXT_YES_NO]];
        TB_SetTextColor(gYesFrequency ? 22 : 1);
        TB_DrawText(x, y, str);
        x = confirmWidth / 4 + 160;
        str = gText.taba[gText.tab[TEXT_YES_NO] + 1];
        TB_SetTextColor(gYesFrequency ? 1 : 22);
        TB_DrawText(x, y, str);
    } else if (gConfirmPane != 0) {
        RemovePane(gConfirmPane);
        gConfirmPane = 0;
    }
    TB_SetJust(0, 3);
}
