#include "game/game.h"

typedef struct tCircuit { u8 pad0[4]; u8 u4; u8 pad5[3]; s8 b8; s8 b9; } tCircuit;
VIEW(tCircuit *, gCircuit);
#define gCircuit gCircuit_view

VIEW(u8[], gUnlockedCircuits);
#define gUnlockedCircuits gUnlockedCircuits_view
VIEW(s8[][19], gUnlockedTracks);
#define gUnlockedTracks gUnlockedTracks_view

void resetEnabled(void)
{
    gContinueEnabled = !(gCircuit->b9 >= gCircuitSize) && gCircuit->u4 != 0;
    gStatsEnabled = gCircuit->u4;
    if (gCircuit->b8 > 0) {
        gUnlockedCircuits[1] = 1;
        gUnlockedTracks[0][4] = 1;
        gForceUnlockedCircuits[1] = 1;
    }
    if (gCircuit->b8 >= 2 && expansion_pak_present != 0) {
        gUnlockedCircuits[2] = 1;
        gUnlockedTracks[0][5] = 1;
        gForceUnlockedCircuits[2] = 1;
    }
    if (gCircuit->b8 == 3) {
        gUnlockedCircuits[3] = 1;
        gForceUnlockedCircuits[3] = 1;
    }
    if (expansion_pak_present == 0 && gCircuit->b8 == 2) {
        gContinueEnabled = 0;
    }
}

VIEW(u32, gActiveAutoEdges);
#define gActiveAutoEdges gActiveAutoEdges_view

VIEW(u8[20], circuitscr_gCircuitCode);
#define circuitscr_gCircuitCode circuitscr_gCircuitCode_view
VIEW(u8 *, gPasswordAlphabet);
#define gPasswordAlphabet gPasswordAlphabet_view

void controlCircuitCode(void)
{
    s32 result;
    if (gActiveAutoEdges & 0x400) {
        AudPlayEffect(0x28, 0, 1, 0);
        if (gCodeSelect == -2) gCodeSelect = 21;
        else if (gCodeSelect < 8) gCodeSelect += 24;
        else if (gCodeSelect < 12) gCodeSelect = -2;
        else gCodeSelect -= 12;
    } else if (gActiveAutoEdges & 0x800) {
        AudPlayEffect(0x27, 0, 1, 0);
        if (gCodeSelect == -2) gCodeSelect = 9;
        else if (gCodeSelect < 20) gCodeSelect += 12;
        else if (gCodeSelect < 24) gCodeSelect = -2;
        else gCodeSelect -= 24;
    } else if (gActiveAutoEdges & 0x1000) {
        AudPlayEffect(0x29, 0, 1, 0);
        if (gCodeSelect == -2) gCodeSelect = 31;
        else if (gCodeSelect == 24) gCodeSelect = -2;
        else if (gCodeSelect % 12 == 0) gCodeSelect += 11;
        else gCodeSelect--;
    } else if (gActiveAutoEdges & 0x2000) {
        AudPlayEffect(0x2C, 0, 1, 0);
        if (gCodeSelect == -2) gCodeSelect = 24;
        else if (gCodeSelect == 31) gCodeSelect = -2;
        else if (gCodeSelect % 12 == 11) gCodeSelect -= 11;
        else gCodeSelect++;
    }
    if (gActiveEdges & 2) {
        AudPlayEffect(0x25, 0, 1, 0);
        if (gCodeSelect == -2) {
            if (gCodeLen > 0) {
                gCodeLen--;
                circuitscr_gCircuitCode[gCodeLen] = '.';
                gInvalidCode = 0;
                gWrongNumPlayers = 0;
            }
        } else if (gCodeLen < 20) {
            circuitscr_gCircuitCode[gCodeLen] = gPasswordAlphabet[gCodeSelect];
            gCodeLen++;
            if (gCodeLen == 20) {
                result = SetCircuitCode(circuitscr_gCircuitCode);
                if (result == 0) gInvalidCode = 1;
                else if (result == -1) gWrongNumPlayers = 1;
                else {
                    gEnterCircuitCode = 0;
                    resetEnabled();
                    circuitscr_buttonsJustInited = 1;
                    gReviewTrack = gCircuit->b9;
                }
            }
        }
    } else if (gActiveEdges & 5) {
        PlayExitSound(gActiveEdges);
        gEnterCircuitCode = 0;
        circuitscr_buttonsJustInited = 1;
    }
}

/* PC: the pak file record's host layout (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct Item { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 player; } Item;
typedef struct Selection { Item *item; } Selection;
typedef struct Resource { u8 *bytes; } Resource;
typedef struct MenuOwner MenuOwner;
typedef struct OwnerBody {
    MenuOwner *next, *previous;
    Selection *selection;
    u8 other[32];
    Resource *resource;
} OwnerBody;
struct MenuOwner { OwnerBody *body; };
typedef struct Player { u8 other[72]; MenuOwner *owner; } Player;
/* The profile record (save.c's SaveSlot) is 64 bytes on the PC, its name at +0x20 (PROFILE_OFS, pc_prelude.h;
 * docs/port/POINTERS.md 9.10). */
typedef struct Object { u8 other[32]; f32 scale; u8 tail[32]; } Object;
typedef struct Blit Blit;
typedef struct FiveParts FiveParts;
typedef struct Texture Texture;
typedef struct MultiBlit {
    char *name;
    s16 x,y,width,height,top,bottom,left,right;
    u32 z,alpha;
    s32 (*callback)(Blit *);
    u32 descriptor;
} MultiBlit;

extern Player gPlayers[];

/* The circuit's cars (MAT3 tCircuitCar): rank, the car's place over the circuit (MAT3 totalPlace). */
typedef struct PlayerStats {u8 other0,rank;unsigned short count;u8 other4[72];} PlayerStats;
VIEW(PlayerStats[], gCircuitCars);
#define gCircuitCars gCircuitCars_view
VIEW(u8, gNumModels);
#define gNumModels gNumModels_view

VIEW(f32[12], gTrophyMat);
#define gTrophyMat gTrophyMat_view
VIEW(f32[12], unitmatrix);
#define unitmatrix unitmatrix_view

VIEW(Object[], object_list_scale_view);
#define object_list_scale_view object_list_scale_view_view

VIEW(MultiBlit[], circuitscr_mblit);
#define circuitscr_mblit circuitscr_mblit_view

extern Texture *MBOX_FindTexture_WORK(char *,s16 *,s8,s8,s32);
extern void ZOID_SetTexture(s16,intptr_t,s32);

extern void particle_lifetime_set(void);

extern FiveParts *InitPane(s32,s32,s32,s32,s32,intptr_t,s32,s32);
extern Blit *NewMultiBlit(s16,s16,const MultiBlit *,s16);

