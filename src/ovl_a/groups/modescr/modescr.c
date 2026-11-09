/* The mode menu (MAT3 modescr.c): race, practice, time trial (ghost), circuit, stunt, obstacle course and battle,
 * a column of 3D buttons with a spinning cursor. updateModeScreen runs each frame in GS_MODE; modescr_init.c sets the
 * screen up and modescr_animText.c draws its text. */
#include "game/game.h"

/* A 3D menu button (MAT3 tMenuButton without its alpha: ModeScreenButton in include/game). */
typedef struct Slot64 {
    s32 unknown0,id;
    f32 angle,matrix[3][3],position[3];
    s32 unknown3C;
} Slot64;
/* PC: the pak file record's host layout (pc_prelude.h, docs/port/POINTERS.md 9.10): pak is the pak it is on. */
typedef struct Resource { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 pak; } Resource;
/* A profile (MAT3 tPlayerProfile) in its PC layout: only its pak file handle (NULL for a guest). */
typedef struct Owner { void *unknown0,*unknown4; Resource **fileh; } Owner;
/* A player's pad record (MAT3 tPadState, gPlayers): only its profile. */
typedef struct Player76 { u8 unknown0[72]; Owner **profile; } Player76;
typedef struct Blit Blit;

VIEW(Slot64[13], modescr_buttonInfo);
#define modescr_buttonInfo modescr_buttonInfo_view
extern Player76 gPlayers[];

extern void *memcpy(void *,const void *,u32);

extern f32 fabsf(f32);

/* The menu's rows (gModeMenuSelect, MAT3 enum_kModeMenuCount): row n sets gRace n. */
#define kModeMenuRace 0
#define kModeMenuPractice 1
#define kModeMenuTimeTrial 2
#define kModeMenuCircuit 3
#define kModeMenuStunt 4
#define kModeMenuObstacle 5
#define kModeMenuBattle 6
#define kModeMenuCount 7

/* Is the menu's row `index` offered? By the player count (modescr_validOption[players - 1][row]); the obstacle
 * course also needs track 18 unlocked. */
s32 modescr_optionValid(s32 index)
{
    s32 value=modescr_validOption[gNumPlayers-1][index];
    if(index==kModeMenuObstacle && !IsTrackUnlocked(TRACK_OBSTACLE))value=0;
    return value;
}

/* Lay the 3D buttons out for this frame (MAT3 modescr.c updateButtons). The selected row's button turns to show its
 * selected face (a half turn at 4 pi a second; unselected ones turn back to 0, MAT3 to 10 degrees); the offered rows
 * stack down from maxY, each moving at most 180 a second towards its place, the others hidden. The cursor spins
 * beside the selected row and eases towards it at 180 to 540 a second (scaled by gButtonScale). */
