#include "game/game.h"

typedef struct Slot64 {
    s32 unknown0,id;
    f32 angle,matrix[3][3],position[3];
    s32 unknown3C;
} Slot64;
/* PC: the pak file record's host layout (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct Resource { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 player; } Resource;
typedef struct Owner { void *unknown0,*unknown4; Resource **resource; } Owner;   /* the profile, pc_prelude.h */
typedef struct Player76 { u8 unknown0[72]; Owner **owner; } Player76;
typedef struct Blit Blit;

VIEW(u32, gActiveEdges);
#define gActiveEdges gActiveEdges_view

VIEW(f32[3][3], unitmatrix);
#define unitmatrix unitmatrix_view
VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view
VIEW(Slot64[13], modescr_buttonInfo);
#define modescr_buttonInfo modescr_buttonInfo_view
VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view

extern Player76 gPlayers[];

extern void *memcpy(void *,const void *,u32);

extern void ZOID_SetTexture(s16,intptr_t,s32);
extern void *MBOX_FindTexture_WORK(char *,s16 *,s32,s8,s32);

extern void RemoveBlit ( Blit * );
extern f32 fabsf(f32);
#pragma intrinsic (fabsf)

s32 modescr_optionValid(s32 index)
{
    s32 value=modescr_validOption[gNumPlayers-1][index];
    if(index==5 && !IsTrackUnlocked(18))value=0;
    return value;
}

/* : modescr_updateButtons = MAT3 modescr.c updateButtons (GC 0x800745F0, 0x414 B). Direct typed port; locals
 * and order from the GC DWARF (buttonIdx, prevPos[3], buttonY, selectY, tex, texIdx).  N64 differences (retail
 * displacements/constants): unselected buttons settle to 0.0f (GC 0.17453292f); the cursor block stores
 * position[0], position[1], position[2] in that order (GC: [2], [0], [1]); fabsf on the right of the speed compare
 * (`cursorSpeed < fabsf(d) * 60.0f / 10.0f`, single precision); gFrameSeconds and gNumModels
 * (gNumModels) are volatile (lui/addiu/l* 0(reg) at every use).  Names: buttonInfo = modescr_buttonInfo[13] (0x40),
 * gButtonScale/gButtonSpace/buttonX/maxY/buttonZ/cursorZ = modescr_gButtonScale..modescr_cursorZ, cursorSpeed = modescr_cursorSpeed,
 * buttonsInited/buttonsJustInited = modescr_buttonsInited/modescr_buttonsJustInited, gModeMenuSelect, optionValid = modescr_optionValid,
 * CopyUV, MBOX_HideObject/ShowObject = MBOX_HideObject/MBOX_ShowObject, ZOID_SetTexture =
 * ZOID_SetTexture, MBOX_FindTexture_WORK, WPitchUV/WRollUV/ScaleUV = WPitchUV/WRollUV/ScaleUV.
 * SHAPING (disclosed frame residual): unused0..unused2 are unused f32 locals declared first.  Retail frame 96; without
 * them 88 with every home 4 bytes high (prevPos 72, selectY 64, texIdx 58); one gives the right homes but frame 88;
 * three give retail exactly (prevPos 68, selectY 60, texIdx 54, frame 96) -- the same three-slot residual found
 * the startscr updateButtons sibling.  No pad arrays.
 */
