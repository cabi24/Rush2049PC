/* The start (main) menu (MAT3 startscr.c), GS_START: the player count, records, options, audio, video, the cheat
 * menu once its code has been entered, PC SETUP on the PC, and exit; a column of 3D buttons with a spinning cursor
 * and a second cursor over the player count. Entering the start menu's code here unlocks the cheat menu. The text
 * is startscr_text.c. */
#include "game/game.h"

typedef struct Blit Blit;
/* A 3D menu button (MAT3 tMenuButton without its alpha): its type (startscr_buttonNames), its scene object, its
 * turn, its matrix (uvs[3] its position). */
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
extern void *memcpy(void *, const void *, u32);
extern void particle_lifetime_set(void);      /* createBG */

/* startscr_InitEgg is this file's InitEgg (inlined on the N64): it points at the main menu's code, start_screen_egg;
   eggs.c's InitEgg at cheat `egg` of gEggs. Under one name the PC linked eggs.c's for both, so InitEgg(23)
   pointed past the table and the cheat screen could never be opened (docs/port/QA.md). */
/* PC: game.pc_setup (port/config/start_menu.c, docs/port/FRONTEND.md "PC SETUP"): option slot 8, never valid on the
   N64, is PC SETUP while it is on, A on it opens the settings overlay over the menu; game.cheats_menu: slot 6 valid. */
int pc_start_valid(int index, int value);
int pc_start_exit(int exit, int select);
#define PC_START_VALID(i, v) pc_start_valid((i), (v))
#define PC_START_EXIT(e) ((e) = pc_start_exit((e), gStartMenuSelect))

/* The menu's rows (gStartMenuSelect, MAT3 enum_kStartMenuCount). */
#define kStartMenuPlayers 0         /* left and right set the player count */
#define kStartMenuRecord 1
#define kStartMenuOption 2
#define kStartMenuAudio 3
#define kStartMenuVideo 4
#define kStartMenuWeb 5             /* MAT3's; never offered on the N64 */
#define kStartMenuCheat 6           /* offered once the start menu's code has been entered (or game.cheats_menu) */
#define kStartMenuDebug 7           /* offered (when its startscr_validOption entry is set) while game button 8 is held */
#define kStartMenuPath 8            /* MAT3's; PC SETUP on the PC */
#define kStartMenuExit 9
#define kStartMenuCount 10
#define GS_DEBUG 0x10000000         /* the debug row's state (MAT3 rush_flags.h guesses GS_PAK_EXIT) */
#define EGG_START_MENU 23           /* the start menu's code, past gEggs' own (start_screen_egg) */
#define CHEAT_MENU_UNLOCKED 21      /* gCheatActive[]: the start menu's code has been entered */
#define SONG_MENU 6                 /* the front end's music */
/* startscr_buttonInfo[]: the ten rows' buttons, then these. */
#define START_PLAYERS_VALUE 10      /* the player count's value button */
#define START_TITLE 11
#define START_A_ICON 12             /* the A button's icon and its arrow */
#define START_A_ARROW 13
#define START_B_ICON 14             /* the B button's icon and its arrow */
#define START_B_ARROW 15
#define START_CURSOR 16
#define START_PLAYER_CURSOR 17      /* the cursor over the player count */
#define START_BUTTONS 18

/* Start watching for code `egg`: this file's own InitEgg, which numbers the start menu's code 23 (start_screen_egg,
 * just past gEggs). */
void startscr_InitEgg(s32 egg)
{
    gEgg = &start_screen_egg[egg - EGG_START_MENU];
    gNextTimeOut = 0;
    gEggIdx = 0;
}

/* The menu's frame. After a race it passes the player straight on to the player screen (starting the menu music
 * when coming from a race). It counts the controllers plugged in from the first (gMaxPlayers) and keeps the player
 * count within them. Start takes the first row, A the selected one, B goes back to the title; left and right
 * change the player count on its row; up and down move between the offered rows. A taken row opens its screen
 * (exit stops the music and the effects). A soft reset goes back to the title. Every frame the start menu's code
 * is followed until it is entered (CheckForEgg). */
