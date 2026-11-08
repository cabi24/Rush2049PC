/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* superseding revision: the two inlined helpers formerly named modescr_uninitButtons
 * modescr_uninitScreen are renamed after their retail deleted stubs, modescr_uninitButtons (uninitButtons@modescr.c) and
 * modescr_uninitScreen (uninitScreen@modescr.c), and claimed. Post-order of updateModeScreen (its last calls are the three
 * uninitScreen calls): uninitButtons 99384, uninitScreen 9938C, optionValid 99394, updateButtons 993EC, initButtons
 * stub 998B8, initScreen 998C0, updateModeScreen 99D10 = the retail layout. No code changed. */
/* superseding revision: modescr_updateButtons (modescr updateButtons) and updateModeScreen (updateModeScreen) are
 * now claimed members; modescr_optionValid and modescr_animText.c's modescr_animText are unchanged.  Shared-declaration change:
 * gNumModels and gFrameSeconds are volatile (only the new bodies use them). */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
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
extern s16 gNumPlayers;
extern s8 modescr_validOption_biased[][7],modescr_inited,gSoftReset;
extern s32 modescr_buttonsInited,modescr_buttonsJustInited,gModeMenuSelect,exitModeScreen,gRace;
extern u32 gSkipToGameState,gGameState,gActiveEdges;
extern f32 modescr_gButtonScale,modescr_gButtonSpace,modescr_buttonX,modescr_maxY,modescr_buttonZ,modescr_cursorZ;
extern f32 modescr_cursorSpeed,unitmatrix[3][3];
extern volatile f32 gFrameSeconds;
extern Slot64 modescr_buttonInfo[13];
extern volatile u8 gNumModels;
extern char modescr_select_texture_name[],modescr_button_texture_name[];
extern Player76 gPlayers[];
extern Blit *modescr_blit;
extern s32 IsTrackUnlocked(s32);
extern s8 PS_PakChanged(s32);
extern void *memcpy(void *,const void *,u32);
extern void CopyUV(void *,void *);
extern void WPitchUV(f32,f32 (*)[3]);
extern void WRollUV(f32,f32 (*)[3]);
extern void ScaleUV(f32 (*)[3],f32 (*)[3],f32);
extern void ZOID_SetTexture(s16,intptr_t,s32);
extern void *MBOX_FindTexture_WORK(char *,s16 *,s32,s8,s32);
extern void MBOX_HideObject(s32,s32,u32),MBOX_ShowObject(s32,s32,u32);
extern void setGameState(s32),modescr_initScreen(void);
extern void PlayStartABSound(u32),PlayUDSound(u32);
extern void ZOID_RemoveObject(s16),RemoveBlit(Blit *),removeBG(void);
extern f32 fabsf(f32);
#pragma intrinsic (fabsf)

s32 modescr_optionValid(s32 index)
{
    s32 value=modescr_validOption_biased[gNumPlayers][index];
    if(index==5 && !IsTrackUnlocked(18))value=0;
    return value;
}

/* : modescr_updateButtons = MAT3 modescr.c updateButtons (GC 0x800745F0, 0x414 B). Direct typed port; locals
 * and order from the GC DWARF (buttonIdx, prevPos[3], buttonY, selectY, tex, texIdx).  N64 differences (retail
 * displacements/constants): unselected buttons settle to 0.0f (GC 0.17453292f); the cursor block stores
 * position[0], position[1], position[2] in that order (GC: [2], [0], [1]); fabsf on the right of the speed compare
 * (`cursorSpeed < fabsf(d) * 60.0f / 10.0f`, single precision); gFrameSeconds (gFrameSeconds) and gNumModels
 * (gNumModels) are volatile (lui/addiu/l* 0(reg) at every use).  Names: buttonInfo = modescr_buttonInfo[13] (0x40),
 * gButtonScale/gButtonSpace/buttonX/maxY/buttonZ/cursorZ = modescr_gButtonScale..modescr_cursorZ, cursorSpeed = modescr_cursorSpeed,
 * buttonsInited/buttonsJustInited = modescr_buttonsInited/modescr_buttonsJustInited, gModeMenuSelect = gModeMenuSelect, optionValid = modescr_optionValid,
 * CopyUV = CopyUV, MBOX_HideObject/ShowObject = MBOX_HideObject/MBOX_ShowObject, ZOID_SetTexture =
 * ZOID_SetTexture, MBOX_FindTexture_WORK = MBOX_FindTexture_WORK, WPitchUV/WRollUV/ScaleUV = WPitchUV/WRollUV/ScaleUV.
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

/* : updateModeScreen = MAT3 modescr.c updateModeScreen (GC 0x80073A38, 0x2B8 B), the game-called root.
 * Direct typed port; uninitScreen (GC 0x80073DE8) and uninitButtons (GC 0x8007457C) are real NON-static functions off
 * `keep` (unique symbols modescr_uninitScreen / modescr_uninitButtons), defined after the root, inlined three times by
 * umerge (retail's caller-less stubs 0x80399384/0x8039938C).
 * N64 differences: none in structure; gPlayers[i].save->fileh/pak = gPlayers[i].owner->resource->player, PS_PakChanged =
 * PS_PakChanged, ZOID_RemoveObject = ZOID_RemoveObject, RemoveBlit = RemoveBlit, removeBG = removeBG,
 * setGameState = setGameState, PlayStartABSound = PlayStartABSound, PlayUDSound = PlayUDSound,
 * gSkipToGameState/gGameState = gSkipToGameState/gGameState, inited = modescr_inited, blit = modescr_blit, exitModeScreen =
 * exitModeScreen, gRace = gRace, gSoftReset = gSoftReset, gActiveEdges = gActiveEdges, initScreen = modescr_initScreen.
 * MATCH on the first typed port.
 */
void modescr_uninitScreen(void);

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

void modescr_uninitButtons(void);

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