void modescr_updateButtons(void)
{
    s32 buttonIdx;
    f32 prevPos[3];
    f32 buttonY;
    f32 selectY;
    void *tex;
    u16 texIdx;
    if (!modescr_buttonsInited) return;
    buttonY = modescr_maxY;
    for (buttonIdx = 0; buttonIdx < kModeMenuCount; buttonIdx++) {
        if (buttonIdx == gModeMenuSelect) {
            selectY = buttonY;
            if (modescr_buttonInfo[buttonIdx].angle < 3.1415927f) modescr_buttonInfo[buttonIdx].angle += 12.566371f * gFrameSeconds;
            if (modescr_buttonInfo[buttonIdx].angle > 3.1415927f || modescr_buttonsJustInited == 1) modescr_buttonInfo[buttonIdx].angle = 3.1415927f;
        } else {
            if (modescr_buttonInfo[buttonIdx].angle > 0.0f) modescr_buttonInfo[buttonIdx].angle -= 12.566371f * gFrameSeconds;
            if (modescr_buttonInfo[buttonIdx].angle < 0.0f || modescr_buttonsJustInited == 1) modescr_buttonInfo[buttonIdx].angle = 0.0f;
        }
        memcpy(modescr_buttonInfo[buttonIdx].matrix, unitmatrix, 36);
        memcpy(prevPos, modescr_buttonInfo[buttonIdx].position, 12);
        modescr_buttonInfo[buttonIdx].position[0] = modescr_buttonX;
        modescr_buttonInfo[buttonIdx].position[1] = buttonY;
        modescr_buttonInfo[buttonIdx].position[2] = modescr_buttonZ;
        if (!modescr_buttonsJustInited) {
            if (modescr_buttonInfo[buttonIdx].position[1] > prevPos[1] + 180.0f * gFrameSeconds) modescr_buttonInfo[buttonIdx].position[1] = prevPos[1] + 180.0f * gFrameSeconds;
            if (modescr_buttonInfo[buttonIdx].position[1] < prevPos[1] - 180.0f * gFrameSeconds) modescr_buttonInfo[buttonIdx].position[1] = prevPos[1] - 180.0f * gFrameSeconds;
        }
        WPitchUV(modescr_buttonInfo[buttonIdx].angle - 1.5707964f, modescr_buttonInfo[buttonIdx].matrix);
        if (modescr_buttonInfo[buttonIdx].angle > 1.5707964f) tex = MBOX_FindTexture_WORK(modescr_select_texture_name, (s16 *)&texIdx, 0, gNumModels[0] - 1, 1);
        else tex = MBOX_FindTexture_WORK(modescr_button_texture_name, (s16 *)&texIdx, 0, gNumModels[0] - 1, 1);
        ZOID_SetTexture((s16)modescr_buttonInfo[buttonIdx].id, tex, -1);
        if (!modescr_optionValid(buttonIdx)) MBOX_HideObject(modescr_buttonInfo[buttonIdx].id, 0, 15);
        else {
            MBOX_ShowObject(modescr_buttonInfo[buttonIdx].id, 0, 15);
            buttonY -= modescr_gButtonSpace * modescr_gButtonScale;
        }
    }
    memcpy(prevPos, modescr_buttonInfo[MODE_CURSOR].position, 12);
    modescr_buttonInfo[MODE_CURSOR].angle += 6.2831855f * gFrameSeconds;
    if (modescr_buttonInfo[MODE_CURSOR].angle > 6.2831855f) modescr_buttonInfo[MODE_CURSOR].angle -= 6.2831855f;
    CopyUV(unitmatrix, modescr_buttonInfo[MODE_CURSOR].matrix);
    modescr_buttonInfo[MODE_CURSOR].position[0] = modescr_buttonX - 80.0f;
    modescr_buttonInfo[MODE_CURSOR].position[1] = selectY * modescr_cursorZ / modescr_buttonZ;
    modescr_buttonInfo[MODE_CURSOR].position[2] = modescr_cursorZ;
    if (!modescr_buttonsJustInited) {
        if (modescr_cursorSpeed < fabsf(modescr_buttonInfo[MODE_CURSOR].position[1] - prevPos[1]) * 60.0f / 10.0f) {
            modescr_cursorSpeed = fabsf(modescr_buttonInfo[MODE_CURSOR].position[1] - prevPos[1]) * 60.0f / 10.0f;
            modescr_cursorSpeed = modescr_cursorSpeed < 540.0f * modescr_gButtonScale ? modescr_cursorSpeed : 540.0f * modescr_gButtonScale;
            modescr_cursorSpeed = modescr_cursorSpeed > 180.0f * modescr_gButtonScale ? modescr_cursorSpeed : 180.0f * modescr_gButtonScale;
        }
        if (modescr_buttonInfo[MODE_CURSOR].position[1] > prevPos[1] + modescr_cursorSpeed * gFrameSeconds) modescr_buttonInfo[MODE_CURSOR].position[1] = prevPos[1] + modescr_cursorSpeed * gFrameSeconds;
        else if (modescr_buttonInfo[MODE_CURSOR].position[1] < prevPos[1] - modescr_cursorSpeed * gFrameSeconds) modescr_buttonInfo[MODE_CURSOR].position[1] = prevPos[1] - modescr_cursorSpeed * gFrameSeconds;
        else modescr_cursorSpeed = 180.0f * modescr_gButtonScale;
    }
    WRollUV(modescr_buttonInfo[MODE_CURSOR].angle, modescr_buttonInfo[MODE_CURSOR].matrix);
    WPitchUV(1.5707964f, modescr_buttonInfo[MODE_CURSOR].matrix);
    ScaleUV(modescr_buttonInfo[MODE_CURSOR].matrix, modescr_buttonInfo[MODE_CURSOR].matrix, 0.4f);
    modescr_buttonsJustInited = 0;
}