void circuitscr_initScreen(void)
{
    s32 i;
    s32 selected;
    s32 model;
    s16 texture_index;
    for (i = 0; i < gNumPlayers; i++) {
        SavePlayerRaceType((u8 ***)gPlayers[i].owner, gRace);
    }
    newCircuitType = 0;
    if (gCircuit->u4 && gCircuit->b9 == 0)
        gCircuit->u4 = 0;
    gReviewTrack = gCircuit->b9;
    if (gCircuit->u4) {
        ContinueCircuit();
        if (gCircuit->b9 >= gCircuitSize) {
            selected = (s8) gCircuitCars[0].rank;
            gShowTrophy = 1;
            if (selected < 3) {
                model = MBOX_FindObject_WORK(gTrophyNames[selected], 0, gNumModels - 1, 1);
                trophyObjIdx = MBOX_NewObject(model, (uintptr_t)gTrophyMat, -1, 0);
                model = MBOX_FindObject_WORK(gTrophySheenNames[selected], 0, gNumModels - 1, 1);
                trophySheenObjIdx = MBOX_NewObject(model, (uintptr_t)unitmatrix, trophyObjIdx, 0x2C6084);
                ZOID_SetTexture((s16)trophySheenObjIdx,
                    (intptr_t)MBOX_FindTexture_WORK(circuitscr_sheen_texture_name, &texture_index, 0, gNumModels - 1, 1), -1);
                object_list_scale_view[trophySheenObjIdx].scale = circuitscr_trophy_scale;
                ZOID_SetColor((s16)trophySheenObjIdx, trophySheenColor);
                ZOID_SetEnvColor((s16)trophySheenObjIdx, trophySheenColor);
                MBOX_ShowObject(trophyObjIdx, 0, 15);
                CopyUV(unitmatrix, gTrophyMat);
                gTrophyMat[9] = 0.0f;
                gTrophyMat[10] = -13.0f;
                gTrophyMat[11] = 65.0f;
                ZeroCameras();
            }
        }
    }
    resetEnabled();
    gCircuitMenuSelect = 0;
    gEnterCircuitCode = 0;
    gConfirmNewCircuit = 0;
    circuitscr_gYes = 0;
    calcProgressSize();
    particle_lifetime_set();
    circuitscr_initButtons();
    circuitscr_pane = InitPane(-20, 0, -10, 10, 192, 0, 0, 0);
    circuitscr_blit = NewMultiBlit(0, 0, circuitscr_mblit, 1);
    circuitscr_inited = 1;
}

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

s32 menuItemValid(s32 mode)
{
    return (mode != 3 || gPlayers[gCircuitPlayer].owner->body->selection == 0)
        && (!(mode == 0) || gContinueEnabled != 0)
        && (!(mode == 1) || gStatsEnabled != 0);
}

void updateCircuitScreen(void)
{
    s32 i;
    gCircuit = (tCircuit *)(gPlayers[gCircuitPlayer].owner->body->resource->bytes + 0x6F4);
    if (gSkipToGameState != 1) {
        if (gSkipToGameState != gGameState) {
            setGameState(0x80);
            return;
        }
        gSkipToGameState = 1;
    }
    if (!circuitscr_inited)
        circuitscr_initScreen();
    while (!menuItemValid(gCircuitMenuSelect)) {
        gCircuitMenuSelect++;
        if (gCircuitMenuSelect >= 4)
            gCircuitMenuSelect = 0;
    }
    if (gShowTrophy) controlShowTrophy(); else if (gEnterCircuitCode) {
        controlCircuitCode();
    } else if (gActiveEdges & 1) {
        AudPlayEffect(0x2E, 0, 1, 0);
        if (gConfirmNewCircuit) {
            gConfirmNewCircuit = 0;
            circuitscr_gYes = 0;
            circuitscr_buttonsJustInited = 1;
        } else if (!menuItemValid(0)) {
            gCircuitMenuSelect = 2;
            goto confirm_option;
        } else {
            ContinueCircuit();
            gCircuitScreenExit = 1;
        }
    } else if (gActiveEdges & 2) {
        AudPlayEffect(0x25, 0, 1, 0);
 confirm_option:
        switch (gCircuitMenuSelect) {
        case 0:
            ContinueCircuit();
            gCircuitScreenExit = 1;
            break;
        case 1:
            break;
        case 3:
            gEnterCircuitCode = 1;
            gCodeLen = 0;
            gCodeSelect = gCodeLen;
            TB_Strcpy(circuitscr_gCircuitCode, circuitscr_empty_code_text);
            gInvalidCode = 0;
            gWrongNumPlayers = 0;
            break;
        case 2:
            if (circuitscr_gYes || !gContinueEnabled) {
                NewCircuit(newCircuitType);
                ContinueCircuit();
                gCircuitScreenExit = 1;
            } else if (gConfirmNewCircuit) {
                gConfirmNewCircuit = 0;
                circuitscr_gYes = 0;
                circuitscr_buttonsJustInited = 1;
            } else {
                circuitscr_gYes = 0;
                gConfirmNewCircuit = 1;
            }
            break;
        }
    } else if (gActiveEdges & 4) {
        AudPlayEffect(0x26, 0, 1, 0);
        if (gConfirmNewCircuit) {
            gConfirmNewCircuit = 0;
            circuitscr_gYes = 0;
            circuitscr_buttonsJustInited = 1;
        } else {
            gCircuitScreenExit = 2;
        }
    } else if (gActiveEdges & 0x400) {
        if (!gConfirmNewCircuit) {
            PlayUDSound(gActiveEdges);
            do {
                gCircuitMenuSelect--;
                if (gCircuitMenuSelect < 0)
                    gCircuitMenuSelect = 3;
            } while (!menuItemValid(gCircuitMenuSelect));
        }
    } else if (gActiveEdges & 0x800) {
        if (!gConfirmNewCircuit) {
            PlayUDSound(gActiveEdges);
            do {
                gCircuitMenuSelect++;
                if (gCircuitMenuSelect >= 4)
                    gCircuitMenuSelect = 0;
            } while (!menuItemValid(gCircuitMenuSelect));
        }
    } else if (gActiveEdges & 0x3000) {
        if (gConfirmNewCircuit) {
            PlayLRSound(gActiveEdges);
            circuitscr_gYes = !circuitscr_gYes;
        } else if (gCircuitMenuSelect == 2) {
            PlayLRSound(gActiveEdges);
            do {
                if (gActiveEdges & 0x1000)
                    newCircuitType--;
                else if (gActiveEdges & 0x2000)
                    newCircuitType++;
                if (newCircuitType < 0)
                    newCircuitType = 3;
                if (newCircuitType >= 4)
                    newCircuitType = 0;
            } while (!IsCircuitUnlocked(newCircuitType));
        } else if (gCircuitMenuSelect == 1) {
            PlayLRSound(gActiveEdges);
            if (gActiveEdges & 0x1000)
                gReviewTrack--;
            else if (gActiveEdges & 0x2000)
                gReviewTrack++;
            if (gReviewTrack < 0)
                gReviewTrack = gCircuit->b9;
            if (gReviewTrack > gCircuit->b9)
                gReviewTrack = 0;
        }
    }
    for (i = 0; i < gNumPlayers; i++) {
        if (gPlayers[i].owner->body->selection) {
            if (PS_PakChanged(gPlayers[i].owner->body->selection->item->player)) {
                circuitscr_uninitScreen();
                setGameState(0x20);
                return;
            }
        }
    }
    if (gSoftReset) {
        circuitscr_uninitScreen();
        setGameState(4);
    } else if (gCircuitScreenExit) {
        circuitscr_uninitScreen();
        if (gCircuitScreenExit == 1) {
            switch (gCircuitMenuSelect) {
            case 0: setGameState(0x80); break;
            case 2: setGameState(0x80); break;
            }
        } else if (gCircuitScreenExit == 2) {
            setGameState(0x40);
        }
        gCircuitScreenExit = 0;
    }
    circuitscr_updateButtons();
}

