/* The options screen (MAT3 optionscr.c): the language and thirteen on/off game options (gOptions), a list of 3D
 * buttons in the front end (GS_OPTIONS), a flat list in the pause menu. This file has its frame
 * (updateOptionScreen), its tear-down and its buttons' layout; option_init.c sets it up and
 * optionscr's animators draw its text. */
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
} Button; /* 0x40, MAT3 tMenuButton */

/* options_buttonInfo[]: the rows' buttons, a label and a value button for each of the 14 rows, then the cursor. */
#define OPTION_ROW_LANGUAGE 0

VIEW(Color, options_buttonColor);
#define options_buttonColor options_buttonColor_view
VIEW(Button[], options_buttonInfo);
#define options_buttonInfo options_buttonInfo_view

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view
f32 fabsf(f32);
#pragma intrinsic(fabsf)
void *memcpy(void *, const void *, u32);

VIEW(intptr_t, options_blit);
#define options_blit options_blit_view

/* Take the screen down: the pause menu's panes, the text blit, the background and the buttons. */
void options_uninitScreen(void)
{
    if (!(gGameState & GS_FRONTEND_MASK)) {
        RemovePanes(0);
        options_gTitlePane = 0;
    }
    if (options_blit != 0) {
        RemoveBlit((tBlit *) options_blit);
        options_blit = 0;
    }
    removeBG();
    {
        s32 i;
        for (i = 0; i < OPTIONS_BUTTONS; i++) {
            if (options_buttonInfo[i].handle != -1) {
                RemoveObjectTree(options_buttonInfo[i].handle, 0, 0);
                options_buttonInfo[i].handle = -1;
            }
        }
        options_buttonsInited = 0;
    }
    options_inited = 0;
}

/* Lay the 3D buttons out for this frame (front end only, once they exist). The cursor spins beside the selected
 * row and eases towards it (its speed follows the distance, 180 to 540 a second). Each row's two buttons turn to
 * show their selected face when the row is selected (a half turn at 4 pi a second), scroll so the selection stays in
 * view (at most 540 a second), fade out at the top and bottom of the screen, and hide when the row isn't offered
 * (options_validOption). */
void options_updateButtons(void) {
    static f32 cursorSpeed;
    static f32 scrollSpeed;
    s32 i;
    s32 j;
    s32 n;
    f32 y;
    f32 old[3];
    f32 sel_y;
    s8 settled;
    struct tTexture *texture;
    s32 yi;
    s16 tex;
    Color color;

    settled = 1;
    color.rgba = options_buttonColor.rgba;
    if (options_buttonsInited == 0)
        return;
    n = 0;
    for (j = 0; j < options_gSelectedOption; j++) {
        if (options_validOption[j] != 0)
            n++;
    }
    memcpy(old, options_buttonInfo[OPTIONS_CURSOR].pos, 12);
    options_buttonInfo[OPTIONS_CURSOR].ang += 6.2831855f * gFrameSeconds;
    if (6.2831855f < options_buttonInfo[OPTIONS_CURSOR].ang)
        options_buttonInfo[OPTIONS_CURSOR].ang -= 6.2831855f;
    memcpy(options_buttonInfo[OPTIONS_CURSOR].mat, unitmatrix, 36);
    sel_y = 75.0f - n * 165.0f / (gMenuCount - 1);
    options_buttonInfo[OPTIONS_CURSOR].pos[0] = -160.0f;
    options_buttonInfo[OPTIONS_CURSOR].pos[2] = 225.0f;
    options_buttonInfo[OPTIONS_CURSOR].pos[1] = sel_y;
    if (options_buttonsJustInited == 0) {
        y = options_buttonInfo[OPTIONS_CURSOR].pos[1];
        if (cursorSpeed < fabsf(y - old[1]) * 60.0f / 10.0f) {
            cursorSpeed = fabsf(y - old[1]) * 60.0f / 10.0f;
            if (!(cursorSpeed < 540.0f))
                cursorSpeed = 540.0f;
            if (!(180.0f < cursorSpeed))
                cursorSpeed = 180.0f;
        }
        if (old[1] + cursorSpeed * gFrameSeconds < y)
            options_buttonInfo[OPTIONS_CURSOR].pos[1] = old[1] + cursorSpeed * gFrameSeconds;
        else if (y < old[1] - cursorSpeed * gFrameSeconds)
            options_buttonInfo[OPTIONS_CURSOR].pos[1] = old[1] - cursorSpeed * gFrameSeconds;
        else
            cursorSpeed = 180.0f;
    }
    WRollUV(options_buttonInfo[OPTIONS_CURSOR].ang, options_buttonInfo[OPTIONS_CURSOR].mat);
    WPitchUV(1.5707964f, options_buttonInfo[OPTIONS_CURSOR].mat);
    ScaleUV(options_buttonInfo[OPTIONS_CURSOR].mat, options_buttonInfo[OPTIONS_CURSOR].mat, 0.4f);
    yi = sel_y * 1.33f + n * 40;
    for (i = 0; i < OPTIONS_ROWS; i++) {
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
                settled = 0;
                options_buttonInfo[i].pos[1] = old[1] + scrollSpeed * gFrameSeconds;
            } else if (options_buttonInfo[i].pos[1] < old[1] - scrollSpeed * gFrameSeconds) {
                settled = 0;
                options_buttonInfo[i].pos[1] = old[1] - scrollSpeed * gFrameSeconds;
            }
        }
        options_buttonInfo[i + 14].pos[0] = 150.0f;
        options_buttonInfo[i + 14].pos[1] = options_buttonInfo[i].pos[1] * 31.0f / 30.0f;
        options_buttonInfo[i + 14].pos[2] = 310.0f;
        WPitchUV(options_buttonInfo[i].ang - 1.5707964f, options_buttonInfo[i].mat);
        WPitchUV(options_buttonInfo[i + 14].ang - 1.5707964f, options_buttonInfo[i + 14].mat);
        if (1.5707964f < options_buttonInfo[i].ang)
            texture = MBOX_FindTexture_WORK("BUTTON_SELECT", &tex, 0, (s8)(gNumModels[0] - 1), 1);
        else
            texture = MBOX_FindTexture_WORK("BUTTON", &tex, 0, (s8)(gNumModels[0] - 1), 1);
        ZOID_SetTexture(options_buttonInfo[i].handle, texture, -1);
        ZOID_SetTexture(options_buttonInfo[i + 14].handle, texture, -1);
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
    if (settled == 1)
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

