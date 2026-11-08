/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* group updateVideoScreen: the pause-menu file tail, five bodies, all EQUAL in the whole-program unit
 * with their real callers (no stand-ins, no implicit int, no false prototype):
 *   video_uninitButtons  (2 w)   deleted static-like helper: RemovePanes(0); inlined into video_uninitScreen.
 *                          Its retail remains are the caller-less stub at 0x800B55F4 (locked stub).
 *   video_uninitScreen  (49 w)  menu teardown: release the 12 button models, clear the button count, sounds.
 *   video_validOption  (2 w)   getter video_optionValid[i] (entry visible); inlined into video_updateButtons and
 *                          updateVideoScreen. Its retail remains are the caller-less stub at 0x800B59E8.
 * video_updateButtons (343 w) pause-menu 3D buttons update (semantics).
 *   updateVideoScreen    (313 w) pause-menu input handler, reached through a pointer table (kept).
 * updateVideoScreen: btn/rep/hold = pressed/repeat/held pad words (gActiveEdges/gActiveAutoEdges/gActiveLevels when
 * gGameState & 0x7C03FFFE, else per-controller gContEdges/gAutoEdges/gContLevels[playerInPause]).
 *
 * Disclosed shaping (owner review):
 * 1. Unused locals that are the exact frame residual: updateVideoScreen `v`, `c`, `pad` (each removal ->
 *    frame 144, 10-14 words); video_updateButtons `cur`, `d` (removal -> frame 104). video_updateButtons's
 *    declaration order (i, name, y first) is set by its homes (old 84, tex 70, sel_y 76).
 * 2. Helper identities: video_uninitButtons (one call, RemovePanes(0)) and video_validOption (getter) are
 *    hypotheses supported by (a) their caller-less 2-word stubs sitting exactly where a definition in this
 *    source order leaves them, and (b) traced compiler evidence: the inlined helper body carries the
 *    helper's (earlier) line numbers, which is the only listing order that gives retail's
 *    `sw zero,video_buttonsInited` in the jal delay slot (asm.sh oracle), and the getter's inlined parameter/return
 * temps are the "invisible holders" of v0/a0 in updateVideoScreen's selection loop and of v0
 * video_updateButtons (vis -> v1, implicit-int residual).
 * 3. updateVideoScreen's reset branch reads back the just-zeroed globals (gOptions[19] = gHorzCenter ...,
 * video_set_offset(gHorzCenter, gVertCenter)) (w_e); `volatile` on the controller-present byte.
 * 4. Old-style declarations with the right return type: MBOX_FindTexture_WORK(), MBOX_HideObject(),
 * MBOX_ShowObject (not false prototypes).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct {
    s32 type;            /* 0x00 */
    s32 handle;          /* 0x04 */
    f32 ang;             /* 0x08 */
    f32 mat[3][3];       /* 0x0C */
    f32 pos[3];          /* 0x30 */
    u8 pad3C[4];
} Button; /* 0x40 */

extern s32 video_buttonsInited;
extern s32 video_buttonsJustInited;
extern f32 video_buttonSlideScale;
extern Button video_buttonInfo[];
extern s8 video_optionValid[];
extern s32 gVideoMenuSelect;
extern f32 unitmatrix[];
extern volatile f32 gFrameSeconds;
extern volatile u8 gNumModels;
extern f32 video_buttonSlideSpeed;

f32 fabsf(f32);
#pragma intrinsic(fabsf)
void *memcpy(void *, const void *, u32);
intptr_t MBOX_FindTexture_WORK();
void WPitchUV(f32 angle, f32 uv[][3]);
void WRollUV(f32 angle, f32 uv[][3]);
void ScaleUV(f32 (*dst)[3], f32 (*src)[3], f32 s);
void MBOX_HideObject();
void MBOX_ShowObject();
void ZOID_SetTexture(s16 arg0, intptr_t arg1, s32 arg2);

extern s8 video_inited;
extern intptr_t video_blit;
extern s8 video_panesCreated;
extern intptr_t video_gTitlePane;
void SaveFlush(s32 arg0);
void RemovePanes(s32 arg0);
void RemoveBlit(intptr_t sound_id);
void removeBG(void);
void RemoveObjectTree(s16 arg0, s32 arg1, s32 arg2);

void video_uninitButtons(void) {
    RemovePanes(0);
}

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
            RemoveBlit(video_blit);
            video_blit = 0;
        }
        removeBG();
        video_panesCreated = 0;
        video_inited = 0;
    }
}

s32 video_validOption(s32 i) {
    s32 v;
    v = video_optionValid[i];
    return v;
}

void video_updateButtons(void) {
    s32 i;
    intptr_t name;
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
            name = MBOX_FindTexture_WORK("BUTTON_SELECT", &tex, 0, (s8)(gNumModels - 1), 1);
        else
            name = MBOX_FindTexture_WORK("BUTTON", &tex, 0, (s8)(gNumModels - 1), 1);
        ZOID_SetTexture(video_buttonInfo[i].handle, name, -1);
        ZOID_SetTexture(video_buttonInfo[i + 4].handle, name, -1);
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


extern s32 gGameState;
extern s32 gNewGameState;
extern s32 gActiveEdges;
extern s32 gActiveAutoEdges;
extern s32 gActiveLevels;
extern s32 playerInPause;
extern s32 gContEdges[];
extern s32 gAutoEdges[];
extern s32 gContLevels[];
extern s8 video_inited;
extern s8 gTestFrequency;
extern s8 gConfirmFrequency;
extern s8 gYesFrequency;
extern s32 exitVideoScreen;
extern s32 video_resetChoice;
extern s32 gVertCenter;
extern s32 gHorzCenter;
extern s8 video_gControllerRemoved;
extern s8 gSoftReset;
extern s32 video_dither_filter;
extern u8 gOptions[];
typedef struct { volatile s8 b0; u8 pad[15]; } Pad16;
extern Pad16 gControllerData[];

s32 CountdownTimeOut(void);
void video_set_offset(s32 x, s32 y);
void video_initScreen(void);
void PlayStartABSound(s32 a);
void PlayLRSound(s32 a);
void PlayUDSound(s32 a);
void video_TitlePane(void);
void video_uninitScreen(void);
void setPauseState(s32 a);

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

    if (gGameState & 0x7C03FFFE) {
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
    if (gGameState & 0x7C0000) {
        p = &gControllerData[playerInPause];
        if (!p->b0 != video_gControllerRemoved) {
            video_gControllerRemoved = !p->b0;
            video_TitlePane();
        }
    }
    if (gSoftReset) {
        video_uninitScreen();
        if (gGameState & 0x7C03FFFE)
            gNewGameState = 4;
        else
            setPauseState(1);
    } else if (exitVideoScreen) {
        video_uninitScreen();
        if (gGameState & 0x7C03FFFE)
            gNewGameState = 16;
        else
            setPauseState(1);
    }
}
