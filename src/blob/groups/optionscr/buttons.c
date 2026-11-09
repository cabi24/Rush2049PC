#include "game/game.h"

typedef union Color {
    u8 c[4];
    u32 rgba;
} Color;

typedef struct {
    s32 type;            /* 0x00 */
    s32 handle;          /* 0x04 */
    f32 ang;             /* 0x08 */
    f32 mat[3][3];       /* 0x0C */
    f32 pos[3];          /* 0x30 */
    u8 alpha;            /* 0x3C */
    u8 pad3D[3];
} Button; /* 0x40 */

VIEW(Color, options_buttonColor);
#define options_buttonColor options_buttonColor_view
VIEW(Button[], options_buttonInfo);
#define options_buttonInfo options_buttonInfo_view

VIEW(f32[], unitmatrix);
#define unitmatrix unitmatrix_view
VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view
VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view

f32 fabsf(f32);
#pragma intrinsic(fabsf)
void *memcpy(void *, const void *, u32);
intptr_t MBOX_FindTexture_WORK();
void ZOID_SetTexture(s16 handle, intptr_t tex, s32 c);

VIEW(intptr_t, options_blit);
#define options_blit options_blit_view

void RemoveBlit(intptr_t sound_id);

void options_uninitScreen(void)
{
    if (!(gGameState & 0x7C03FFFE)) {
        RemovePanes(0);
        options_gTitlePane = 0;
    }
    if (options_blit != 0) {
        RemoveBlit(options_blit);
        options_blit = 0;
    }
    removeBG();
    {
        s32 i;
        for (i = 0; i < 32; i++) {
            if (options_buttonInfo[i].handle != -1) {
                RemoveObjectTree(options_buttonInfo[i].handle, 0, 0);
                options_buttonInfo[i].handle = -1;
            }
        }
        options_buttonsInited = 0;
    }
    options_inited = 0;
}