/* The screen's frame. In the front end everyone's buttons count, in the pause menu only the pausing player's.
 * Start or B leaves; up and down (auto-repeating) move between the offered rows, the list scrolling to keep the
 * selection in its middle; left or right changes the row: the language (one of six, which reloads the texts) or
 * flips the row's game option. In the pause menu a change applies at once (SetOptions), and the title changes when
 * the pausing player's controller is pulled or put back. Leaving (or a soft reset) goes back to the start menu
 * (the title on a reset) or to the pause menu's main page. */
void updateOptionScreen(void) {
    Pad16 *p;
    s32 btn;
    s32 rep;
    s32 dir;
    s32 x;

    if (gGameState & GS_FRONTEND_MASK) {
        btn = gActiveEdges;
        rep = gActiveAutoEdges;
    } else {
        btn = gContEdges[playerInPause];
        rep = gAutoEdges[playerInPause];
    }
    if (options_inited == 0)
        options_initScreen();
    if (btn & (BUTTON_START | BUTTON_B)) {
        PlayStartABSound(btn);
        exitOptionScreen = 2;
    } else if (rep & BUTTON_UP) {
        PlayUDSound(rep);
        do {
            options_gSelectedOption--;
            if (options_gSelectedOption < 0)
                options_gSelectedOption = OPTIONS_ROWS - 1;
        } while (!options_validOption[options_gSelectedOption]);
    } else if (rep & BUTTON_DOWN) {
        PlayUDSound(rep);
        do {
            options_gSelectedOption++;
            if (options_gSelectedOption >= OPTIONS_ROWS)
                options_gSelectedOption = 0;
        } while (!options_validOption[options_gSelectedOption]);
    }
    x = options_gSelectedOption - gOptionMenuSize / 2;
    if (options_gFirstOption < x)
        options_gFirstOption = x;
    if (x < options_gFirstOption)
        options_gFirstOption = x;
    if (OPTIONS_ROWS - gOptionMenuSize < options_gFirstOption)
        options_gFirstOption = OPTIONS_ROWS - gOptionMenuSize;
    if (options_gFirstOption < 0)
        options_gFirstOption = 0;
    if (btn & (BUTTON_LEFT | BUTTON_RIGHT)) {
        dir = (btn & BUTTON_LEFT) ? -1 : 1;
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
        case OPTION_ROW_LANGUAGE:
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
        if (!(gGameState & GS_FRONTEND_MASK))
            SetOptions();
    }
    options_updateButtons();
    if (gGameState & GS_RACE_MASK) {
        p = &gControllerData[playerInPause];
        if (!p->b0 != options_gControllerRemoved) {
            options_gControllerRemoved = !p->b0;
            options_TitlePane();
        }
    }
    if (gSoftReset) {
        options_uninitScreen();
        if (gGameState & GS_FRONTEND_MASK)
            gNewGameState = GS_TITLE;
        else
            setPauseState(kPauseMainScreen);
    } else if (exitOptionScreen) {
        options_uninitScreen();
        if (gGameState & GS_FRONTEND_MASK)
            gNewGameState = GS_START;
        else
            setPauseState(kPauseMainScreen);
    }
}