/* controlShowTrophy (MAT3 circuitscr.c GC controlShowTrophy): inlined into updateCircuitScreen updateCircuitScreen;
 * deleted stub 0x8039B558. Body = the former C140_ANIMATION_ARM. */
void controlShowTrophy(void)
{
    YawUV(-90.0f * gFrameSeconds * circuitscr_degrees_to_radians, (f32 (*)[3])gTrophyMat);
    if (gActiveEdges & 7) {
        if (trophyObjIdx != -1) {
            ZOID_RemoveObject((s16)trophyObjIdx);
            trophyObjIdx = -1;
        }
        if (trophySheenObjIdx != -1) {
            ZOID_RemoveObject((s16)trophySheenObjIdx);
            trophySheenObjIdx = -1;
        }
        PlayExitSound(gActiveEdges);
        gShowTrophy = 0;
        circuitscr_buttonsJustInited = 1;
    }
}

typedef struct tMenuButton {
    s32 other,handle;
    f32 angle;
    f32 matrix[3][3];
    f32 position[3];
    u8 alpha,unaccessed[3];
} tMenuButton;
VIEW(tMenuButton[12], circuitscr_buttonInfo);
#define circuitscr_buttonInfo circuitscr_buttonInfo_view

extern void RemoveBlit(Blit *);
extern void sound_handles_array_clear(s8 *);

void circuitscr_uninitScreen(void)
{
    circuitscr_uninitButtons();
    if (circuitscr_blit) {
        RemoveBlit(circuitscr_blit);
        circuitscr_blit = 0;
    }
    if (circuitscr_pane) {
        sound_handles_array_clear((s8 *)circuitscr_pane);
        circuitscr_pane = 0;
    }
    if (trophyObjIdx != -1) {
        ZOID_RemoveObject((s16)trophyObjIdx);
        trophyObjIdx = -1;
    }
    if (trophySheenObjIdx != -1) {
        ZOID_RemoveObject((s16)trophySheenObjIdx);
        trophySheenObjIdx = -1;
    }
    removeBG();
    circuitscr_inited = 0;
}

/* uninitButtons@circuitscr.c (MAT3 GC uninitButtons): inlined into uninitScreen circuitscr_uninitScreen; deleted stub
 * 0x8039B004. Loop moved verbatim. */
void circuitscr_uninitButtons(void)
{
    s32 i;
    for (i = 0; i < 12; i++) {
        if (circuitscr_buttonInfo[i].handle != -1) {
            if (gGameState & 0x7C03FFFE)
                ZOID_RemoveObject((s16)circuitscr_buttonInfo[i].handle);
            circuitscr_buttonInfo[i].handle = -1;
        }
    }
    circuitscr_buttonsInited = 0;
}

typedef union Color {u32 word;u8 channel[4];} Color;
VIEW(Color, circuitscr_button_color);
#define circuitscr_button_color circuitscr_button_color_view

VIEW(u8, gNumModels);
#define gNumModels gNumModels_view
extern void *memcpy(void *,const void *,u32);
extern f32 fabsf(f32);
#pragma intrinsic(fabsf)

extern Texture *MBOX_FindTexture_WORK(char *,s16 *,s8,s8,s32);
extern void ZOID_SetTexture(s16,intptr_t,s32);

/* : circuitscr_updateButtons = updateButtons@circuitscr.c (MAT3 GC 0x8006A730, 0x89C B): per-frame scroll, fade and
 * texture of the four circuit-menu buttons, their right-hand value buttons (4, 5) and the cursor (7, 9). Direct
 * typed port of reference/repos/mat3 decomp/src/GameSource/src/circuitscr.c updateButtons; called only by
 * updateCircuitScreen as its last statement, so it keeps the group's private ABI (saves only ra).
 * N64 names: buttonInfo circuitscr_buttonInfo, buttonsInited circuitscr_buttonsInited, buttonsJustInited circuitscr_buttonsJustInited, gShowTrophy
 * gShowTrophy, gEnterCircuitCode gEnterCircuitCode, gConfirmNewCircuit gConfirmNewCircuit, gCircuitMenuSelect gCircuitMenuSelect,
 * gButtonScale circuitscr_gButtonScale, gButtonSpace circuitscr_gButtonSpace, buttonX circuitscr_buttonX, maxY circuitscr_maxY, minY circuitscr_minY,
 * buttonZ circuitscr_buttonZ, cursorZ circuitscr_cursorZ, gFrameSeconds gFrameSeconds (volatile), unitmatrix unitmatrix,
 * gNumModels gNumModels, buttonColor initialiser circuitscr_button_color, "BUTTON_SELECT"/"BUTTON" circuitscr_select_texture_name/circuitscr_button_texture_name;
 * MBOX_ShowObject MBOX_ShowObject, MBOX_HideObject MBOX_HideObject, ScaleUV ScaleUV, WPitchUV
 * WPitchUV, WRollUV WRollUV, MBOX_FindTexture_WORK MBOX_FindTexture_WORK, ZOID_SetTexture ZOID_SetTexture,
 * ZOID_SetColor ZOID_SetColor, menuItemValid menuItemValid.
 * N64 differences read from the retail words: unselected buttons settle to 0.0f (GC 0.08726646f); alpha ramps
 * linearly over one button pitch (GC fixed 80); single-precision speed math (fabsf intrinsic, 60.0f/10.0f).
 * cursorSpeed/scrollSpeed are the function-local statics (retail .bss 0x803BA228/0x803BA22C).
 * Shaping (the carscr sibling's measured idioms, carscr_updateButtons): fabsf on the right of the speed tests;
 * the second 255 of each alpha ramp is the int 255 (keeps 255.0f rematerialised, 60.0f hoisted into $f30);
 * gNumModels read through a volatile view (the group's shared declaration stays non-volatile for B560/BE48).
 * FRAME RESIDUAL (disclosed): thirteen unused s32 homes declared first. Retail frame 152 with the locals at
 * sp+60..100 (buttonColor +60, texIdx +66, clearScrollSpeed +75, selectedPos +76, prevPos +84) and 52 bytes of
 * homes above them that no instruction touches; without the homes ugen gives frame 96 (locals +56..96); with
 * them 152 and every slot as retail. Their original names/purpose are unknown (no MAT3/DWARF counterpart; an
 * inlined helper's locals land below the caller's, so it is not an inline). No padding array is used.
 * Own data: six float literals, the two .bss statics. */
