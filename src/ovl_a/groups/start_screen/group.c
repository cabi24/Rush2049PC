#include "game/game.h"

typedef struct Blit Blit;
typedef struct ButtonInfo { s32 buttonType; s32 objIdx; f32 angle; f32 uvs[4][3]; } ButtonInfo;
typedef union tRGBA { u8 c[4]; u32 w; } tRGBA;
typedef struct MultiBlit MultiBlit;

/* gcc rejects arrays of an incomplete type, which IDO accepts. Layouts as in src/blob/NewMultiBlit.c (MultiBlit)
   and src/ovl_a/groups/cheat_screen/eggs.c (Egg). */
struct MultiBlit { const char *texname; s16 dulx, duly, width, height, top, bot, left, right; u32 zdepth, alpha;
                   s32 (*animfunc)(Blit *); u32 animid; };

typedef struct Controller { s8 attached; u8 rest[15]; } Controller;

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view
VIEW(f32[3][3], unitmatrix);
#define unitmatrix unitmatrix_view
VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view
VIEW(s8[], gOptions);
#define gOptions gOptions_view

VIEW(u32, gActiveEdges);
#define gActiveEdges gActiveEdges_view

VIEW(Controller[4], gControllerData);
#define gControllerData gControllerData_view

VIEW(ButtonInfo[18], startscr_buttonInfo);
#define startscr_buttonInfo startscr_buttonInfo_view

VIEW(MultiBlit[], startscr_mblit);
#define startscr_mblit startscr_mblit_view

VIEW(Egg *, gEgg);
#define gEgg gEgg_view

extern f32 fabsf(f32);
#pragma intrinsic (fabsf)
extern void *memcpy(void *, const void *, u32);
extern void particle_lifetime_set(void);      /* createBG */

extern Blit *NewMultiBlit(s16, s16, MultiBlit *, s16);

extern void RemoveBlit(Blit *);

extern void *MBOX_FindTexture_WORK(char *, u16 *, s8, s8, s32);
extern void ZOID_SetTexture(s16, void *, s32);

/* startscr_InitEgg is this file's InitEgg (inlined on the N64): it points at the main menu's code, start_screen_egg;
   eggs.c's InitEgg at cheat `egg` of gEggs. Under one name the PC linked eggs.c's for both, so InitEgg(23)
   pointed past the table and the cheat screen could never be opened (docs/port/QA.md). */
/* PC: game.pc_setup (port/config/start_menu.c, docs/port/FRONTEND.md "PC SETUP"): option slot 8, never valid on the
   N64, is PC SETUP while it is on, A on it opens the settings overlay over the menu; game.cheats_menu: slot 6 valid. */
int pc_start_valid(int index, int value);
int pc_start_exit(int exit, int select);
#define PC_START_VALID(i, v) pc_start_valid((i), (v))
#define PC_START_EXIT(e) ((e) = pc_start_exit((e), gStartMenuSelect))

void startscr_InitEgg(s32 egg)
{
    gEgg = &start_screen_egg[egg - 23];
    gNextTimeOut = 0;
    gEggIdx = 0;
}

