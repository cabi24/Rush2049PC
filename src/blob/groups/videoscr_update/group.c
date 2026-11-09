/* The video options screen: its buttons and its update (MAT3 videoscr.c). */
#include "game/game.h"

typedef struct {
    s32 type;            /* 0x00 */
    s32 handle;          /* 0x04 */
    f32 ang;             /* 0x08 */
    f32 mat[3][3];       /* 0x0C */
    f32 pos[3];          /* 0x30 */
    u8 pad3C[4];
} Button; /* 0x40 */

VIEW(Button[], video_buttonInfo);
#define video_buttonInfo video_buttonInfo_view

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view
f32 fabsf(f32);
#pragma intrinsic(fabsf)
void *memcpy(void *, const void *, u32);

VIEW(intptr_t, video_blit);
#define video_blit video_blit_view

/* Removes the video screen's panes (the permanent ones stay). */
void video_uninitButtons(void) {
    RemovePanes(0);
}

/* Leaves the video screen: the paks' files written (SaveFlush), its 3D buttons, panes, blit and background removed. */
void video_uninitScreen(void) {
    s32 i;

    if (video_inited != 0) {
        SaveFlush(1);
        for (i = 0; i < 12; i++) {
            if (video_buttonInfo[i].handle != -1) {
                RemoveObjectTree(video_buttonInfo[i].handle, 0, 0);
                video_buttonInfo[i].handle = -1;
            }
        }
        video_buttonsInited = 0;
        video_uninitButtons();
        video_gTitlePane = 0;
        if (video_blit != 0) {
            RemoveBlit((tBlit *) video_blit);
            video_blit = 0;
        }
        removeBG();
        video_panesCreated = 0;
        video_inited = 0;
    }
}

/* Whether video option i can be chosen. */
s32 video_validOption(s32 i) {
    s32 v;
    v = video_optionValid[i];
    return v;
}

/* Moves and turns the video screen's 3D buttons towards the chosen option. */
void video_updateButtons(void) {
    s32 i;
    struct tTexture *texture;
    f32 y;
    f32 cur;
    f32 old[3];
    f32 d;
    f32 sel_y;
    s32 vis;
    s16 tex;

    if (video_buttonsInited == 0)
        return;
    y = 100.0f;
    for (i = 0; i < 4; i++) {
        if (i == gVideoMenuSelect) {
            sel_y = y;
            if (video_buttonInfo[i].ang < 3.1415927f)
                video_buttonInfo[i].ang += 12.566371f * gFrameSeconds;
            if (3.1415927f < video_buttonInfo[i].ang || video_buttonsJustInited == 1)
                video_buttonInfo[i].ang = 3.1415927f;
        } else {
            if (0.0f < video_buttonInfo[i].ang)
                video_buttonInfo[i].ang -= 12.566371f * gFrameSeconds;
            if (video_buttonInfo[i].ang < 0.0f || video_buttonsJustInited == 1)
                video_buttonInfo[i].ang = 0.0f;
        }
        memcpy(video_buttonInfo[i].mat, unitmatrix, 36);
        memcpy(old, video_buttonInfo[i].pos, 12);
        video_buttonInfo[i].pos[0] = -90.0f;
        video_buttonInfo[i].pos[1] = y;
        video_buttonInfo[i].pos[2] = 300.0f;
        if (video_buttonsJustInited == 0) {
            if (old[1] + 180.0f * gFrameSeconds < video_buttonInfo[i].pos[1])
                video_buttonInfo[i].pos[1] = old[1] + 180.0f * gFrameSeconds;
            if (video_buttonInfo[i].pos[1] < old[1] - 180.0f * gFrameSeconds)
                video_buttonInfo[i].pos[1] = old[1] - 180.0f * gFrameSeconds;
        }
        video_buttonInfo[i + 4].ang = video_buttonInfo[i].ang;
        video_buttonInfo[i + 4].pos[0] = 150.0f;
        video_buttonInfo[i + 4].pos[1] = video_buttonInfo[i].pos[1];
        video_buttonInfo[i + 4].pos[2] = video_buttonInfo[i].pos[2];
        WPitchUV(video_buttonInfo[i].ang - 1.5707964f, video_buttonInfo[i].mat);
        memcpy(video_buttonInfo[i + 4].mat, video_buttonInfo[i].mat, 36);
        if (1.5707964f < video_buttonInfo[i].ang)
            texture = MBOX_FindTexture_WORK("BUTTON_SELECT", &tex, 0, (s8)(gNumModels[0] - 1), 1);
        else
            texture = MBOX_FindTexture_WORK("BUTTON", &tex, 0, (s8)(gNumModels[0] - 1), 1);
        ZOID_SetTexture(video_buttonInfo[i].handle, texture, -1);
        ZOID_SetTexture(video_buttonInfo[i + 4].handle, texture, -1);
        if ((vis = video_validOption(i)) == 0) {
            MBOX_HideObject(video_buttonInfo[i].handle, 1, 15);
            MBOX_HideObject(video_buttonInfo[i + 4].handle, 1, 15);
        } else {
            MBOX_ShowObject(video_buttonInfo[i].handle, 0, 15);
            if (i < 3)
                MBOX_ShowObject(video_buttonInfo[i + 4].handle, 0, 15);
            else
                MBOX_HideObject(video_buttonInfo[i + 4].handle, 1, 15);
            y -= 40.0f;
        }
    }
    memcpy(old, video_buttonInfo[8].pos, 12);
    video_buttonInfo[8].ang += 6.2831855f * gFrameSeconds;
    if (6.2831855f < video_buttonInfo[8].ang)
        video_buttonInfo[8].ang -= 6.2831855f;
    memcpy(video_buttonInfo[8].mat, unitmatrix, 36);
    video_buttonInfo[8].pos[0] = -160.0f;
    video_buttonInfo[8].pos[2] = 225.0f;
    video_buttonInfo[8].pos[1] = sel_y * 225.0f / 300.0f;
    if (video_buttonsJustInited == 0) {
        y = video_buttonInfo[8].pos[1];
        if (video_buttonSlideSpeed < fabsf(y - old[1]) * 60.0f / 10.0f) {
            video_buttonSlideSpeed = fabsf(y - old[1]) * 60.0f / 10.0f;
            video_buttonSlideSpeed = (video_buttonSlideSpeed < 540.0f * video_buttonSlideScale) ? video_buttonSlideSpeed : 540.0f * video_buttonSlideScale;
            video_buttonSlideSpeed = (180.0f * video_buttonSlideScale < video_buttonSlideSpeed) ? video_buttonSlideSpeed : 180.0f * video_buttonSlideScale;
        }
        if (old[1] + video_buttonSlideSpeed * gFrameSeconds < y)
            video_buttonInfo[8].pos[1] = old[1] + video_buttonSlideSpeed * gFrameSeconds;
        else if (y < old[1] - video_buttonSlideSpeed * gFrameSeconds)
            video_buttonInfo[8].pos[1] = old[1] - video_buttonSlideSpeed * gFrameSeconds;
        else
            video_buttonSlideSpeed = 180.0f * video_buttonSlideScale;
    }
    WRollUV(video_buttonInfo[8].ang, video_buttonInfo[8].mat);
    WPitchUV(1.5707964f, video_buttonInfo[8].mat);
    ScaleUV(video_buttonInfo[8].mat, video_buttonInfo[8].mat, 0.4f);
    video_buttonsJustInited = 0;
}