void circuitscr_updateButtons(void)
{
    static f32 cursorSpeed;
    static f32 scrollSpeed;
    s32 frame_home0, frame_home1, frame_home2, frame_home3, frame_home4, frame_home5, frame_home6;
    s32 frame_home7, frame_home8, frame_home9, frame_home10, frame_home11, frame_home12;
    s32 buttonIdx;
    f32 prevPos[3];
    f32 buttonY;
    f32 selectedPos = 0.0f;
    s8 clearScrollSpeed = 1;
    Texture *tex;
    s16 texIdx;
    Color buttonColor = circuitscr_button_color;

    if (!circuitscr_buttonsInited)
        return;
    if (gShowTrophy == 1) {
        for (buttonIdx = 0; buttonIdx < 12; buttonIdx++) {
            if (buttonIdx == 6 || buttonIdx == 8 || buttonIdx == 10)
                MBOX_ShowObject(circuitscr_buttonInfo[buttonIdx].handle, 0, 15);
            else {
                circuitscr_buttonInfo[buttonIdx].alpha = 0;
                MBOX_HideObject(circuitscr_buttonInfo[buttonIdx].handle, 0, 15);
            }
        }
        return;
    }
    for (buttonIdx = 0; buttonIdx < 12; buttonIdx++)
        MBOX_ShowObject(circuitscr_buttonInfo[buttonIdx].handle, 0, 15);
    buttonY = circuitscr_maxY;
    for (buttonIdx = 0; buttonIdx < 4; buttonIdx++) {
        if (gEnterCircuitCode || gConfirmNewCircuit) {
            MBOX_HideObject(circuitscr_buttonInfo[buttonIdx].handle, 0, 15);
            if (buttonIdx == 1)
                MBOX_HideObject(circuitscr_buttonInfo[4].handle, 0, 15);
            else if (buttonIdx == 2)
                MBOX_HideObject(circuitscr_buttonInfo[5].handle, 0, 15);
        } else {
            if (buttonIdx == gCircuitMenuSelect) {
                selectedPos = buttonY;
                if (circuitscr_buttonInfo[buttonIdx].angle < 3.1415927f)
                    circuitscr_buttonInfo[buttonIdx].angle += 12.566371f * gFrameSeconds;
                if (circuitscr_buttonInfo[buttonIdx].angle > 3.1415927f)
                    circuitscr_buttonInfo[buttonIdx].angle = 3.1415927f;
                if (circuitscr_buttonsJustInited)
                    circuitscr_buttonInfo[buttonIdx].angle = 3.1415927f;
            } else {
                if (circuitscr_buttonInfo[buttonIdx].angle > 0.0f)
                    circuitscr_buttonInfo[buttonIdx].angle -= 12.566371f * gFrameSeconds;
                if (circuitscr_buttonInfo[buttonIdx].angle < 0.0f)
                    circuitscr_buttonInfo[buttonIdx].angle = 0.0f;
                if (circuitscr_buttonsJustInited)
                    circuitscr_buttonInfo[buttonIdx].angle = 0.0f;
            }
            memcpy(prevPos, circuitscr_buttonInfo[buttonIdx].position, 12);
            circuitscr_buttonInfo[buttonIdx].position[0] = circuitscr_buttonX + -100.0f * circuitscr_gButtonScale;
            circuitscr_buttonInfo[buttonIdx].position[1] = buttonY;
            circuitscr_buttonInfo[buttonIdx].position[2] = circuitscr_buttonZ;
            if (!circuitscr_buttonsJustInited) {
                if (scrollSpeed < fabsf(circuitscr_buttonInfo[buttonIdx].position[1] - prevPos[1]) * 60.0f / 10.0f) {
                    scrollSpeed = fabsf(circuitscr_buttonInfo[buttonIdx].position[1] - prevPos[1]) * 60.0f / 10.0f;
                    scrollSpeed = scrollSpeed < 540.0f * circuitscr_gButtonScale ? scrollSpeed : 540.0f * circuitscr_gButtonScale;
                    scrollSpeed = scrollSpeed > 180.0f * circuitscr_gButtonScale ? scrollSpeed : 180.0f * circuitscr_gButtonScale;
                }
                if (circuitscr_buttonInfo[buttonIdx].position[1] > prevPos[1] + scrollSpeed * gFrameSeconds) {
                    circuitscr_buttonInfo[buttonIdx].position[1] = prevPos[1] + scrollSpeed * gFrameSeconds;
                    clearScrollSpeed = 0;
                } else if (circuitscr_buttonInfo[buttonIdx].position[1] < prevPos[1] - scrollSpeed * gFrameSeconds) {
                    circuitscr_buttonInfo[buttonIdx].position[1] = prevPos[1] - scrollSpeed * gFrameSeconds;
                    clearScrollSpeed = 0;
                }
            }
            ScaleUV((f32 (*)[3])unitmatrix, circuitscr_buttonInfo[buttonIdx].matrix, circuitscr_gButtonScale);
            WPitchUV(circuitscr_buttonInfo[buttonIdx].angle - 1.5707964f, circuitscr_buttonInfo[buttonIdx].matrix);
            if (circuitscr_buttonInfo[buttonIdx].angle > 1.5707964f)
                tex = MBOX_FindTexture_WORK(circuitscr_select_texture_name, &texIdx, 0, *(volatile u8 *)&gNumModels - 1, 1);
            else
                tex = MBOX_FindTexture_WORK(circuitscr_button_texture_name, &texIdx, 0, *(volatile u8 *)&gNumModels - 1, 1);
            ZOID_SetTexture(circuitscr_buttonInfo[buttonIdx].handle, (intptr_t)tex, -1);
            if (circuitscr_buttonInfo[buttonIdx].position[1] > circuitscr_maxY + circuitscr_gButtonSpace * circuitscr_gButtonScale
                || circuitscr_buttonInfo[buttonIdx].position[1] < circuitscr_minY - circuitscr_gButtonSpace * circuitscr_gButtonScale)
                circuitscr_buttonInfo[buttonIdx].alpha = 0;
            else if (circuitscr_buttonInfo[buttonIdx].position[1] > circuitscr_maxY)
                circuitscr_buttonInfo[buttonIdx].alpha = 255.0f - (circuitscr_buttonInfo[buttonIdx].position[1] - circuitscr_maxY) * 255 / (circuitscr_gButtonSpace * circuitscr_gButtonScale);
            else if (circuitscr_buttonInfo[buttonIdx].position[1] < circuitscr_minY)
                circuitscr_buttonInfo[buttonIdx].alpha = 255.0f - (circuitscr_minY - circuitscr_buttonInfo[buttonIdx].position[1]) * 255 / (circuitscr_gButtonSpace * circuitscr_gButtonScale);
            else
                circuitscr_buttonInfo[buttonIdx].alpha = 255;
            if (!menuItemValid(buttonIdx))
                circuitscr_buttonInfo[buttonIdx].alpha = 0;
            buttonColor.channel[3] = circuitscr_buttonInfo[buttonIdx].alpha;
            ZOID_SetColor(circuitscr_buttonInfo[buttonIdx].handle, (s32 *)&buttonColor);
            if (buttonIdx == 1) {
                circuitscr_buttonInfo[4].angle = circuitscr_buttonInfo[buttonIdx].angle;
                circuitscr_buttonInfo[4].position[0] = circuitscr_buttonX + 100.0f * circuitscr_gButtonScale;
                circuitscr_buttonInfo[4].position[1] = circuitscr_buttonInfo[buttonIdx].position[1];
                circuitscr_buttonInfo[4].position[2] = circuitscr_buttonInfo[buttonIdx].position[2];
                ScaleUV((f32 (*)[3])unitmatrix, circuitscr_buttonInfo[4].matrix, circuitscr_gButtonScale);
                WPitchUV(circuitscr_buttonInfo[4].angle - 1.5707964f, circuitscr_buttonInfo[4].matrix);
                ZOID_SetTexture(circuitscr_buttonInfo[4].handle, (intptr_t)tex, -1);
                circuitscr_buttonInfo[4].alpha = circuitscr_buttonInfo[buttonIdx].alpha;
                ZOID_SetColor(circuitscr_buttonInfo[4].handle, (s32 *)&buttonColor);
                if (!menuItemValid(buttonIdx))
                    MBOX_HideObject(circuitscr_buttonInfo[4].handle, 0, 15);
                else if (!circuitscr_buttonInfo[buttonIdx].alpha)
                    MBOX_HideObject(circuitscr_buttonInfo[4].handle, 0, 15);
                else
                    MBOX_ShowObject(circuitscr_buttonInfo[4].handle, 0, 15);
            } else if (buttonIdx == 2) {
                circuitscr_buttonInfo[5].angle = circuitscr_buttonInfo[buttonIdx].angle;
                circuitscr_buttonInfo[5].position[0] = circuitscr_buttonX + 100.0f * circuitscr_gButtonScale;
                circuitscr_buttonInfo[5].position[1] = circuitscr_buttonInfo[buttonIdx].position[1];
                circuitscr_buttonInfo[5].position[2] = circuitscr_buttonInfo[buttonIdx].position[2];
                ScaleUV((f32 (*)[3])unitmatrix, circuitscr_buttonInfo[5].matrix, circuitscr_gButtonScale);
                WPitchUV(circuitscr_buttonInfo[5].angle - 1.5707964f, circuitscr_buttonInfo[5].matrix);
                ZOID_SetTexture(circuitscr_buttonInfo[5].handle, (intptr_t)tex, -1);
                circuitscr_buttonInfo[5].alpha = circuitscr_buttonInfo[buttonIdx].alpha;
                ZOID_SetColor(circuitscr_buttonInfo[5].handle, (s32 *)&buttonColor);
                if (!menuItemValid(buttonIdx))
                    MBOX_HideObject(circuitscr_buttonInfo[5].handle, 0, 15);
                else if (!circuitscr_buttonInfo[buttonIdx].alpha)
                    MBOX_HideObject(circuitscr_buttonInfo[5].handle, 0, 15);
                else
                    MBOX_ShowObject(circuitscr_buttonInfo[5].handle, 0, 15);
            }
            if (!menuItemValid(buttonIdx))
                MBOX_HideObject(circuitscr_buttonInfo[buttonIdx].handle, 0, 15);
            else if (!circuitscr_buttonInfo[buttonIdx].alpha) {
                MBOX_HideObject(circuitscr_buttonInfo[buttonIdx].handle, 0, 15);
                buttonY -= circuitscr_gButtonSpace * circuitscr_gButtonScale;
            } else {
                MBOX_ShowObject(circuitscr_buttonInfo[buttonIdx].handle, 0, 15);
                buttonY -= circuitscr_gButtonSpace * circuitscr_gButtonScale;
            }
        }
    }
    memcpy(prevPos, circuitscr_buttonInfo[7].position, 12);
    circuitscr_buttonInfo[7].angle += 6.2831855f * gFrameSeconds;
    if (circuitscr_buttonInfo[7].angle > 6.2831855f)
        circuitscr_buttonInfo[7].angle -= 6.2831855f;
    circuitscr_buttonInfo[7].position[0] = circuitscr_buttonX * circuitscr_cursorZ / circuitscr_buttonZ + -140.0f * circuitscr_gButtonScale;
    circuitscr_buttonInfo[7].position[1] = selectedPos * circuitscr_cursorZ / circuitscr_buttonZ;
    circuitscr_buttonInfo[7].position[2] = circuitscr_cursorZ;
    if (!circuitscr_buttonsJustInited) {
        if (cursorSpeed < fabsf(circuitscr_buttonInfo[7].position[1] - prevPos[1]) * 60.0f / 10.0f) {
            cursorSpeed = fabsf(circuitscr_buttonInfo[7].position[1] - prevPos[1]) * 60.0f / 10.0f;
            cursorSpeed = cursorSpeed < 540.0f * circuitscr_gButtonScale ? cursorSpeed : 540.0f * circuitscr_gButtonScale;
            cursorSpeed = cursorSpeed > 180.0f * circuitscr_gButtonScale ? cursorSpeed : 180.0f * circuitscr_gButtonScale;
        }
        if (circuitscr_buttonInfo[7].position[1] > prevPos[1] + cursorSpeed * gFrameSeconds)
            circuitscr_buttonInfo[7].position[1] = prevPos[1] + cursorSpeed * gFrameSeconds;
        else if (circuitscr_buttonInfo[7].position[1] < prevPos[1] - cursorSpeed * gFrameSeconds)
            circuitscr_buttonInfo[7].position[1] = prevPos[1] - cursorSpeed * gFrameSeconds;
        else
            cursorSpeed = 180.0f * circuitscr_gButtonScale;
    }
    ScaleUV((f32 (*)[3])unitmatrix, circuitscr_buttonInfo[7].matrix, circuitscr_gButtonScale * 0.4f);
    WRollUV(circuitscr_buttonInfo[7].angle, circuitscr_buttonInfo[7].matrix);
    WPitchUV(1.5707964f, circuitscr_buttonInfo[7].matrix);
    if (gEnterCircuitCode || gConfirmNewCircuit) {
        MBOX_HideObject(circuitscr_buttonInfo[7].handle, 0, 15);
        MBOX_HideObject(circuitscr_buttonInfo[9].handle, 0, 15);
    } else {
        MBOX_ShowObject(circuitscr_buttonInfo[7].handle, 0, 15);
        MBOX_ShowObject(circuitscr_buttonInfo[9].handle, 0, 15);
    }
    if (clearScrollSpeed == 1)
        scrollSpeed = 0.0f;
    circuitscr_buttonsJustInited = 0;
}