void updateStartScreen(void)
{
    s32 ctrlIdx;
    if (!gSoftReset)
        gGameStarted = 1;
    if (gSkipToGameState != 1) {
        if (gSkipToGameState != gGameState) {
            if (!(gSkipToGameState & 0x7c0000) && gOptions[14] <= 12) {
                AudSetMusicVol(gOptions[12] / 10.0f, 0.05f);
                AudPlaySong(6, 1);
            }
            setGameState(32);
            return;
        }
        gSkipToGameState = 1;
    }
    if (!startscr_inited)
        startscr_initScreen();
    while (!startscr_optionValid(gStartMenuSelect)) {
        if (++gStartMenuSelect >= 10)
            gStartMenuSelect = 0;
    }
    gMaxPlayers = 0;
    for (ctrlIdx = 0; ctrlIdx < 4; ctrlIdx++) {
        if (gControllerData[ctrlIdx].attached != 1)
            break;
        gMaxPlayers++;
    }
    if (gNumPlayers > gMaxPlayers) {
        if (gMaxPlayers < 1)
            gNumPlayers = 1;
        else
            gNumPlayers = gMaxPlayers;
    }
    if (gActiveEdges & 1) {
        PlayStartABSound(gActiveEdges);
        gStartMenuSelect = 0;
        exitStartScreen = 1;
    } else if (gActiveEdges & 2) {
        PlayStartABSound(gActiveEdges);
        exitStartScreen = 1;
    } else if (gActiveEdges & 4) {
        PlayStartABSound(gActiveEdges);
        exitStartScreen = 2;
    } else if (gActiveEdges & 0x1000) {
        PlayLRSound(gActiveEdges);
        if (gStartMenuSelect == 0) {
            if (--gNumPlayers < 1)
                gNumPlayers = gMaxPlayers;
        }
    } else if (gActiveEdges & 0x2000) {
        PlayLRSound(gActiveEdges);
        if (gStartMenuSelect == 0) {
            if (++gNumPlayers > gMaxPlayers)
                gNumPlayers = 1;
        }
    } else if (gActiveEdges & 0x400) {
        PlayUDSound(gActiveEdges);
        do {
            if (--gStartMenuSelect < 0)
                gStartMenuSelect = 9;
        } while (!startscr_optionValid(gStartMenuSelect));
    } else if (gActiveEdges & 0x800) {
        PlayUDSound(gActiveEdges);
        do {
            if (++gStartMenuSelect >= 10)
                gStartMenuSelect = 0;
        } while (!startscr_optionValid(gStartMenuSelect));
    }
    startscr_updateButtons();
    if (gSoftReset) {
        startscr_uninitScreen();
        setGameState(4);
    } else if (PC_START_EXIT(exitStartScreen)) {
        startscr_uninitScreen();
        switch (exitStartScreen) {
            case 1:
                switch (gStartMenuSelect) {
                    case 0: setGameState(32); break;
                    case 1: setGameState(0x2000); break;
                    case 2: setGameState(0x200); break;
                    case 3: setGameState(0x800); break;
                    case 4: setGameState(0x1000); break;
                    case 6: setGameState(0x8000000); break;
                    case 7: setGameState(0x10000000); break;
                    case 9:
                        /* MAT3 then calls ExitSystem: the N64's is an empty function (a console has nothing to
                         * exit to), so the PC calls nothing. */
                        AudKillSong();
                        AudStopAllEffects();
                        break;
                }
                break;
            case 2:
                setGameState(4);
                break;
        }
        exitStartScreen = 0;
    }
    if (!gCheatActive[21])
        gCheatActive[21] = CheckForEgg();
}

void startscr_initScreen(void)
{
    if (gOptions[14] <= 12) {
        AudSetMusicVol(gOptions[12] / 10.0f, 0.05f);
        AudPlaySong(6, 1);
    }
    particle_lifetime_set();
    startscr_initButtons();
    startscr_blit = NewMultiBlit(0, 0, startscr_mblit, 1);
    exitStartScreen = 0;
    ClearRushControls();
    if (!gCheatActive[21])
        startscr_InitEgg(23);
    startscr_inited = 1;
}

void startscr_uninitScreen(void) /* uninitScreen (non-static, named after its retail stub) */
{
    if (startscr_blit) {
        RemoveBlit(startscr_blit);
        startscr_blit = 0;
    }
    removeBG();
    startscr_uninitButtons();
    startscr_inited = 0;
}