void updateStartScreen(void)
{
    s32 ctrlIdx;
    if (!gSoftReset)
        gGameStarted = 1;
    if (gSkipToGameState != SKIP_NONE) {
        if (gSkipToGameState != gGameState) {
            if (!(gSkipToGameState & GS_RACE_MASK) && gOptions[14] <= 12) {
                AudSetMusicVol(gOptions[12] / 10.0f, 0.05f);
                AudPlaySong(SONG_MENU, 1);
            }
            setGameState(GS_PLAYER);
            return;
        }
        gSkipToGameState = SKIP_NONE;
    }
    if (!startscr_inited)
        startscr_initScreen();
    while (!startscr_optionValid(gStartMenuSelect)) {
        if (++gStartMenuSelect >= kStartMenuCount)
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
    if (gActiveEdges & BUTTON_START) {
        PlayStartABSound(gActiveEdges);
        gStartMenuSelect = kStartMenuPlayers;
        exitStartScreen = 1;
    } else if (gActiveEdges & BUTTON_A) {
        PlayStartABSound(gActiveEdges);
        exitStartScreen = 1;
    } else if (gActiveEdges & BUTTON_B) {
        PlayStartABSound(gActiveEdges);
        exitStartScreen = 2;
    } else if (gActiveEdges & BUTTON_LEFT) {
        PlayLRSound(gActiveEdges);
        if (gStartMenuSelect == kStartMenuPlayers) {
            if (--gNumPlayers < 1)
                gNumPlayers = gMaxPlayers;
        }
    } else if (gActiveEdges & BUTTON_RIGHT) {
        PlayLRSound(gActiveEdges);
        if (gStartMenuSelect == kStartMenuPlayers) {
            if (++gNumPlayers > gMaxPlayers)
                gNumPlayers = 1;
        }
    } else if (gActiveEdges & BUTTON_UP) {
        PlayUDSound(gActiveEdges);
        do {
            if (--gStartMenuSelect < 0)
                gStartMenuSelect = kStartMenuCount - 1;
        } while (!startscr_optionValid(gStartMenuSelect));
    } else if (gActiveEdges & BUTTON_DOWN) {
        PlayUDSound(gActiveEdges);
        do {
            if (++gStartMenuSelect >= kStartMenuCount)
                gStartMenuSelect = 0;
        } while (!startscr_optionValid(gStartMenuSelect));
    }
    startscr_updateButtons();
    if (gSoftReset) {
        startscr_uninitScreen();
        setGameState(GS_TITLE);
    } else if (PC_START_EXIT(exitStartScreen)) {
        startscr_uninitScreen();
        switch (exitStartScreen) {
            case 1:
                switch (gStartMenuSelect) {
                    case kStartMenuPlayers: setGameState(GS_PLAYER); break;
                    case kStartMenuRecord: setGameState(GS_RECORDS); break;
                    case kStartMenuOption: setGameState(GS_OPTIONS); break;
                    case kStartMenuAudio: setGameState(GS_AUDIO); break;
                    case kStartMenuVideo: setGameState(GS_VIDEO); break;
                    case kStartMenuCheat: setGameState(GS_CHEAT); break;
                    case kStartMenuDebug: setGameState(GS_DEBUG); break;
                    case kStartMenuExit:
                        /* MAT3 then calls ExitSystem: the N64's is an empty function (a console has nothing to
                         * exit to), so the PC calls nothing. */
                        AudKillSong();
                        AudStopAllEffects();
                        break;
                }
                break;
            case 2:
                setGameState(GS_TITLE);
                break;
        }
        exitStartScreen = 0;
    }
    if (!gCheatActive[CHEAT_MENU_UNLOCKED])
        gCheatActive[CHEAT_MENU_UNLOCKED] = CheckForEgg();
}

/* Set the menu up: the menu music (at the music volume option), the buttons, the text blit, the presses so far
 * dropped, and the start menu's code watched for until it has been entered. */
void startscr_initScreen(void)
{
    if (gOptions[14] <= 12) {
        AudSetMusicVol(gOptions[12] / 10.0f, 0.05f);
        AudPlaySong(SONG_MENU, 1);
    }
    particle_lifetime_set();
    startscr_initButtons();
    startscr_blit = NewMultiBlit(0, 0, startscr_mblit, 1);
    exitStartScreen = 0;
    ClearRushControls();
    if (!gCheatActive[CHEAT_MENU_UNLOCKED])
        startscr_InitEgg(EGG_START_MENU);
    startscr_inited = 1;
}

/* Take the menu down: its text blit, the background and its buttons. */
void startscr_uninitScreen(void)
{
    if (startscr_blit) {
        RemoveBlit(startscr_blit);
        startscr_blit = 0;
    }
    removeBG();
    startscr_uninitButtons();
    startscr_inited = 0;
}

/* Make the menu's 3D buttons that don't exist yet, each a model by its type (types 0 and 1 drawn see-through,
 * 0x42000), white; then texture and place the title, the A and B icons with their arrows and the cursor. */
void startscr_initButtons(void)
{
    tRGBA buttonColor = {255, 255, 255, 255};
    void *tex;
    u16 texIdx;
    s32 buttonIdx;
    for (buttonIdx = 0; buttonIdx < START_BUTTONS; buttonIdx++)
        if (startscr_buttonInfo[buttonIdx].objIdx == -1) {
            memcpy(startscr_buttonInfo[buttonIdx].uvs, unitmatrix, 48);
            startscr_buttonInfo[buttonIdx].objIdx = MBOX_NewObject(
                MBOX_FindObject_WORK(startscr_buttonNames[startscr_buttonInfo[buttonIdx].buttonType], 0, gNumModels - 1, 1),
                startscr_buttonInfo[buttonIdx].uvs, -1,
                (startscr_buttonInfo[buttonIdx].buttonType == 0 || startscr_buttonInfo[buttonIdx].buttonType == 1) ? 0x42000 : 0);
            ZOID_SetColor(startscr_buttonInfo[buttonIdx].objIdx, &buttonColor);
        }
    tex = MBOX_FindTexture_WORK("BUTTON_TITLE", &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(startscr_buttonInfo[START_TITLE].objIdx, tex, -1);
    memcpy(startscr_buttonInfo[START_TITLE].uvs, unitmatrix, 36);
    WPitchUV(-1.5707964f, startscr_buttonInfo[START_TITLE].uvs);
    startscr_buttonInfo[START_TITLE].uvs[3][0] = 0.0f;
    startscr_buttonInfo[START_TITLE].uvs[3][1] = 115.0f;
    startscr_buttonInfo[START_TITLE].uvs[3][2] = 200.0f;
    tex = MBOX_FindTexture_WORK("A1", &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(startscr_buttonInfo[START_A_ICON].objIdx, tex, -1);
    memcpy(startscr_buttonInfo[START_A_ICON].uvs, unitmatrix, 36);
    WPitchUV(-1.5707964f, startscr_buttonInfo[START_A_ICON].uvs);
    startscr_buttonInfo[START_A_ICON].uvs[3][0] = 75.1f;
    startscr_buttonInfo[START_A_ICON].uvs[3][1] = 55.2f;
    startscr_buttonInfo[START_A_ICON].uvs[3][2] = 100.0f;
    ScaleUV(startscr_buttonInfo[START_A_ICON].uvs[0], startscr_buttonInfo[START_A_ICON].uvs[0], 0.5f);
    memcpy(startscr_buttonInfo[START_A_ARROW].uvs, unitmatrix, 36);
    WPitchUV(-1.5707964f, startscr_buttonInfo[START_A_ARROW].uvs);
    WRollUV(3.1415927f, startscr_buttonInfo[START_A_ARROW].uvs);
    startscr_buttonInfo[START_A_ARROW].uvs[3][0] = 87.1f;
    startscr_buttonInfo[START_A_ARROW].uvs[3][1] = 55.2f;
    startscr_buttonInfo[START_A_ARROW].uvs[3][2] = 100.0f;
    tex = MBOX_FindTexture_WORK("B1", &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(startscr_buttonInfo[START_B_ICON].objIdx, tex, -1);
    memcpy(startscr_buttonInfo[START_B_ICON].uvs, unitmatrix, 36);
    WPitchUV(-1.5707964f, startscr_buttonInfo[START_B_ICON].uvs);
    startscr_buttonInfo[START_B_ICON].uvs[3][0] = -75.1f;
    startscr_buttonInfo[START_B_ICON].uvs[3][1] = 55.20f;
    startscr_buttonInfo[START_B_ICON].uvs[3][2] = 100.0f;
    ScaleUV(startscr_buttonInfo[START_B_ICON].uvs[0], startscr_buttonInfo[START_B_ICON].uvs[0], 0.5f);
    memcpy(startscr_buttonInfo[START_B_ARROW].uvs, unitmatrix, 36);
    WPitchUV(-1.5707964f, startscr_buttonInfo[START_B_ARROW].uvs);
    startscr_buttonInfo[START_B_ARROW].uvs[3][0] = -87.1f;
    startscr_buttonInfo[START_B_ARROW].uvs[3][1] = 55.20f;
    startscr_buttonInfo[START_B_ARROW].uvs[3][2] = 100.0f;
    tex = MBOX_FindTexture_WORK("CURSOR", &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(startscr_buttonInfo[START_CURSOR].objIdx, tex, -1);
    startscr_buttonsInited = 1;
    startscr_buttonsJustInited = 1;
    ZeroCameras();
}

/* Remove the menu's 3D buttons from the scene. */
void startscr_uninitButtons(void)
{
    s32 buttonIdx;
    for (buttonIdx = 0; buttonIdx < START_BUTTONS; buttonIdx++)
        if (startscr_buttonInfo[buttonIdx].objIdx != -1) {
            ZOID_RemoveObject(startscr_buttonInfo[buttonIdx].objIdx);
            startscr_buttonInfo[buttonIdx].objIdx = -1;
        }
    startscr_buttonsInited = 0;
}

/* Lay the 3D buttons out for this frame. The offered rows stack down from maxY (the others hidden), the selected one
 * turned to show its selected face (a half turn at 4 pi a second); the player count's value button follows the
 * first row. The cursor spins beside the selected row and eases towards it, the player cursor slides over the
 * player count; both at 180 to 540 a second (scaled by gButtonScale). */
void startscr_updateButtons(void)
{
    s32 buttonIdx;
    f32 prevPos[3];
    void *tex;
    u16 texIdx;
    f32 buttonY, selectedY;
    if (!startscr_buttonsInited)
        return;
    buttonY = startscr_maxY;
    selectedY = startscr_maxY;
    for (buttonIdx = 0; buttonIdx < kStartMenuCount; buttonIdx++) {
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
        if (buttonIdx == kStartMenuPlayers) {
            startscr_buttonInfo[START_PLAYERS_VALUE].angle = startscr_buttonInfo[buttonIdx].angle;
            startscr_buttonInfo[START_PLAYERS_VALUE].uvs[3][0] = startscr_buttonInfo[0].uvs[3][0] + 160.0f * startscr_gButtonScale;
            startscr_buttonInfo[START_PLAYERS_VALUE].uvs[3][1] = startscr_buttonInfo[0].uvs[3][1];
            startscr_buttonInfo[START_PLAYERS_VALUE].uvs[3][2] = startscr_buttonInfo[0].uvs[3][2];
            startscr_buttonInfo[START_PLAYERS_VALUE].uvs[3][0] = 1.02f * startscr_buttonInfo[START_PLAYERS_VALUE].uvs[3][0];
            startscr_buttonInfo[START_PLAYERS_VALUE].uvs[3][1] = 1.02f * startscr_buttonInfo[START_PLAYERS_VALUE].uvs[3][1];
            startscr_buttonInfo[START_PLAYERS_VALUE].uvs[3][2] = 1.02f * startscr_buttonInfo[START_PLAYERS_VALUE].uvs[3][2];
            ScaleUV(unitmatrix[0], startscr_buttonInfo[START_PLAYERS_VALUE].uvs[0], startscr_gButtonScale);
            WPitchUV(startscr_buttonInfo[START_PLAYERS_VALUE].angle - 1.5707964f, startscr_buttonInfo[START_PLAYERS_VALUE].uvs[0]);
            if (startscr_buttonInfo[START_PLAYERS_VALUE].angle > 1.5707964f)
                tex = MBOX_FindTexture_WORK("BUTTON_SELECT", &texIdx, 0, gNumModels - 1, 1);
            else
                tex = MBOX_FindTexture_WORK("BUTTON", &texIdx, 0, gNumModels - 1, 1);
            ZOID_SetTexture(startscr_buttonInfo[START_PLAYERS_VALUE].objIdx, tex, -1);
        }
        buttonY -= startscr_gButtonSpace * startscr_gButtonScale;
    }
    memcpy(prevPos, startscr_buttonInfo[START_CURSOR].uvs[3], 12);
    startscr_buttonInfo[START_CURSOR].angle += 6.2831855f * gFrameSeconds;
    if (startscr_buttonInfo[START_CURSOR].angle > 6.2831855f)
        startscr_buttonInfo[START_CURSOR].angle -= 6.2831855f;
    startscr_buttonInfo[START_CURSOR].uvs[3][0] = startscr_buttonX * startscr_cursorZ / startscr_buttonZ + (-60.0f) * startscr_gButtonScale;
    startscr_buttonInfo[START_CURSOR].uvs[3][1] = selectedY * startscr_cursorZ / startscr_buttonZ;
    startscr_buttonInfo[START_CURSOR].uvs[3][2] = startscr_cursorZ;
    if (!startscr_buttonsJustInited) {
        if (60.0f * fabsf(startscr_buttonInfo[START_CURSOR].uvs[3][1] - prevPos[1]) / 10.0f > startscr_cursorSpeed) {
            startscr_cursorSpeed = 60.0f * fabsf(startscr_buttonInfo[START_CURSOR].uvs[3][1] - prevPos[1]) / 10.0f;
            startscr_cursorSpeed = startscr_cursorSpeed < 540.0f * startscr_gButtonScale ? startscr_cursorSpeed : 540.0f * startscr_gButtonScale;
            startscr_cursorSpeed = startscr_cursorSpeed > 180.0f * startscr_gButtonScale ? startscr_cursorSpeed : 180.0f * startscr_gButtonScale;
        }
        if (startscr_buttonInfo[START_CURSOR].uvs[3][1] > prevPos[1] + startscr_cursorSpeed * gFrameSeconds)
            startscr_buttonInfo[START_CURSOR].uvs[3][1] = prevPos[1] + startscr_cursorSpeed * gFrameSeconds;
        else if (startscr_buttonInfo[START_CURSOR].uvs[3][1] < prevPos[1] - startscr_cursorSpeed * gFrameSeconds)
            startscr_buttonInfo[START_CURSOR].uvs[3][1] = prevPos[1] - startscr_cursorSpeed * gFrameSeconds;
        else
            startscr_cursorSpeed = 180.0f * startscr_gButtonScale;
    }
    ScaleUV(unitmatrix[0], startscr_buttonInfo[START_CURSOR].uvs[0], 0.4f * startscr_gButtonScale);
    WRollUV(startscr_buttonInfo[START_CURSOR].angle, startscr_buttonInfo[START_CURSOR].uvs[0]);
    WPitchUV(1.5707964f, startscr_buttonInfo[START_CURSOR].uvs[0]);
    memcpy(prevPos, startscr_buttonInfo[START_PLAYER_CURSOR].uvs[3], 12);
    startscr_buttonInfo[START_PLAYER_CURSOR].uvs[3][0] = startscr_buttonX * startscr_cursorZ / startscr_buttonZ + (72.0f + gNumPlayers * 22.5f) * startscr_gButtonScale;
    startscr_buttonInfo[START_PLAYER_CURSOR].uvs[3][1] = startscr_maxY * startscr_cursorZ / startscr_buttonZ;
    startscr_buttonInfo[START_PLAYER_CURSOR].uvs[3][2] = startscr_cursorZ;
    if (!startscr_buttonsJustInited) {
        if (60.0f * fabsf(startscr_buttonInfo[START_PLAYER_CURSOR].uvs[3][0] - prevPos[0]) / 10.0f > playerCursorSpeed) {
            playerCursorSpeed = 60.0f * fabsf(startscr_buttonInfo[START_PLAYER_CURSOR].uvs[3][0] - prevPos[0]) / 10.0f;
            playerCursorSpeed = playerCursorSpeed < 540.0f * startscr_gButtonScale ? playerCursorSpeed : 540.0f * startscr_gButtonScale;
            playerCursorSpeed = playerCursorSpeed > 180.0f * startscr_gButtonScale ? playerCursorSpeed : 180.0f * startscr_gButtonScale;
        }
        if (startscr_buttonInfo[START_PLAYER_CURSOR].uvs[3][0] > prevPos[0] + playerCursorSpeed * gFrameSeconds)
            startscr_buttonInfo[START_PLAYER_CURSOR].uvs[3][0] = prevPos[0] + playerCursorSpeed * gFrameSeconds;
        else if (startscr_buttonInfo[START_PLAYER_CURSOR].uvs[3][0] < prevPos[0] - playerCursorSpeed * gFrameSeconds)
            startscr_buttonInfo[START_PLAYER_CURSOR].uvs[3][0] = prevPos[0] - playerCursorSpeed * gFrameSeconds;
        else
            playerCursorSpeed = 180.0f * startscr_gButtonScale;
    }
    if (startscr_buttonInfo[0].angle > 1.5707964f)
        tex = MBOX_FindTexture_WORK("CURSOR", &texIdx, 0, gNumModels - 1, 1);
    else
        tex = MBOX_FindTexture_WORK("CURSOR_B", &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(startscr_buttonInfo[START_PLAYER_CURSOR].objIdx, tex, -1);
    ScaleUVXYZ(unitmatrix[0], startscr_buttonInfo[START_PLAYER_CURSOR].uvs[0], 0.6f, 0.4f, 0.4f);
    ScaleUV(startscr_buttonInfo[START_PLAYER_CURSOR].uvs[0], startscr_buttonInfo[START_PLAYER_CURSOR].uvs[0], startscr_gButtonScale);
    WPitchUV(1.5707964f + startscr_buttonInfo[0].angle, startscr_buttonInfo[START_PLAYER_CURSOR].uvs[0]);
    startscr_buttonsJustInited = 0;
}