void options_updateButtons(void) {
    static f32 cursorSpeed;
    static f32 scrollSpeed;
    s32 i;
    s32 j;
    s32 n;
    f32 y;
    f32 old[3];
    f32 sel_y;
    s8 flag;
    intptr_t name;
    s32 yi;
    s32 u0;
    s16 tex;
    Color color;

    flag = 1;
    color.rgba = options_buttonColor.rgba;
    if (options_buttonsInited == 0)
        return;
    n = 0;
    for (j = 0; j < options_gSelectedOption; j++) {
        if (options_validOption[j] != 0)
            n++;
    }
    memcpy(old, options_buttonInfo[28].pos, 12);
    options_buttonInfo[28].ang += 6.2831855f * gFrameSeconds;
    if (6.2831855f < options_buttonInfo[28].ang)
        options_buttonInfo[28].ang -= 6.2831855f;
    memcpy(options_buttonInfo[28].mat, unitmatrix, 36);
    sel_y = 75.0f - n * 165.0f / (gMenuCount - 1);
    options_buttonInfo[28].pos[0] = -160.0f;
    options_buttonInfo[28].pos[2] = 225.0f;
    options_buttonInfo[28].pos[1] = sel_y;
    if (options_buttonsJustInited == 0) {
        y = options_buttonInfo[28].pos[1];
        if (cursorSpeed < fabsf(y - old[1]) * 60.0f / 10.0f) {
            cursorSpeed = fabsf(y - old[1]) * 60.0f / 10.0f;
            if (!(cursorSpeed < 540.0f))
                cursorSpeed = 540.0f;
            if (!(180.0f < cursorSpeed))
                cursorSpeed = 180.0f;
        }
        if (old[1] + cursorSpeed * gFrameSeconds < y)
            options_buttonInfo[28].pos[1] = old[1] + cursorSpeed * gFrameSeconds;
        else if (y < old[1] - cursorSpeed * gFrameSeconds)
            options_buttonInfo[28].pos[1] = old[1] - cursorSpeed * gFrameSeconds;
        else
            cursorSpeed = 180.0f;
    }
    WRollUV(options_buttonInfo[28].ang, options_buttonInfo[28].mat);
    WPitchUV(1.5707964f, options_buttonInfo[28].mat);
    do {} while (0);
    ScaleUV(options_buttonInfo[28].mat, options_buttonInfo[28].mat, 0.4f);
    yi = sel_y * 1.33f + n * 40;
    for (i = 0; i < 14; i++) {
        if (i == options_gSelectedOption) {
            if (options_buttonInfo[i].ang < 3.1415927f)
                options_buttonInfo[i].ang += 12.566371f * gFrameSeconds;
            if (3.1415927f < options_buttonInfo[i].ang)
                options_buttonInfo[i].ang = 3.1415927f;
            if (options_buttonsJustInited != 0)
                options_buttonInfo[i].ang = 3.1415927f;
        } else {
            if (0.0f < options_buttonInfo[i].ang)
                options_buttonInfo[i].ang -= 12.566371f * gFrameSeconds;
            if (options_buttonInfo[i].ang < 0.0f)
                options_buttonInfo[i].ang = 0.0f;
            if (options_buttonsJustInited != 0)
                options_buttonInfo[i].ang = 0.0f;
        }
        options_buttonInfo[i + 14].ang = options_buttonInfo[i].ang;
        memcpy(options_buttonInfo[i].mat, unitmatrix, 36);
        memcpy(options_buttonInfo[i + 14].mat, unitmatrix, 36);
        memcpy(old, options_buttonInfo[i].pos, 12);
        options_buttonInfo[i].pos[0] = -90.0f;
        options_buttonInfo[i].pos[1] = yi;
        options_buttonInfo[i].pos[2] = 300.0f;
        if (options_buttonsJustInited == 0) {
            if (scrollSpeed < fabsf(options_buttonInfo[i].pos[1] - old[1]) * 60.0f / 10.0f) {
                scrollSpeed = fabsf(options_buttonInfo[i].pos[1] - old[1]) * 60.0f / 10.0f;
                if (!(scrollSpeed < 540.0f))
                    scrollSpeed = 540.0f;
            }
            if (old[1] + scrollSpeed * gFrameSeconds < options_buttonInfo[i].pos[1]) {
                flag = 0;
                options_buttonInfo[i].pos[1] = old[1] + scrollSpeed * gFrameSeconds;
            } else if (options_buttonInfo[i].pos[1] < old[1] - scrollSpeed * gFrameSeconds) {
                flag = 0;
                options_buttonInfo[i].pos[1] = old[1] - scrollSpeed * gFrameSeconds;
            }
        }
        options_buttonInfo[i + 14].pos[0] = 150.0f;
        options_buttonInfo[i + 14].pos[1] = options_buttonInfo[i].pos[1] * 31.0f / 30.0f;
        options_buttonInfo[i + 14].pos[2] = 310.0f;
        WPitchUV(options_buttonInfo[i].ang - 1.5707964f, options_buttonInfo[i].mat);
        WPitchUV(options_buttonInfo[i + 14].ang - 1.5707964f, options_buttonInfo[i + 14].mat);
        if (1.5707964f < options_buttonInfo[i].ang)
            name = MBOX_FindTexture_WORK("BUTTON_SELECT", &tex, 0, (s8)(gNumModels - 1), 1);
        else
            name = MBOX_FindTexture_WORK("BUTTON", &tex, 0, (s8)(gNumModels - 1), 1);
        ZOID_SetTexture(options_buttonInfo[i].handle, name, -1);
        ZOID_SetTexture(options_buttonInfo[i + 14].handle, name, -1);
        if (140.0f < options_buttonInfo[i].pos[1] || options_buttonInfo[i].pos[1] < -160.0f)
            options_buttonInfo[i].alpha = 0;
        else if (100.0f < options_buttonInfo[i].pos[1])
            options_buttonInfo[i].alpha = 255.0f - (options_buttonInfo[i].pos[1] - 100.0f) * 255 / 40.0f;
        else if (options_buttonInfo[i].pos[1] < -120.0f)
            options_buttonInfo[i].alpha = 255.0f - (-120.0f - options_buttonInfo[i].pos[1]) * 255 / 40.0f;
        else
            options_buttonInfo[i].alpha = 255;
        options_buttonInfo[i + 14].alpha = options_buttonInfo[i].alpha;
        color.c[3] = options_buttonInfo[i].alpha;
        do {} while (0);
        ZOID_SetColor(options_buttonInfo[i].handle, &color);
        ZOID_SetColor(options_buttonInfo[i + 14].handle, &color);
        if (options_validOption[i] == 0) {
            MBOX_HideObject(options_buttonInfo[i].handle, 0, 15);
            MBOX_HideObject(options_buttonInfo[i + 14].handle, 0, 15);
            options_buttonInfo[i].alpha = 0;
        } else {
            if (options_buttonInfo[i].alpha == 0) {
                MBOX_HideObject(options_buttonInfo[i].handle, 0, 15);
                MBOX_HideObject(options_buttonInfo[i + 14].handle, 0, 15);
            } else {
                MBOX_ShowObject(options_buttonInfo[i].handle, 0, 15);
                MBOX_ShowObject(options_buttonInfo[i + 14].handle, 0, 15);
            }
            yi -= 40.0f;
        }
    }
    if (yi) {}
    if (yi) {}
    if (flag == 1)
        scrollSpeed = 0.0f;
    options_buttonsJustInited = 0;
}