typedef struct LanguageIndex {u8 other[108];u16 name_base;} LanguageIndex;
typedef struct Language {
    void *other0;
    u8 **text;
    void *other8;
    LanguageIndex *indices;
    u8 **names;
} Language;
VIEW(Language, gText);
#define gText gText_view

void getPointsStr(s32 player,u8 *destination)
{
    u8 number[32];
    TB_Strcpy(destination,gText.text[80]);
    TB_Sprintf(number,circuitscr_points_format,gCircuitCars[player].count);
    TB_Strcat(destination,number);
    TB_Strcat(destination,gText.text[130]);
}

void getPlaceStr(s32 player,u8 *destination)
{
    TB_Strcpy(destination,gText.text[77]);
    TB_Strcat(destination,gText.names[gCircuitCars[player].rank + gText.indices->name_base]);
    TB_Strcat(destination,circuitscr_place_separator);
    TB_Strcat(destination,gText.text[129]);
}

void getToGoStr(u8 *destination)
{
    u8 number[32];
    TB_Strcpy(destination,gText.text[72]);
    TB_Sprintf(number,circuitscr_to_go_count_format,gCircuitSize - gCircuit->b9);
    TB_Strcat(destination,number);
    TB_Strcat(destination,gCircuitSize - gCircuit->b9 != 1 ? gText.text[71] : gText.text[70]);
    TB_Strcat(destination,gText.text[73]);
}