/* The screen's frame. After a race it passes the player straight on to the circuit menu or the track select
 * (gSkipToGameState). Start or A takes the row (the race type, then the track select, or the circuit menu for a
 * circuit), B goes back to the player screen; up and down move between the offered rows. A pak pulled from under a
 * player's profile goes back to the player screen, a soft reset to the title. */
void updateModeScreen(void)
{
    s32 i;
    if (gSkipToGameState != SKIP_NONE) {
        if (gSkipToGameState != gGameState) {
            if (gSkipToGameState == GS_CIRCUIT) setGameState(GS_CIRCUIT);
            else setGameState(GS_TRACK);
            return;
        }
        gSkipToGameState = SKIP_NONE;
    }
    if (!modescr_inited) modescr_initScreen();
    while (!modescr_optionValid(gModeMenuSelect)) {
        if (++gModeMenuSelect >= kModeMenuCount) gModeMenuSelect = 0;
    }
    modescr_updateButtons();
    if (gActiveEdges & BUTTON_ACCEPT) { PlayStartABSound(gActiveEdges); exitModeScreen = 1; }
    else if (gActiveEdges & BUTTON_B) { PlayStartABSound(gActiveEdges); exitModeScreen = 2; }
    else if (gActiveEdges & BUTTON_UP) {
        PlayUDSound(gActiveEdges);
        do { if (--gModeMenuSelect < 0) gModeMenuSelect = kModeMenuCount - 1; } while (!modescr_optionValid(gModeMenuSelect));
    } else if (gActiveEdges & BUTTON_DOWN) {
        PlayUDSound(gActiveEdges);
        do { if (++gModeMenuSelect >= kModeMenuCount) gModeMenuSelect = 0; } while (!modescr_optionValid(gModeMenuSelect));
    }
    for (i = 0; i < gNumPlayers; i++) {
        Resource **fileh = (*gPlayers[i].profile)->fileh;
        if (fileh && PS_PakChanged((*fileh)->pak)) {
            modescr_uninitScreen(); setGameState(GS_PLAYER); return;
        }
    }
    if (gSoftReset) { modescr_uninitScreen(); setGameState(GS_TITLE); return; }
    if (exitModeScreen) {
        modescr_uninitScreen();
        switch (exitModeScreen) {
        case 1:
            switch (gModeMenuSelect) {
            case kModeMenuRace: gRace = kOneRace; setGameState(GS_TRACK); break;
            case kModeMenuPractice: gRace = kPractice; setGameState(GS_TRACK); break;
            case kModeMenuTimeTrial: gRace = kGhost; setGameState(GS_TRACK); break;
            case kModeMenuCircuit: gRace = kCircuit; setGameState(GS_CIRCUIT); break;
            case kModeMenuStunt: gRace = kStunt; setGameState(GS_TRACK); break;
            case kModeMenuObstacle: gRace = kObstacle; setGameState(GS_TRACK); break;
            case kModeMenuBattle: gRace = kBattle; setGameState(GS_TRACK); break;
            }
            break;
        case 2: setGameState(GS_PLAYER); break;
        }
        exitModeScreen = 0;
    }
}

/* Take the screen down: its buttons, its text blit and the background. */
void modescr_uninitScreen(void)
{
    modescr_uninitButtons();
    if (modescr_blit) { RemoveBlit(modescr_blit); modescr_blit = 0; }
    removeBG();
    modescr_inited = 0;
}

/* Remove the screen's 3D buttons from the scene. */
void modescr_uninitButtons(void)
{
    s32 buttonIdx;
    for (buttonIdx = 0; buttonIdx < MODE_BUTTONS; buttonIdx++) {
        if (modescr_buttonInfo[buttonIdx].id != -1) {
            ZOID_RemoveObject(modescr_buttonInfo[buttonIdx].id);
            modescr_buttonInfo[buttonIdx].id = -1;
        }
    }
    modescr_buttonsInited = 0;
}