void modescr_updateButtons(void)
{
    f32 unused0, unused1, unused2;
    s32 buttonIdx;
    f32 prevPos[3];
    f32 buttonY;
    f32 selectY;
    void *tex;
    u16 texIdx;
    if (!modescr_buttonsInited) return;
    buttonY = modescr_maxY;
    for (buttonIdx = 0; buttonIdx < 7; buttonIdx++) {
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
        if (modescr_buttonInfo[buttonIdx].angle > 1.5707964f) tex = MBOX_FindTexture_WORK(modescr_select_texture_name, (s16 *)&texIdx, 0, gNumModels - 1, 1);
        else tex = MBOX_FindTexture_WORK(modescr_button_texture_name, (s16 *)&texIdx, 0, gNumModels - 1, 1);
        ZOID_SetTexture((s16)modescr_buttonInfo[buttonIdx].id, (intptr_t)tex, -1);
        if (!modescr_optionValid(buttonIdx)) MBOX_HideObject(modescr_buttonInfo[buttonIdx].id, 0, 15);
        else {
            MBOX_ShowObject(modescr_buttonInfo[buttonIdx].id, 0, 15);
            buttonY -= modescr_gButtonSpace * modescr_gButtonScale;
        }
    }
    memcpy(prevPos, modescr_buttonInfo[7].position, 12);
    modescr_buttonInfo[7].angle += 6.2831855f * gFrameSeconds;
    if (modescr_buttonInfo[7].angle > 6.2831855f) modescr_buttonInfo[7].angle -= 6.2831855f;
    CopyUV(unitmatrix, modescr_buttonInfo[7].matrix);
    modescr_buttonInfo[7].position[0] = modescr_buttonX - 80.0f;
    modescr_buttonInfo[7].position[1] = selectY * modescr_cursorZ / modescr_buttonZ;
    modescr_buttonInfo[7].position[2] = modescr_cursorZ;
    if (!modescr_buttonsJustInited) {
        if (modescr_cursorSpeed < fabsf(modescr_buttonInfo[7].position[1] - prevPos[1]) * 60.0f / 10.0f) {
            modescr_cursorSpeed = fabsf(modescr_buttonInfo[7].position[1] - prevPos[1]) * 60.0f / 10.0f;
            modescr_cursorSpeed = modescr_cursorSpeed < 540.0f * modescr_gButtonScale ? modescr_cursorSpeed : 540.0f * modescr_gButtonScale;
            modescr_cursorSpeed = modescr_cursorSpeed > 180.0f * modescr_gButtonScale ? modescr_cursorSpeed : 180.0f * modescr_gButtonScale;
        }
        if (modescr_buttonInfo[7].position[1] > prevPos[1] + modescr_cursorSpeed * gFrameSeconds) modescr_buttonInfo[7].position[1] = prevPos[1] + modescr_cursorSpeed * gFrameSeconds;
        else if (modescr_buttonInfo[7].position[1] < prevPos[1] - modescr_cursorSpeed * gFrameSeconds) modescr_buttonInfo[7].position[1] = prevPos[1] - modescr_cursorSpeed * gFrameSeconds;
        else modescr_cursorSpeed = 180.0f * modescr_gButtonScale;
    }
    WRollUV(modescr_buttonInfo[7].angle, modescr_buttonInfo[7].matrix);
    WPitchUV(1.5707964f, modescr_buttonInfo[7].matrix);
    ScaleUV(modescr_buttonInfo[7].matrix, modescr_buttonInfo[7].matrix, 0.4f);
    modescr_buttonsJustInited = 0;
}

void updateModeScreen(void)
{
    s32 i;
    if (gSkipToGameState != 1) {
        if (gSkipToGameState != gGameState) {
            if (gSkipToGameState == 0x4000) setGameState(0x4000);
            else setGameState(0x80);
            return;
        }
        gSkipToGameState = 1;
    }
    if (!modescr_inited) modescr_initScreen();
    while (!modescr_optionValid(gModeMenuSelect)) {
        if (++gModeMenuSelect >= 7) gModeMenuSelect = 0;
    }
    modescr_updateButtons();
    if (gActiveEdges & 3) { PlayStartABSound(gActiveEdges); exitModeScreen = 1; }
    else if (gActiveEdges & 4) { PlayStartABSound(gActiveEdges); exitModeScreen = 2; }
    else if (gActiveEdges & 0x400) {
        PlayUDSound(gActiveEdges);
        do { if (--gModeMenuSelect < 0) gModeMenuSelect = 6; } while (!modescr_optionValid(gModeMenuSelect));
    } else if (gActiveEdges & 0x800) {
        PlayUDSound(gActiveEdges);
        do { if (++gModeMenuSelect >= 7) gModeMenuSelect = 0; } while (!modescr_optionValid(gModeMenuSelect));
    }
    for (i = 0; i < gNumPlayers; i++) {
        Resource **fileh = (*gPlayers[i].owner)->resource;
        if (fileh && PS_PakChanged((*fileh)->player)) {
            modescr_uninitScreen(); setGameState(0x20); return;
        }
    }
    if (gSoftReset) { modescr_uninitScreen(); setGameState(4); return; }
    if (exitModeScreen) {
        modescr_uninitScreen();
        switch (exitModeScreen) {
        case 1:
            switch (gModeMenuSelect) {
            case 0: gRace = 0; setGameState(0x80); break;
            case 1: gRace = 1; setGameState(0x80); break;
            case 2: gRace = 2; setGameState(0x80); break;
            case 3: gRace = 3; setGameState(0x4000); break;
            case 4: gRace = 4; setGameState(0x80); break;
            case 5: gRace = 5; setGameState(0x80); break;
            case 6: gRace = 6; setGameState(0x80); break;
            }
            break;
        case 2: setGameState(0x20); break;
        }
        exitModeScreen = 0;
    }
}

void modescr_uninitScreen(void)
{
    modescr_uninitButtons();
    if (modescr_blit) { RemoveBlit(modescr_blit); modescr_blit = 0; }
    removeBG();
    modescr_inited = 0;
}

void modescr_uninitButtons(void)
{
    s32 buttonIdx;
    for (buttonIdx = 0; buttonIdx < 13; buttonIdx++) {
        if (modescr_buttonInfo[buttonIdx].id != -1) {
            ZOID_RemoveObject(modescr_buttonInfo[buttonIdx].id);
            modescr_buttonInfo[buttonIdx].id = -1;
        }
    }
    modescr_buttonsInited = 0;
}
