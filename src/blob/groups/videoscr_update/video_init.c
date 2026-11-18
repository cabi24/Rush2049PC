/* The video options screen's start: its font, menu rectangle and blits (MAT3 videoscr.c). */
#include "game/game.h"

typedef struct Blit Blit;
typedef struct MultiBlit MultiBlit;

typedef union Color { u8 c[4]; u32 rgba; } Color;

void *memcpy(void *, const void *, u32);

/* Sets the font under the text lock (HAL_SetFont, inline). */
void vi_SetFont(s32 font)
{
    text_lock();
    HAL_SetFontInternal(font);
    text_unlock();
}

/* Sizes the video screen's menu rectangle to its options' widest text. */
void video_calcMenuRect(void)
{
    s32 menuSize;
    s32 optionWidth;
    s32 valueWidth;
    s32 menuIdx;
    u8 str1[40];

    vi_SetFont(11);
    menuSize = 0;
    optionWidth = 0;
    valueWidth = 0;
    for (menuIdx = 0; menuIdx < 4; menuIdx++) {
        if (video_validOption(menuIdx) == 1) {
            menuSize++;
            optionWidth = HAL_GetTextWidth(gText.taba[gText.tab[41] + menuIdx], -1) > optionWidth ?
                HAL_GetTextWidth(gText.taba[gText.tab[41] + menuIdx], -1) : optionWidth;
            if (menuIdx == 2) {
                TB_Sprintf((char *)str1, "%s  %s", (gText.taba + gText.tab[42])[0],
                             (gText.taba + gText.tab[42])[1]);
                valueWidth = HAL_GetTextWidth(str1, -1) > valueWidth ? HAL_GetTextWidth(str1, -1) : valueWidth;
            } else {
                valueWidth = HAL_GetTextWidth("000", -1) > valueWidth ? HAL_GetTextWidth("000", -1) : valueWidth;
            }
        }
    }
    video_menuRect.w = optionWidth + valueWidth + 10;
    video_menuRect.h = menuSize * TB_GetFontLineHeight();
    video_menuRect.x = 160 - video_menuRect.w / 2;
    video_menuRect.cx = video_menuRect.x + optionWidth + 5;
}

/* Starts the video screen: its blits (from the front end or the pause screen), buttons and menu rectangle. */
void video_initScreen(void)
{
    s32 u0, u1, u2, u3;

    SaveGlobals();
    video_calcMenuRect();
    if (gGameState & GS_FRONTEND_MASK) {
        createBG();
        {
            Color buttonColor = { { 255, 255, 255, 255 } };
            tTexture *tex;
            u16 texIdx;
            s32 buttonIdx;

            for (buttonIdx = 0; buttonIdx < 12; buttonIdx++) {
                if (video_buttonInfo[buttonIdx].handle == -1) {
                    memcpy(video_buttonInfo[buttonIdx].matrix, unitmatrix, 36);
                    video_buttonInfo[buttonIdx].handle = MBOX_NewObject(
                        MBOX_FindObject_WORK(video_buttonNames[video_buttonInfo[buttonIdx].type], 0, gNumModels[0] - 1, 1),
                        &video_buttonInfo[buttonIdx].matrix, -1,
                        (video_buttonInfo[buttonIdx].type == 0 || video_buttonInfo[buttonIdx].type == 1 ||
                         video_buttonInfo[buttonIdx].type == 2) ? 0x42000 : 0);
                    ZOID_SetColor(video_buttonInfo[buttonIdx].handle, &buttonColor);
                }
            }
            tex = MBOX_FindTexture_WORK("CURSOR", &texIdx, 0, gNumModels[0] - 1, 1);
            ZOID_SetTexture(video_buttonInfo[8].handle, tex, -1);
            tex = MBOX_FindTexture_WORK("B1", &texIdx, 0, gNumModels[0] - 1, 1);
            ZOID_SetTexture(video_buttonInfo[9].handle, tex, -1);
            memcpy(video_buttonInfo[9].matrix, unitmatrix, 36);
            WPitchUV(-1.5707964f, video_buttonInfo[9].matrix);
            video_buttonInfo[9].position[0] = -75.1f;
            video_buttonInfo[9].position[1] = 55.2f;
            video_buttonInfo[9].position[2] = 100.0f;
            ScaleUV(video_buttonInfo[9].matrix, video_buttonInfo[9].matrix, 0.5f);
            memcpy(video_buttonInfo[10].matrix, unitmatrix, 36);
            WPitchUV(-1.5707964f, video_buttonInfo[10].matrix);
            video_buttonInfo[10].position[0] = -87.1f;
            video_buttonInfo[10].position[1] = 55.2f;
            video_buttonInfo[10].position[2] = 100.0f;
            tex = MBOX_FindTexture_WORK("BUTTON_TITLE", &texIdx, 0, gNumModels[0] - 1, 1);
            ZOID_SetTexture(video_buttonInfo[11].handle, tex, -1);
            memcpy(video_buttonInfo[11].matrix, unitmatrix, 36);
            WPitchUV(-1.5707964f, video_buttonInfo[11].matrix);
            video_buttonInfo[11].position[0] = 0.0f;
            video_buttonInfo[11].position[1] = 115.0f;
            video_buttonInfo[11].position[2] = 200.0f;
            video_buttonsInited = 1;
            video_buttonsJustInited = 1;
            ZeroCameras();
        }
        video_blit = NewMultiBlit(0, 0, &video_frontEndBlit, 2);
    } else {
        video_blit = NewMultiBlit(0, 0, &video_pauseBlit, 2);
    }
    gVertCenter = gOptions[20];
    gHorzCenter = gOptions[19];
    ClearRushControls();
    while (!video_validOption(gVideoMenuSelect)) {
        if (++gVideoMenuSelect >= 4) {
            gVideoMenuSelect = 0;
            break;
        }
    }
    gTestFrequency = 0;
    gConfirmFrequency = 0;
    gYesFrequency = 0;
    exitVideoScreen = 0;
    video_inited = 1;
}