void getFinishedStr(u8 *destination)
{
    u8 number[32];
    TB_Strcpy(destination,gText.text[74]);
    TB_Sprintf(number,circuitscr_finished_count_format,gCircuit->b9);
    TB_Strcat(destination,number);
    TB_Strcat(destination,gCircuit->b9 != 1 ? gText.text[71] : gText.text[70]);
    TB_Strcat(destination,gText.text[75]);
}

/* getPlayerStr (MAT3 circuitscr.c GC getPlayerStr(int p, unsigned char *str)): inlined into calcProgressSize
 * calcProgressSize; deleted stub 0x8039BA20. */
void getPlayerStr(s32 player,u8 *destination)
{
    TB_Strcpy(destination,(u8 *)gPlayers[player].owner->body + PROFILE_OFS(20));
}

void calcProgressSize(void)
{
    u8 text[80];
    /* Measured unused scalar homes: 128 + 8*4 = native 160-byte frame. */
    s32 frame_home0, frame_home1, frame_home2, frame_home3;
    s32 width;
    s32 maximum;
    s32 player;
    s32 i;
    gProgressWidth = 0;
    gProgressHeight = 0;
    if (gStatsEnabled) {
        HAL_SetFont(11);
        if (gContinueEnabled) {
            gProgressHeight += 35;
            width = HAL_GetTextWidth(gText.text[67],-1);
            gProgressWidth = width < gProgressWidth ? gProgressWidth : width;
            getFinishedStr(text);
            width = HAL_GetTextWidth(text,-1);
            gProgressWidth = width < gProgressWidth ? gProgressWidth : width;
            getToGoStr(text);
            width = HAL_GetTextWidth(text,-1);
            gProgressWidth = width < gProgressWidth ? gProgressWidth : width;
        } else {
            gProgressHeight += 15;
            width = HAL_GetTextWidth(gText.text[76],-1);
            gProgressWidth = width < gProgressWidth ? gProgressWidth : width;
        }
        gProgressHeight += HAL_GetFontAscent() + 30;
        maximum = 0;
        player = gCircuitPlayer;
        for (i = 0; i < 2; i++) {
            getPlayerStr(player,text);
            width = HAL_GetTextWidth(text,-1);
            if (width >= maximum) maximum = width;
            getPlaceStr(player,text);
            width = HAL_GetTextWidth(text,-1);
            if (width >= maximum) maximum = width;
            getPointsStr(player,text);
            width = HAL_GetTextWidth(text,-1);
            if (width >= maximum) maximum = width;
            if (gNumPlayers == 1) break;
            player ^= 1;
        }
        if (gNumPlayers >= 2)
            gProgressWidth = maximum + 160;
        else
            gProgressWidth = maximum < gProgressWidth ? gProgressWidth : maximum;
    }
    gProgressWidth += 20;
    gProgressTop = gMenuBottom + (220 - gMenuBottom - gProgressHeight) / 2;
}

VIEW(Color, flashColors);
#define flashColors flashColors_view
VIEW(Color, flashColors_at_1);
#define flashColors_at_1 flashColors_at_1_view
VIEW(u8 *[], gDefaultNames);
#define gDefaultNames gDefaultNames_view

extern void TB_FlashTextColors(Color,Color);

extern void ChangePaneSize(FiveParts *,s32,s32,s32,s32);

/* A5658 renders the six competitor ranking/name/count entries. The selected
 * result-table selector is compared first in both equality expressions; the
 * natural for loop exposes its initializer and loop-header address setup on
 * the same compiler source line. Both are ordinary C with the same reads.
 *
 * Exact frame reconstruction: without the six unused scalar declarations,
 * stock ugen reports M=140, T=16, P=4 and frame 168. Six four-byte homes make
 * M=164 and frame 192, exactly retail. count moves +52 to +76, name[32] moves
 * +72 to +96, and number[64] moves +104 to +128. Buffer capacities unchanged.
 * These homes are a measured layout reconstruction, not recovered names or
 * a claim about their unknown original purpose. No padding array is used.
 */
void drawReview(void)
{
    u8 number[64],name[32];
    s32 i, rank, x, y;
    s32 count;
    s32 unused_home0, unused_home1, unused_home2;
    s32 unused_home3, unused_home4, unused_home5;
    HAL_SetFont(10);
    TB_SetTextColor(1);
    HAL_GetFontAscent();
    count=6;
    for(i=0;i<count;i++) {
        if(gReviewTrack==gCircuit->b9)rank=gCircuitCars[i].rank;
        else {
            rank=gCircuitCars[i].other4[gReviewTrack];
            if(rank>=6) {
                HAL_SetFont(10);
                TB_SetTextColor(1);
                TB_DrawText(TB_CenterText(gText.text[81],160),165,gText.text[81]);
                return;
            }
        }
        x=rank<3 ? 16 : 160;
        TB_SetTextColor(i<gNumPlayers ? 17 : 1);
        HAL_SetFont(5);
        TB_Sprintf(number,circuitscr_review_place_format,rank+1);
        y=129+(rank%3)*24;
        TB_DrawText(TB_CenterText(number,x+6),y+1,number);
        HAL_SetFont(11);
        if(i<gNumPlayers)TB_Strcpy(name,(u8 *)gPlayers[i].owner->body+PROFILE_OFS(20));
        else TB_Strcpy(name,gDefaultNames[gCircuitCars[i].other0]);
        TB_DrawEllipseText((s16)(x+15),y+2,120,name);
        TB_Sprintf(number,circuitscr_review_points_format,gReviewTrack==gCircuit->b9 ?
                     gCircuitCars[i].count : gCircuitCars[i].other4[24+gReviewTrack]);
        TB_Strcat(number,gText.text[130]);
        TB_DrawText((s16)(x+15),y+11,number);
    }
}

/* A line's width in the current font, as MAT3's HAL_GetTextWidth returns it: an unsigned int (the panes below
 * compare it unsigned). */
static u32 text_width(u8 *str) { return HAL_GetTextWidth(str, -1); }