typedef struct {
    volatile s8 b0;
    u8 pad[15];
} Pad16;

VIEW(s8[], gOptions);
#define gOptions gOptions_view

VIEW(Pad16[], gControllerData);
#define gControllerData gControllerData_view

void updateOptionScreen(void) {
    Pad16 *p;
    s32 btn;
    s32 rep;
    s32 dir;
    s32 x;

    if (gGameState & 0x7C03FFFE) {
        btn = gActiveEdges;
        rep = gActiveAutoEdges;
    } else {
        btn = gContEdges[playerInPause];
        rep = gAutoEdges[playerInPause];
    }
    if (options_inited == 0)
        options_initScreen();
    if (btn & 5) {
        PlayStartABSound(btn);
        exitOptionScreen = 2;
    } else if (rep & 0x400) {
        PlayUDSound(rep);
        do {
            options_gSelectedOption--;
            if (options_gSelectedOption < 0)
                options_gSelectedOption = 13;
        } while (!options_validOption[options_gSelectedOption]);
    } else if (rep & 0x800) {
        PlayUDSound(rep);
        do {
            options_gSelectedOption++;
            if (options_gSelectedOption >= 14)
                options_gSelectedOption = 0;
        } while (!options_validOption[options_gSelectedOption]);
    }
    x = options_gSelectedOption - gOptionMenuSize / 2;
    if (options_gFirstOption < x)
        options_gFirstOption = x;
    if (x < options_gFirstOption)
        options_gFirstOption = x;
    if (14 - gOptionMenuSize < options_gFirstOption)
        options_gFirstOption = 14 - gOptionMenuSize;
    if (options_gFirstOption < 0)
        options_gFirstOption = 0;
    if (btn & 0x3000) {
        dir = (btn & 0x1000) ? -1 : 1;
        PlayLRSound(btn);
        switch (options_gSelectedOption) {
        case 1:
            gOptions[10] = !gOptions[10];
            break;
        case 2:
            gOptions[0] ^= 1;
            break;
        case 3:
            gOptions[1] ^= 1;
            break;
        case 4:
            gOptions[2] ^= 1;
            break;
        case 5:
            gOptions[3] ^= 1;
            break;
        case 6:
            gOptions[4] ^= 1;
            break;
        case 7:
            gOptions[5] ^= 1;
            break;
        case 8:
            gOptions[6] ^= 1;
            break;
        case 9:
            gOptions[7] ^= 1;
            break;
        case 10:
            gOptions[8] ^= 1;
            break;
        case 11:
            gOptions[9] ^= 1;
            break;
        case 12:
            gOptions[18] ^= 1;
            break;
        case 13:
            gOptions[11] ^= 1;
            break;
        case 0:
            gLang += dir;
            if (gLang < 0)
                gLang = 5;
            else if (gLang >= 6)
                gLang = 0;
            gOptions[16] = gLang;
            gOptions[17] = 1;
            LoadTextWave();
            break;
        }
        if (!(gGameState & 0x7C03FFFE))
            SetOptions();
    }
    options_updateButtons();
    if (gGameState & 0x7C0000) {
        p = &gControllerData[playerInPause];
        if (!p->b0 != options_gControllerRemoved) {
            options_gControllerRemoved = !p->b0;
            options_TitlePane();
        }
    }
    if (gSoftReset) {
        options_uninitScreen();
        if (gGameState & 0x7C03FFFE)
            gNewGameState = 4;
        else
            setPauseState(1);
    } else if (exitOptionScreen) {
        options_uninitScreen();
        if (gGameState & 0x7C03FFFE)
            gNewGameState = 16;
        else
            setPauseState(1);
    }
}
