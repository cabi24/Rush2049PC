/* The options screen's set-up (MAT3 optionscr.c calcOptionMenuMaxSize and initButtons). */
#include "game/game.h"

typedef struct TexDef TexDef;

typedef struct { s32 type, handle; f32 ang; f32 mat[3][3]; f32 pos[3]; u8 pad3C[4]; } Button;   /* 0x40 */

typedef union Color { u8 c[4]; u32 rgba; } Color;

VIEW(Button[32], options_buttonInfo);
#define options_buttonInfo options_buttonInfo_view
void *memcpy(void *, const void *, u32);

/* Counts the options offered (gMenuCount) and how many lines of them fit below the title (gOptionMenuSize). */
void op_calcOptionMenuMaxSize(void)
{
    s32 height;
    s32 i;

    HAL_SetFont(13);
    height = 210 - TB_GetFontLineHeight() * 2;
    HAL_SetFont(11);
    height -= TB_GetFontLineHeight();
    gMenuCount = 0;
    for (i = 0; i < 14; i++) {
        if (options_validOption[i]) gMenuCount++;
    }
    gOptionMenuSize = gMenuCount < height / TB_GetFontLineHeight() ? gMenuCount : height / TB_GetFontLineHeight();
}

/* Makes the options screen's 3D buttons that don't exist yet (the first three kinds lit differently), then sets up
 * the cursor (28), the B button (29), its frame (30) and the title button (31), and resets the cameras. */
void op_initButtons(void)
{
    Color buttonColor = { { 255, 255, 255, 255 } };
    TexDef *tex;
    u16 texIdx;
    s32 buttonIdx;

    for (buttonIdx = 0; buttonIdx < 32; buttonIdx++) {
        if (options_buttonInfo[buttonIdx].handle == -1) {
            memcpy(options_buttonInfo[buttonIdx].mat, unitmatrix, 36);
            options_buttonInfo[buttonIdx].handle = MBOX_NewObject(
                MBOX_FindObject_WORK(options_buttonNames[options_buttonInfo[buttonIdx].type], 0, gNumModels[0] - 1, 1),
                &options_buttonInfo[buttonIdx].mat, -1,
                (options_buttonInfo[buttonIdx].type == 0 || options_buttonInfo[buttonIdx].type == 1 ||
                 options_buttonInfo[buttonIdx].type == 2) ? 0x42000 : 0);
            ZOID_SetColor(options_buttonInfo[buttonIdx].handle, &buttonColor);
        }
    }
    tex = MBOX_FindTexture_WORK("CURSOR", &texIdx, 0, gNumModels[0] - 1, 1);
    ZOID_SetTexture(options_buttonInfo[28].handle, tex, -1);
    tex = MBOX_FindTexture_WORK("B1", &texIdx, 0, gNumModels[0] - 1, 1);
    ZOID_SetTexture(options_buttonInfo[29].handle, tex, -1);
    memcpy(options_buttonInfo[29].mat, unitmatrix, 36);
    WPitchUV(-1.5707964f, options_buttonInfo[29].mat);
    options_buttonInfo[29].pos[0] = -75.1f;
    options_buttonInfo[29].pos[1] = 55.2f;
    options_buttonInfo[29].pos[2] = 100.0f;
    ScaleUV(options_buttonInfo[29].mat, options_buttonInfo[29].mat, 0.5f);
    memcpy(options_buttonInfo[30].mat, unitmatrix, 36);
    WPitchUV(-1.5707964f, options_buttonInfo[30].mat);
    options_buttonInfo[30].pos[0] = -87.1f;
    options_buttonInfo[30].pos[1] = 55.2f;
    options_buttonInfo[30].pos[2] = 100.0f;
    tex = MBOX_FindTexture_WORK("BUTTON_TITLE", &texIdx, 0, gNumModels[0] - 1, 1);
    ZOID_SetTexture(options_buttonInfo[31].handle, tex, -1);
    memcpy(options_buttonInfo[31].mat, unitmatrix, 36);
    WPitchUV(-1.5707964f, options_buttonInfo[31].mat);
    options_buttonInfo[31].pos[0] = 0.0f;
    options_buttonInfo[31].pos[1] = 115.0f;
    options_buttonInfo[31].pos[2] = 200.0f;
    options_buttonsInited = 1;
    options_buttonsJustInited = 1;
    ZeroCameras();
}