void circuitscr_animButtonText(void)
{
    s32 i;
    u8 str1[192];
    u8 *str;
    s16 screenPos[2];
    s32 x;
    s32 y;

    HAL_SetTextDepth(0.0f);
    TB_SetJust(0, 1);
    ZOID_ProjectWindow(0, circuitscr_buttonInfo[6].position, &gCamera[0], 0, screenPos);
    x = screenPos[0];
    y = screenPos[1];
    HAL_SetFont(13);
    TB_SetTextColor(1);
    str = gText.text[42];
    TB_DrawText(TB_CenterText(str, x), y, str);
    if (gEnterCircuitCode) {
        static u8 *del = circuitscr_del_text;
        TB_SetJust(0, 3);
        HAL_SetFont(11);
        y = 59;
        x = 115;
        TB_SetTextColor(1);
        TB_DrawText(TB_CenterText(gText.text[44], 160), y, gText.text[44]);
        y += 16;
        if (gInvalidCode || gWrongNumPlayers) {
            TB_SetTextColor(5);
            TB_DrawText(TB_CenterText(gText.text[134], 160), y, gText.text[134]);
        }
        y += 16;
        TB_SetTextColor(12);
        str1[1] = 0;
        for (i = 0; i < 20; i++) {
            str1[0] = circuitscr_gCircuitCode[i];
            TB_DrawText(TB_CenterText(str1, x + 4), y, str1);
            if ((i + 1) % 10 == 0) {
                x = 115;
                y += 9;
            } else
                x += 9;
        }
        TB_SetTextColor(1);
        x = 94;
        y = 126;
        for (i = 0; i < 32; i++) {
            str1[0] = gPasswordAlphabet[i];
            TB_SetTextColor(gCodeSelect == i ? 22 : 1);
            TB_DrawText(TB_CenterText(str1, x + 5), y, str1);
            if ((i + 1) % 12 == 0) {
                x = 94;
                y += 11;
            } else
                x += 11;
        }
        x += 12;
        TB_SetTextColor(gCodeSelect == -2 ? 22 : 1);
        TB_DrawText(TB_CenterText(del, x + 5), y, del);
        if (circuitscr_pane)
            ChangePaneSize(circuitscr_pane, 80, 55, 240, 165);
    } else if (gConfirmNewCircuit) {
        TB_SetJust(1, 3);
        HAL_SetFont(11);
        TB_SetTextColor(1);
        TB_DrawTextScrollBox(160, 59, 140, 78, -1, 0, gText.text[96]);
        y = 59 + 5 * TB_GetFontLineHeight();
        TB_SetTextColor(circuitscr_gYes ? 22 : 1);
        TB_DrawText(145, y, gText.names[((u16 *)gText.indices)[24]]);
        TB_SetTextColor(circuitscr_gYes ? 1 : 22);
        TB_DrawText(175, y, gText.names[((u16 *)gText.indices)[24] + 1]);
        TB_SetJust(0, 3);
        if (circuitscr_pane)
            ChangePaneSize(circuitscr_pane, 60, 55, 260, 130);
    } else {
        HAL_SetFont(11);
        for (i = 0; i < 4; i++) {
            if (circuitscr_buttonInfo[i].angle > 0.6981317f && circuitscr_buttonInfo[i].angle < 2.443461f)
                continue;
            if (!circuitscr_buttonInfo[i].alpha)
                continue;
            HAL_SetTextTranslucency(circuitscr_buttonInfo[i].alpha);
            ZOID_ProjectWindow(0, circuitscr_buttonInfo[i].position, &gCamera[0], 0, screenPos);
            x = screenPos[0] - 35.0f * circuitscr_gButtonScale;
            y = screenPos[1];
            if (circuitscr_buttonInfo[i].angle < 1.5707964f) {
                TB_FlashTextColors(flashColors, flashColors_at_1);
                TB_SetTextColor(1);
            } else {
                TB_FlashTextColors(flashColors, flashColors_at_1);
                TB_SetTextColor(22);
            }
            str = gText.names[((u16 *)gText.indices)[43] + i];
            TB_DrawText(x, y, str);
            if (i == 1) {
                ZOID_ProjectWindow(0, circuitscr_buttonInfo[4].position, &gCamera[0], 0, screenPos);
                x = screenPos[0] - 35.0f * circuitscr_gButtonScale;
                y = screenPos[1];
                if (gReviewTrack == gCircuit->b9)
                    str = gText.text[176];
                else {
                    TB_Sprintf(str1, circuitscr_review_track_format, gText.text[82], gReviewTrack + 1);
                    str = str1;
                }
                TB_DrawText(x, y, str);
            }
            if (i == 2) {
                ZOID_ProjectWindow(0, circuitscr_buttonInfo[5].position, &gCamera[0], 0, screenPos);
                x = screenPos[0] - 35.0f * circuitscr_gButtonScale;
                y = screenPos[1];
                str = gText.names[((u16 *)gText.indices)[44] + newCircuitType];
                TB_DrawText(x, y, str);
            }
        }
        HAL_SetTextTranslucency(-1.0f);
        if (gShowTrophy) {
            s16 width;
            str = str1;
            TB_SetJust(0, 3);
            HAL_SetFont(11);
            TB_SetTextColor(1);
            TB_Sprintf(str, circuitscr_trophy_title_format, gText.names[((u16 *)gText.indices)[44] + gCircuit->b8], gText.text[76]);
            width = text_width(str) < 20 ? 20 : text_width(str);
            TB_DrawText(TB_CenterText(str, 160), 153, str);
            y = 153 + TB_GetFontLineHeight();
            y += TB_GetFontLineHeight();
            getFinishPlaceStr(0, str);
            width = width > text_width(str) ? width : text_width(str);
            TB_DrawText(TB_CenterText(str, 160), y, str);
            y += TB_GetFontLineHeight();
            getPointsStr(0, str);
            width = width > text_width(str) ? width : text_width(str);
            TB_DrawText(TB_CenterText(str, 160), y, str);
            y += TB_GetFontLineHeight();
            ChangePaneSize(circuitscr_pane, 160 - width / 2 - 4, 149, 160 + width / 2 + 4, y + 4);
        } else if (gCircuitMenuSelect == 2) {
            s32 numCircuitTracks;
            s16 width;
            str = str1;
            TB_SetJust(0, 3);
            HAL_SetFont(11);
            TB_SetTextColor(1);
            TB_Sprintf(str, circuitscr_new_circuit_title_format, gText.names[((u16 *)gText.indices)[44] + newCircuitType], gText.text[42]);
            width = text_width(str) < 20 ? 20 : text_width(str);
            TB_DrawText(TB_CenterText(str, 160), 129, str);
            y = 129 + TB_GetFontLineHeight();
            numCircuitTracks = gNumCircuitTracks[newCircuitType];
            if (!expansion_pak_present && numCircuitTracks >= 5)
                numCircuitTracks = 5;
            TB_Sprintf(str, circuitscr_new_circuit_tracks_format, numCircuitTracks * (newCircuitType == 3 ? 4 : 2), gText.text[71]);
            width = width > text_width(str) ? width : text_width(str);
            TB_DrawText(TB_CenterText(str, 160), y, str);
            y += TB_GetFontLineHeight();
            ChangePaneSize(circuitscr_pane, 160 - width / 2 - 4, 125, 160 + width / 2 + 4, y + 4);
        } else if (gStatsEnabled) {
            str = str1;
            TB_SetJust(0, 3);
            HAL_SetFont(11);
            TB_SetTextColor(1);
            if (gCircuitMenuSelect != 1) {
                s16 width;
                if (gContinueEnabled) {
                    TB_Sprintf(str, circuitscr_progress_title_format, gText.names[((u16 *)gText.indices)[44] + gCircuit->b8], gText.text[67]);
                    width = text_width(str) < 20 ? 20 : text_width(str);
                    TB_DrawText(TB_CenterText(str, 160), 129, str);
                    y = 129 + TB_GetFontLineHeight();
                    getFinishedStr(str);
                    width = width > text_width(str) ? width : text_width(str);
                    TB_DrawText(TB_CenterText(str, 160), y, str);
                    y += TB_GetFontLineHeight();
                    getToGoStr(str);
                    width = width > text_width(str) ? width : text_width(str);
                    TB_DrawText(TB_CenterText(str, 160), y, str);
                    y += TB_GetFontLineHeight();
                } else {
                    TB_Sprintf(str, circuitscr_finished_title_format, gText.names[((u16 *)gText.indices)[44] + gCircuit->b8], gText.text[76]);
                    width = text_width(str) < 20 ? 20 : text_width(str);
                    TB_DrawText(TB_CenterText(str, 160), 129, str);
                    y = 129 + TB_GetFontLineHeight();
                }
                y += TB_GetFontLineHeight();
                getPlaceStr(0, str);
                width = width > text_width(str) ? width : text_width(str);
                TB_DrawText(TB_CenterText(str, 160), y, str);
                y += TB_GetFontLineHeight();
                getPointsStr(0, str);
                width = width > text_width(str) ? width : text_width(str);
                TB_DrawText(TB_CenterText(str, 160), y, str);
                y += TB_GetFontLineHeight();
                ChangePaneSize(circuitscr_pane, 160 - width / 2 - 4, 125, 160 + width / 2 + 4, y + 4);
            } else {
                TB_SetJust(0, 3);
                drawReview();
                if (circuitscr_pane)
                    ChangePaneSize(circuitscr_pane, 12, 125, 308, 205);
            }
        } else
            ChangePaneSize(circuitscr_pane, -20, 100, -10, 120);
    }
    TB_SetJust(0, 3);
    HAL_SetTextDepth(-1.0f);
}