VIEW(u8[], gOptions);
#define gOptions gOptions_view
typedef struct { volatile s8 b0; u8 pad[15]; } Pad16;
VIEW(Pad16[], gControllerData);
#define gControllerData gControllerData_view

/* The video screen's frame: reads the controls (choose an option, change its value: the screen's offset, the
 * texture filter, the dither ...), applies the changes and leaves on a back or a confirm. */
void updateVideoScreen(void) {
    Pad16 *p;
    s32 v;
    s32 c;
    s32 btn;
    s32 rep;
    s32 hold;
    s32 pad;
    s32 dz;
    s32 ds;

    if (gGameState & GS_FRONTEND_MASK) {
        btn = gActiveEdges;
        rep = gActiveAutoEdges;
        hold = gActiveLevels;
    } else {
        btn = gContEdges[playerInPause];
        rep = gAutoEdges[playerInPause];
        hold = gContLevels[playerInPause];
    }
    if (video_inited == 0)
        video_initScreen();
    if (gTestFrequency == 1) {
        if (CountdownTimeOut()) {
            gTestFrequency = 0;
            gConfirmFrequency = 1;
            gYesFrequency = 0;
        }
    } else if (gConfirmFrequency == 1) {
        if (btn & 7) {
            PlayStartABSound(btn);
            if (btn & 4)
                gYesFrequency = 0;
            gConfirmFrequency = 0;
        } else if (btn & 0x3000) {
            PlayLRSound(btn);
            gYesFrequency = !gYesFrequency;
        }
    } else if (btn & 5) {
        PlayStartABSound(btn);
        exitVideoScreen = 2;
    } else if (hold & 0x3000) {
        if (rep & 0x3000) {
            dz = (rep & 0x1000) ? -1 : 1;
            PlayLRSound(btn);
            switch (gVideoMenuSelect) {
            case 0:
                gVertCenter += dz;
                if (gVertCenter < -32)
                    gVertCenter = -32;
                else if (gVertCenter > 32)
                    gVertCenter = 32;
                gOptions[20] = gVertCenter;
                video_set_offset(gHorzCenter, gVertCenter);
                break;
            case 1:
                gHorzCenter += dz;
                if (gHorzCenter < -32)
                    gHorzCenter = -32;
                else if (gHorzCenter > 32)
                    gHorzCenter = 32;
                gOptions[19] = gHorzCenter;
                video_set_offset(gHorzCenter, gVertCenter);
                break;
            }
        }
    } else if (btn & 0xC00) {
        ds = (btn & 0x400) ? -1 : 1;
        PlayUDSound(btn);
        do {
            gVideoMenuSelect += ds;
            if (gVideoMenuSelect > 3)
                gVideoMenuSelect = 0;
            else if (gVideoMenuSelect < 0)
                gVideoMenuSelect = 3;
        } while (!video_validOption(gVideoMenuSelect));
    } else if (btn & 2) {
        if (gVideoMenuSelect == 3) {
            gVertCenter = 0;
            gHorzCenter = 0;
            gOptions[19] = gHorzCenter;
            gOptions[20] = gVertCenter;
            video_resetChoice = 0;
            video_dither_filter = 0;
            video_set_offset(gHorzCenter, gVertCenter);
        }
    }
    video_updateButtons();
    if (gGameState & GS_RACE_MASK) {
        p = &gControllerData[playerInPause];
        if (!p->b0 != video_gControllerRemoved) {
            video_gControllerRemoved = !p->b0;
            video_TitlePane();
        }
    }
    if (gSoftReset) {
        video_uninitScreen();
        if (gGameState & GS_FRONTEND_MASK)
            gNewGameState = GS_TITLE;
        else
            setPauseState(1);
    } else if (exitVideoScreen) {
        video_uninitScreen();
        if (gGameState & GS_FRONTEND_MASK)
            gNewGameState = GS_START;
        else
            setPauseState(1);
    }
}