void startscr_initButtons(void) /* initButtons */
{
    tRGBA buttonColor = {255, 255, 255, 255};
    void *tex;
    u16 texIdx;
    s32 buttonIdx;
    for (buttonIdx = 0; buttonIdx < 18; buttonIdx++)
        if (startscr_buttonInfo[buttonIdx].objIdx == -1) {
            memcpy(startscr_buttonInfo[buttonIdx].uvs, unitmatrix, 48);
            startscr_buttonInfo[buttonIdx].objIdx = MBOX_NewObject(
                MBOX_FindObject_WORK(startscr_buttonNames[startscr_buttonInfo[buttonIdx].buttonType], 0, gNumModels - 1, 1),
                startscr_buttonInfo[buttonIdx].uvs, -1,
                (startscr_buttonInfo[buttonIdx].buttonType == 0 || startscr_buttonInfo[buttonIdx].buttonType == 1) ? 0x42000 : 0);
            ZOID_SetColor(startscr_buttonInfo[buttonIdx].objIdx, &buttonColor);
        }
    tex = MBOX_FindTexture_WORK("BUTTON_TITLE", &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(startscr_buttonInfo[11].objIdx, tex, -1);
    memcpy(startscr_buttonInfo[11].uvs, unitmatrix, 36);
    WPitchUV(-1.5707964f, startscr_buttonInfo[11].uvs);
    startscr_buttonInfo[11].uvs[3][0] = 0.0f;
    startscr_buttonInfo[11].uvs[3][1] = 115.0f;
    startscr_buttonInfo[11].uvs[3][2] = 200.0f;
    tex = MBOX_FindTexture_WORK("A1", &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(startscr_buttonInfo[12].objIdx, tex, -1);
    memcpy(startscr_buttonInfo[12].uvs, unitmatrix, 36);
    WPitchUV(-1.5707964f, startscr_buttonInfo[12].uvs);
    startscr_buttonInfo[12].uvs[3][0] = 75.1f;
    startscr_buttonInfo[12].uvs[3][1] = 55.2f;
    startscr_buttonInfo[12].uvs[3][2] = 100.0f;
    ScaleUV(startscr_buttonInfo[12].uvs[0], startscr_buttonInfo[12].uvs[0], 0.5f);
    memcpy(startscr_buttonInfo[13].uvs, unitmatrix, 36);
    WPitchUV(-1.5707964f, startscr_buttonInfo[13].uvs);
    WRollUV(3.1415927f, startscr_buttonInfo[13].uvs);
    startscr_buttonInfo[13].uvs[3][0] = 87.1f;
    startscr_buttonInfo[13].uvs[3][1] = 55.2f;
    startscr_buttonInfo[13].uvs[3][2] = 100.0f;
    tex = MBOX_FindTexture_WORK("B1", &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(startscr_buttonInfo[14].objIdx, tex, -1);
    memcpy(startscr_buttonInfo[14].uvs, unitmatrix, 36);
    WPitchUV(-1.5707964f, startscr_buttonInfo[14].uvs);
    startscr_buttonInfo[14].uvs[3][0] = -75.1f;
    startscr_buttonInfo[14].uvs[3][1] = 55.20f;
    startscr_buttonInfo[14].uvs[3][2] = 100.0f;
    ScaleUV(startscr_buttonInfo[14].uvs[0], startscr_buttonInfo[14].uvs[0], 0.5f);
    memcpy(startscr_buttonInfo[15].uvs, unitmatrix, 36);
    WPitchUV(-1.5707964f, startscr_buttonInfo[15].uvs);
    startscr_buttonInfo[15].uvs[3][0] = -87.1f;
    startscr_buttonInfo[15].uvs[3][1] = 55.20f;
    startscr_buttonInfo[15].uvs[3][2] = 100.0f;
    tex = MBOX_FindTexture_WORK("CURSOR", &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(startscr_buttonInfo[16].objIdx, tex, -1);
    startscr_buttonsInited = 1;
    startscr_buttonsJustInited = 1;
    ZeroCameras();
}

void startscr_uninitButtons(void) /* uninitButtons (non-static, named after its retail stub) */
{
    s32 buttonIdx;
    for (buttonIdx = 0; buttonIdx < 18; buttonIdx++)
        if (startscr_buttonInfo[buttonIdx].objIdx != -1) {
            ZOID_RemoveObject(startscr_buttonInfo[buttonIdx].objIdx);
            startscr_buttonInfo[buttonIdx].objIdx = -1;
        }
    startscr_buttonsInited = 0;
}

void startscr_updateButtons(void)
{
    f32 unused0, unused1, unused2;
    s32 buttonIdx;
    f32 prevPos[3];
    void *tex;
    u16 texIdx;
    f32 buttonY, selectedY;
    if (!startscr_buttonsInited)
        return;
    buttonY = startscr_maxY;
    selectedY = startscr_maxY;
    for (buttonIdx = 0; buttonIdx < 10; buttonIdx++) {
        if (!startscr_optionValid(buttonIdx)) {
            MBOX_HideObject(startscr_buttonInfo[buttonIdx].objIdx, 0, 15);
            continue;
        }
        MBOX_ShowObject(startscr_buttonInfo[buttonIdx].objIdx, 0, 15);
        if (buttonIdx == gStartMenuSelect) {
            if (startscr_buttonInfo[buttonIdx].angle < 3.1415927f)
                startscr_buttonInfo[buttonIdx].angle += 12.566371f * gFrameSeconds;
            if (startscr_buttonInfo[buttonIdx].angle > 3.1415927f || startscr_buttonsJustInited == 1)
                startscr_buttonInfo[buttonIdx].angle = 3.1415927f;
            selectedY = buttonY;
        } else {
            if (startscr_buttonInfo[buttonIdx].angle > 0.0f)
                startscr_buttonInfo[buttonIdx].angle -= 12.566371f * gFrameSeconds;
            if (startscr_buttonInfo[buttonIdx].angle < 0.0f || startscr_buttonsJustInited == 1)
                startscr_buttonInfo[buttonIdx].angle = 0.0f;
        }
        startscr_buttonInfo[buttonIdx].uvs[3][0] = startscr_buttonX;
        startscr_buttonInfo[buttonIdx].uvs[3][1] = buttonY;
        startscr_buttonInfo[buttonIdx].uvs[3][2] = startscr_buttonZ;
        ScaleUV(unitmatrix[0], startscr_buttonInfo[buttonIdx].uvs[0], startscr_gButtonScale);
        WPitchUV(startscr_buttonInfo[buttonIdx].angle - 1.5707964f, startscr_buttonInfo[buttonIdx].uvs[0]);
        if (startscr_buttonInfo[buttonIdx].angle > 1.5707964f)
            tex = MBOX_FindTexture_WORK("BUTTON_SELECT", &texIdx, 0, gNumModels - 1, 1);
        else
            tex = MBOX_FindTexture_WORK("BUTTON", &texIdx, 0, gNumModels - 1, 1);
        ZOID_SetTexture(startscr_buttonInfo[buttonIdx].objIdx, tex, -1);
        if (buttonIdx == 0) {
            startscr_buttonInfo[10].angle = startscr_buttonInfo[buttonIdx].angle;
            startscr_buttonInfo[10].uvs[3][0] = startscr_buttonInfo[0].uvs[3][0] + 160.0f * startscr_gButtonScale;
            startscr_buttonInfo[10].uvs[3][1] = startscr_buttonInfo[0].uvs[3][1];
            startscr_buttonInfo[10].uvs[3][2] = startscr_buttonInfo[0].uvs[3][2];
            startscr_buttonInfo[10].uvs[3][0] = 1.02f * startscr_buttonInfo[10].uvs[3][0];
            startscr_buttonInfo[10].uvs[3][1] = 1.02f * startscr_buttonInfo[10].uvs[3][1];
            startscr_buttonInfo[10].uvs[3][2] = 1.02f * startscr_buttonInfo[10].uvs[3][2];
            ScaleUV(unitmatrix[0], startscr_buttonInfo[10].uvs[0], startscr_gButtonScale);
            WPitchUV(startscr_buttonInfo[10].angle - 1.5707964f, startscr_buttonInfo[10].uvs[0]);
            if (startscr_buttonInfo[10].angle > 1.5707964f)
                tex = MBOX_FindTexture_WORK("BUTTON_SELECT", &texIdx, 0, gNumModels - 1, 1);
            else
                tex = MBOX_FindTexture_WORK("BUTTON", &texIdx, 0, gNumModels - 1, 1);
            ZOID_SetTexture(startscr_buttonInfo[10].objIdx, tex, -1);
        }
        buttonY -= startscr_gButtonSpace * startscr_gButtonScale;
    }
    memcpy(prevPos, startscr_buttonInfo[16].uvs[3], 12);
    startscr_buttonInfo[16].angle += 6.2831855f * gFrameSeconds;
    if (startscr_buttonInfo[16].angle > 6.2831855f)
        startscr_buttonInfo[16].angle -= 6.2831855f;
    startscr_buttonInfo[16].uvs[3][0] = startscr_buttonX * startscr_cursorZ / startscr_buttonZ + (-60.0f) * startscr_gButtonScale;
    startscr_buttonInfo[16].uvs[3][1] = selectedY * startscr_cursorZ / startscr_buttonZ;
    startscr_buttonInfo[16].uvs[3][2] = startscr_cursorZ;
    if (!startscr_buttonsJustInited) {
        if (60.0f * fabsf(startscr_buttonInfo[16].uvs[3][1] - prevPos[1]) / 10.0f > startscr_cursorSpeed) {
            startscr_cursorSpeed = 60.0f * fabsf(startscr_buttonInfo[16].uvs[3][1] - prevPos[1]) / 10.0f;
            startscr_cursorSpeed = startscr_cursorSpeed < 540.0f * startscr_gButtonScale ? startscr_cursorSpeed : 540.0f * startscr_gButtonScale;
            startscr_cursorSpeed = startscr_cursorSpeed > 180.0f * startscr_gButtonScale ? startscr_cursorSpeed : 180.0f * startscr_gButtonScale;
        }
        if (startscr_buttonInfo[16].uvs[3][1] > prevPos[1] + startscr_cursorSpeed * gFrameSeconds)
            startscr_buttonInfo[16].uvs[3][1] = prevPos[1] + startscr_cursorSpeed * gFrameSeconds;
        else if (startscr_buttonInfo[16].uvs[3][1] < prevPos[1] - startscr_cursorSpeed * gFrameSeconds)
            startscr_buttonInfo[16].uvs[3][1] = prevPos[1] - startscr_cursorSpeed * gFrameSeconds;
        else
            startscr_cursorSpeed = 180.0f * startscr_gButtonScale;
    }
    ScaleUV(unitmatrix[0], startscr_buttonInfo[16].uvs[0], 0.4f * startscr_gButtonScale);
    WRollUV(startscr_buttonInfo[16].angle, startscr_buttonInfo[16].uvs[0]);
    WPitchUV(1.5707964f, startscr_buttonInfo[16].uvs[0]);
    memcpy(prevPos, startscr_buttonInfo[17].uvs[3], 12);
    startscr_buttonInfo[17].uvs[3][0] = startscr_buttonX * startscr_cursorZ / startscr_buttonZ + (72.0f + gNumPlayers * 22.5f) * startscr_gButtonScale;
    startscr_buttonInfo[17].uvs[3][1] = startscr_maxY * startscr_cursorZ / startscr_buttonZ;
    startscr_buttonInfo[17].uvs[3][2] = startscr_cursorZ;
    if (!startscr_buttonsJustInited) {
        if (60.0f * fabsf(startscr_buttonInfo[17].uvs[3][0] - prevPos[0]) / 10.0f > playerCursorSpeed) {
            playerCursorSpeed = 60.0f * fabsf(startscr_buttonInfo[17].uvs[3][0] - prevPos[0]) / 10.0f;
            playerCursorSpeed = playerCursorSpeed < 540.0f * startscr_gButtonScale ? playerCursorSpeed : 540.0f * startscr_gButtonScale;
            playerCursorSpeed = playerCursorSpeed > 180.0f * startscr_gButtonScale ? playerCursorSpeed : 180.0f * startscr_gButtonScale;
        }
        if (startscr_buttonInfo[17].uvs[3][0] > prevPos[0] + playerCursorSpeed * gFrameSeconds)
            startscr_buttonInfo[17].uvs[3][0] = prevPos[0] + playerCursorSpeed * gFrameSeconds;
        else if (startscr_buttonInfo[17].uvs[3][0] < prevPos[0] - playerCursorSpeed * gFrameSeconds)
            startscr_buttonInfo[17].uvs[3][0] = prevPos[0] - playerCursorSpeed * gFrameSeconds;
        else
            playerCursorSpeed = 180.0f * startscr_gButtonScale;
    }
    if (startscr_buttonInfo[0].angle > 1.5707964f)
        tex = MBOX_FindTexture_WORK("CURSOR", &texIdx, 0, gNumModels - 1, 1);
    else
        tex = MBOX_FindTexture_WORK("CURSOR_B", &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(startscr_buttonInfo[17].objIdx, tex, -1);
    ScaleUVXYZ(unitmatrix[0], startscr_buttonInfo[17].uvs[0], 0.6f, 0.4f, 0.4f);
    ScaleUV(startscr_buttonInfo[17].uvs[0], startscr_buttonInfo[17].uvs[0], startscr_gButtonScale);
    WPitchUV(1.5707964f + startscr_buttonInfo[0].angle, startscr_buttonInfo[17].uvs[0]);
    startscr_buttonsJustInited = 0;
}