s32 circuitscr_animText(Blit *blit)
{
    if (circuitscr_buttonsInited == 1) {
        circuitscr_animButtonText();
        return 1;
    }
    return 1;
}

void getFinishPlaceStr(s32 p, u8 *str)
{
    TB_Strcpy(str, gText.text[78]);
    TB_Strcat(str, gText.names[gText.indices->name_base + gCircuitCars[p].rank]);
    TB_Strcat(str, circuitscr_finish_place_separator);
    TB_Strcat(str, gText.text[129]);
}


VIEW(tMenuButton[12], circuitscr_buttonInfo);
#define circuitscr_buttonInfo circuitscr_buttonInfo_view

extern void *memcpy(void *,const void *,u32);

VIEW(Color, circuitscr_init_button_color);
#define circuitscr_init_button_color circuitscr_init_button_color_view

void circuitscr_initButtons(void)
{
    Color color = circuitscr_init_button_color;
    s32 i;
    s16 texture_index;
    f32 yaw;
    for (i = 0; i < 12; i++) {
        if (circuitscr_buttonInfo[i].handle == -1) {
            memcpy(circuitscr_buttonInfo[i].matrix, unitmatrix, 36);
            circuitscr_buttonInfo[i].handle = MBOX_NewObject(
                MBOX_FindObject_WORK(circuitscr_buttonNames[circuitscr_buttonInfo[i].other], 0, gNumModels - 1, 1),
                (uintptr_t)circuitscr_buttonInfo[i].matrix, -1,
                (circuitscr_buttonInfo[i].other == 0 || circuitscr_buttonInfo[i].other == 1 || circuitscr_buttonInfo[i].other == 2) ? 0x42000 : 0);
            ZOID_SetColor((s16)circuitscr_buttonInfo[i].handle, (s32 *)&color);
        }
    }
    ZOID_SetTexture((s16)circuitscr_buttonInfo[6].handle,
        (intptr_t)MBOX_FindTexture_WORK(circuitscr_title_texture_name, &texture_index, 0, gNumModels - 1, 1), -1);
    memcpy(circuitscr_buttonInfo[6].matrix, unitmatrix, 36);
    yaw = -1.5707963705062866f;
    WPitchUV(yaw, circuitscr_buttonInfo[6].matrix);
    circuitscr_buttonInfo[6].position[0] = 0.0f;
    circuitscr_buttonInfo[6].position[1] = 115.0f;
    circuitscr_buttonInfo[6].position[2] = 200.0f;
    ZOID_SetTexture((s16)circuitscr_buttonInfo[7].handle,
        (intptr_t)MBOX_FindTexture_WORK(circuitscr_cursor_texture_name, &texture_index, 0, gNumModels - 1, 1), -1);
    ZOID_SetTexture((s16)circuitscr_buttonInfo[8].handle,
        (intptr_t)MBOX_FindTexture_WORK(circuitscr_a_button_texture_name, &texture_index, 0, gNumModels - 1, 1), -1);
    memcpy(circuitscr_buttonInfo[8].matrix, unitmatrix, 36);
    WPitchUV(yaw, circuitscr_buttonInfo[8].matrix);

    circuitscr_buttonInfo[8].position[0] = 75.1f;
    circuitscr_buttonInfo[8].position[1] = 55.2f;
    circuitscr_buttonInfo[8].position[2] = 100.0f;
    ScaleUV(circuitscr_buttonInfo[8].matrix, circuitscr_buttonInfo[8].matrix, 0.5f);
    memcpy(circuitscr_buttonInfo[9].matrix, unitmatrix, 36);
    WPitchUV(yaw, circuitscr_buttonInfo[9].matrix);
    WRollUV(3.1415927410125732f, circuitscr_buttonInfo[9].matrix);
    circuitscr_buttonInfo[9].position[0] = 87.1f;
    circuitscr_buttonInfo[9].position[1] = 55.2f;
    circuitscr_buttonInfo[9].position[2] = 100.0f;
    ZOID_SetTexture((s16)circuitscr_buttonInfo[10].handle,
        (intptr_t)MBOX_FindTexture_WORK(circuitscr_b_button_texture_name, &texture_index, 0, gNumModels - 1, 1), -1);
    memcpy(circuitscr_buttonInfo[10].matrix, unitmatrix, 36);
    WPitchUV(yaw, circuitscr_buttonInfo[10].matrix);

    circuitscr_buttonInfo[10].position[0] = -75.1f;
    circuitscr_buttonInfo[10].position[1] = 55.20f;
    circuitscr_buttonInfo[10].position[2] = 100.0f;
    ScaleUV(circuitscr_buttonInfo[10].matrix, circuitscr_buttonInfo[10].matrix, 0.5f);
    memcpy(circuitscr_buttonInfo[11].matrix, unitmatrix, 36);
    WPitchUV(yaw, circuitscr_buttonInfo[11].matrix);
    circuitscr_buttonInfo[11].position[0] = -87.1f;
    circuitscr_buttonInfo[11].position[1] = 55.20f;
    circuitscr_buttonInfo[11].position[2] = 100.0f;
    circuitscr_buttonsInited = 1;
    circuitscr_buttonsJustInited = 1;
    ZeroCameras();
}
