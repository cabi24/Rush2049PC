/* The circuit screen (MAT3 circuitscr.c), GS_CIRCUIT: continue the profile's circuit, review its results race by
 * race, start a new circuit (beginner to extreme, as far as unlocked), or, without a profile, enter a circuit's
 * code; when a circuit has just been finished, its trophy turns on screen first. updateCircuitScreen runs each
 * frame; circuitscr_animText draws its text. */
#include "game/game.h"

/* The circuit in progress (MAT3 tCircuit), part of the profile's save data. Only the fields this file reads. */
typedef struct tCircuit {
    u8 pad0[4];             /* 0x0 checksum */
    u8 active;              /* 0x4 a circuit is under way */
    u8 pad5[3];             /* 0x5 death, temp, difficulty */
    s8 type;                /* 0x8 kCircuitBeginner ... kCircuitExtreme */
    s8 track;               /* 0x9 the races run */
} tCircuit;
/* A profile's save data (MAT3 tSaveData): the circuit at 0x6F4 (MAT3 0x6F8: the N64's records are 4 bytes earlier). */
typedef struct SaveData { u8 records[0x6F4]; tCircuit circuit; } SaveData;

/* The circuit menu's rows (gCircuitMenuSelect, MAT3 enum_kCircuitMenuCount). */
#define kCircuitMenuContinue 0
#define kCircuitMenuStats 1             /* the review: left and right pick the race */
#define kCircuitMenuNew 2               /* left and right pick the type */
#define kCircuitMenuCode 3              /* a guest's only way into a circuit */
#define kCircuitMenuCount 4
#define kCircuitExtreme 3               /* the last circuit type (MAT3 enum_kNumCircuitTypes) */
#define kNumCircuitTypes 4

/* gCircuitScreenExit */
#define CIRCUIT_EXIT_GO 1               /* on to the track screen */
#define CIRCUIT_EXIT_BACK 2             /* back to the mode menu */

/* The code entry's grid (gCodeSelect): a letter of gPasswordAlphabet (32, 12 a row), or the delete button. */
#define CODE_SEL_DELETE (-2)
#define CODE_GRID_COLUMNS 12
#define CODE_LETTERS 32
#define CODE_LENGTH 20

/* circuitscr_buttonInfo[]: the menu's four rows, then these. */
#define CIRCUIT_BUTTON_REVIEW_VALUE 4
#define CIRCUIT_BUTTON_TYPE_VALUE 5
#define CIRCUIT_BUTTON_TITLE 6
#define CIRCUIT_BUTTON_CURSOR 7
#define CIRCUIT_BUTTON_A_ICON 8
#define CIRCUIT_BUTTON_A_BACK 9
#define CIRCUIT_BUTTON_B_ICON 10
#define CIRCUIT_BUTTON_B_BACK 11
#define CIRCUIT_BUTTONS 12

/* gText's tables (gText.tab[]) */
#define TEXT_CIRCUIT_MENU 43            /* the menu's rows */
#define TEXT_CIRCUIT_NAMES 44           /* the four circuit types */

/* Which rows the profile's circuit allows: continue while a circuit is under way and not finished, the review once
 * one is under way. A circuit under way also unlocks its type and the ones before it (the intermediate type and
 * track 4, the advanced, with an Expansion Pak, and track 5, the extreme). An advanced circuit can't be continued
 * without an Expansion Pak. */
void resetEnabled(void)
{
    gContinueEnabled = !(gCircuit->track >= gCircuitSize) && gCircuit->active != 0;
    gStatsEnabled = gCircuit->active;
    if (gCircuit->type > 0) {
        gUnlockedCircuits[0][1] = 1;
        gUnlockedTracks[0][4] = 1;
        gForceUnlockedCircuits[1] = 1;
    }
    if (gCircuit->type >= 2 && expansion_pak_present != 0) {
        gUnlockedCircuits[0][2] = 1;
        gUnlockedTracks[0][5] = 1;
        gForceUnlockedCircuits[2] = 1;
    }
    if (gCircuit->type == kCircuitExtreme) {
        gUnlockedCircuits[0][3] = 1;
        gForceUnlockedCircuits[3] = 1;
    }
    if (expansion_pak_present == 0 && gCircuit->type == 2) {
        gContinueEnabled = 0;
    }
}

VIEW(u8 *, gPasswordAlphabet);
#define gPasswordAlphabet gPasswordAlphabet_view

/* The circuit code entry's input: the d-pad (auto-repeating) moves over the 32 letters (12 a row, the delete button
 * after the last row's 8), A adds a letter or deletes one; the 20th letter checks the code (SetCircuitCode): a bad
 * one or one for another number of players says so, a good one sets the circuit up. Start or B backs out. */
void controlCircuitCode(void)
{
    s32 result;
    if (gActiveAutoEdges & BUTTON_UP) {
        AudPlayEffect(SND_MENU_UP, 0, 1, 0);
        if (gCodeSelect == CODE_SEL_DELETE) gCodeSelect = 21;
        else if (gCodeSelect < 8) gCodeSelect += 24;
        else if (gCodeSelect < 12) gCodeSelect = CODE_SEL_DELETE;
        else gCodeSelect -= CODE_GRID_COLUMNS;
    } else if (gActiveAutoEdges & BUTTON_DOWN) {
        AudPlayEffect(SND_MENU_DOWN, 0, 1, 0);
        if (gCodeSelect == CODE_SEL_DELETE) gCodeSelect = 9;
        else if (gCodeSelect < 20) gCodeSelect += CODE_GRID_COLUMNS;
        else if (gCodeSelect < 24) gCodeSelect = CODE_SEL_DELETE;
        else gCodeSelect -= 24;
    } else if (gActiveAutoEdges & BUTTON_LEFT) {
        AudPlayEffect(SND_MENU_LEFT, 0, 1, 0);
        if (gCodeSelect == CODE_SEL_DELETE) gCodeSelect = CODE_LETTERS - 1;
        else if (gCodeSelect == 24) gCodeSelect = CODE_SEL_DELETE;
        else if (gCodeSelect % 12 == 0) gCodeSelect += 11;
        else gCodeSelect--;
    } else if (gActiveAutoEdges & BUTTON_RIGHT) {
        AudPlayEffect(SND_MENU_RIGHT, 0, 1, 0);
        if (gCodeSelect == CODE_SEL_DELETE) gCodeSelect = 24;
        else if (gCodeSelect == CODE_LETTERS - 1) gCodeSelect = CODE_SEL_DELETE;
        else if (gCodeSelect % 12 == 11) gCodeSelect -= 11;
        else gCodeSelect++;
    }
    if (gActiveEdges & BUTTON_A) {
        AudPlayEffect(SND_MENU_A, 0, 1, 0);
        if (gCodeSelect == CODE_SEL_DELETE) {
            if (gCodeLen > 0) {
                gCodeLen--;
                circuitscr_gCircuitCode[gCodeLen] = '.';
                gInvalidCode = 0;
                gWrongNumPlayers = 0;
            }
        } else if (gCodeLen < CODE_LENGTH) {
            circuitscr_gCircuitCode[gCodeLen] = gPasswordAlphabet[gCodeSelect];
            gCodeLen++;
            if (gCodeLen == CODE_LENGTH) {
                result = SetCircuitCode(circuitscr_gCircuitCode);
                if (result == 0) gInvalidCode = 1;
                else if (result == -1) gWrongNumPlayers = 1;
                else {
                    gEnterCircuitCode = 0;
                    resetEnabled();
                    circuitscr_buttonsJustInited = 1;
                    gReviewTrack = gCircuit->track;
                }
            }
        }
    } else if (gActiveEdges & (BUTTON_START | BUTTON_B)) {
        PlayExitSound(gActiveEdges);
        gEnterCircuitCode = 0;
        circuitscr_buttonsJustInited = 1;
    }
}

/* A pak file record (MAT3 tPS_File) in its PC layout (pc_prelude.h PSF_OFS, docs/port/POINTERS.md 9.10): pak is
 * the pak it is on. */
typedef struct Item { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 pak; } Item;
typedef struct Selection { Item *file; } Selection;
typedef struct Resource { u8 *bytes; } Resource;
typedef struct MenuOwner MenuOwner;
/* A profile (MAT3 tPlayerProfile) in its PC layout, 64 bytes (pc_prelude.h PROFILE_OFS): the links of
 * gPlayerFiles, its pak file (NULL for a guest), its id, its name, its save data. */
typedef struct OwnerBody {
    MenuOwner *next, *previous;
    Selection *fileh;
    u8 id[8];
    u8 name[24];            /* 0x20 (N64 0x14) */
    Resource *datah;        /* 0x38 */
} OwnerBody;
/* A profile handle (MAT3 tPlayerProfile **). */
struct MenuOwner { OwnerBody *body; };
/* A player's pad record (MAT3 tPadState, gPlayers): only its profile. */
typedef struct Player { u8 other[72]; MenuOwner *owner; } Player;
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



extern void particle_lifetime_set(void);

/* Set the screen up: the players' race type is saved as circuit; a circuit that is active but has run no race is
 * dropped; an active one is continued (ContinueCircuit sets the race up), and if it is finished the trophy for the
 * first player's place (on the podium only) is made, turning in front of the camera. Then the menu, the review
 * and progress box's size, the buttons, the box and the text blit. */
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
    if (gCircuit->active && gCircuit->track == 0)
        gCircuit->active = 0;
    gReviewTrack = gCircuit->track;
    if (gCircuit->active) {
        ContinueCircuit();
        if (gCircuit->track >= gCircuitSize) {
            selected = (s8) gCircuitCars[0].totalPlace;
            gShowTrophy = 1;
            if (selected < 3) {
                model = MBOX_FindObject_WORK(gTrophyNames[selected], 0, gNumModels[0] - 1, 1);
                trophyObjIdx = MBOX_NewObject(model, gTrophyMat, -1, 0);
                model = MBOX_FindObject_WORK(gTrophySheenNames[selected], 0, gNumModels[0] - 1, 1);
                trophySheenObjIdx = MBOX_NewObject(model, unitmatrix, trophyObjIdx, 0x2C6084);
                ZOID_SetTexture((s16)trophySheenObjIdx,
                    MBOX_FindTexture_WORK(circuitscr_sheen_texture_name, &texture_index, 0, gNumModels[0] - 1, 1), -1);
                /* The sheen's scale over the trophy's: the node's w, which the scene draw scales its matrix by
                 * (ZOID_AddObject makes it 1; MAT3 has no such line). The N64 stores it at +0x10 of the node's
                 * 0x44-byte record in ObjInstanceList (the decomp wrote it through a label 0x10 before it, read as
                 * records of 0x44 bytes with the float at +0x20). */
                ObjInstanceList[trophySheenObjIdx].w = circuitscr_trophy_scale;
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

/* Is the menu row `mode` offered? The code entry only to a guest (no pak file), continue and the review only when
 * resetEnabled allows them. */
s32 menuItemValid(s32 mode)
{
    return (mode != kCircuitMenuCode || gPlayers[gCircuitPlayer].owner->body->fileh == 0)
        && (!(mode == kCircuitMenuContinue) || gContinueEnabled != 0)
        && (!(mode == kCircuitMenuStats) || gStatsEnabled != 0);
}

/* The screen's frame (GS_CIRCUIT). gCircuit is the circuit player's circuit. After a race (gSkipToGameState) it goes
 * straight on to the track screen. Then: the trophy, the code entry, or the menu: Start continues (or with nothing
 * to continue goes to "new" as A would), A takes the row (continue; a new circuit, asked first when one is under
 * way: left and right flip yes and no; the code entry), B backs out of the question or to the mode menu, up and
 * down move between the offered rows, left and right change the review's race or the new circuit's type (through
 * the unlocked ones). A pulled pak sends the players back to the player screen, a soft reset to the title. */
void updateCircuitScreen(void)
{
    s32 i;
    gCircuit = &((SaveData *)gPlayers[gCircuitPlayer].owner->body->datah->bytes)->circuit;
    if (gSkipToGameState != SKIP_NONE) {
        if (gSkipToGameState != gGameState) {
            setGameState(GS_TRACK);
            return;
        }
        gSkipToGameState = SKIP_NONE;
    }
    if (!circuitscr_inited)
        circuitscr_initScreen();
    while (!menuItemValid(gCircuitMenuSelect)) {
        gCircuitMenuSelect++;
        if (gCircuitMenuSelect >= kCircuitMenuCount)
            gCircuitMenuSelect = 0;
    }
    if (gShowTrophy) controlShowTrophy(); else if (gEnterCircuitCode) {
        controlCircuitCode();
    } else if (gActiveEdges & BUTTON_START) {
        AudPlayEffect(SND_MENU_START, 0, 1, 0);
        if (gConfirmNewCircuit) {
            gConfirmNewCircuit = 0;
            circuitscr_gYes = 0;
            circuitscr_buttonsJustInited = 1;
        } else if (!menuItemValid(kCircuitMenuContinue)) {
            gCircuitMenuSelect = kCircuitMenuNew;
            goto confirm_option;        /* as if A had been pressed on "new" */
        } else {
            ContinueCircuit();
            gCircuitScreenExit = CIRCUIT_EXIT_GO;
        }
    } else if (gActiveEdges & BUTTON_A) {
        AudPlayEffect(SND_MENU_A, 0, 1, 0);
 confirm_option:
        switch (gCircuitMenuSelect) {
        case kCircuitMenuContinue:
            ContinueCircuit();
            gCircuitScreenExit = CIRCUIT_EXIT_GO;
            break;
        case kCircuitMenuStats:
            break;
        case kCircuitMenuCode:
            gEnterCircuitCode = 1;
            gCodeLen = 0;
            gCodeSelect = gCodeLen;
            TB_Strcpy(circuitscr_gCircuitCode, circuitscr_empty_code_text);
            gInvalidCode = 0;
            gWrongNumPlayers = 0;
            break;
        case kCircuitMenuNew:
            if (circuitscr_gYes || !gContinueEnabled) {
                NewCircuit(newCircuitType);
                ContinueCircuit();
                gCircuitScreenExit = CIRCUIT_EXIT_GO;
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
    } else if (gActiveEdges & BUTTON_B) {
        AudPlayEffect(SND_MENU_BACK, 0, 1, 0);
        if (gConfirmNewCircuit) {
            gConfirmNewCircuit = 0;
            circuitscr_gYes = 0;
            circuitscr_buttonsJustInited = 1;
        } else {
            gCircuitScreenExit = CIRCUIT_EXIT_BACK;
        }
    } else if (gActiveEdges & BUTTON_UP) {
        if (!gConfirmNewCircuit) {
            PlayUDSound(gActiveEdges);
            do {
                gCircuitMenuSelect--;
                if (gCircuitMenuSelect < 0)
                    gCircuitMenuSelect = kCircuitMenuCount - 1;
            } while (!menuItemValid(gCircuitMenuSelect));
        }
    } else if (gActiveEdges & BUTTON_DOWN) {
        if (!gConfirmNewCircuit) {
            PlayUDSound(gActiveEdges);
            do {
                gCircuitMenuSelect++;
                if (gCircuitMenuSelect >= kCircuitMenuCount)
                    gCircuitMenuSelect = 0;
            } while (!menuItemValid(gCircuitMenuSelect));
        }
    } else if (gActiveEdges & (BUTTON_LEFT | BUTTON_RIGHT)) {
        if (gConfirmNewCircuit) {
            PlayLRSound(gActiveEdges);
            circuitscr_gYes = !circuitscr_gYes;
        } else if (gCircuitMenuSelect == kCircuitMenuNew) {
            PlayLRSound(gActiveEdges);
            do {
                if (gActiveEdges & BUTTON_LEFT)
                    newCircuitType--;
                else if (gActiveEdges & BUTTON_RIGHT)
                    newCircuitType++;
                if (newCircuitType < 0)
                    newCircuitType = kNumCircuitTypes - 1;
                if (newCircuitType >= kNumCircuitTypes)
                    newCircuitType = 0;
            } while (!IsCircuitUnlocked(newCircuitType));
        } else if (gCircuitMenuSelect == kCircuitMenuStats) {
            PlayLRSound(gActiveEdges);
            if (gActiveEdges & BUTTON_LEFT)
                gReviewTrack--;
            else if (gActiveEdges & BUTTON_RIGHT)
                gReviewTrack++;
            if (gReviewTrack < 0)
                gReviewTrack = gCircuit->track;
            if (gReviewTrack > gCircuit->track)
                gReviewTrack = 0;
        }
    }
    for (i = 0; i < gNumPlayers; i++) {
        if (gPlayers[i].owner->body->fileh) {
            if (PS_PakChanged(gPlayers[i].owner->body->fileh->file->pak)) {
                circuitscr_uninitScreen();
                setGameState(GS_PLAYER);
                return;
            }
        }
    }
    if (gSoftReset) {
        circuitscr_uninitScreen();
        setGameState(GS_TITLE);
    } else if (gCircuitScreenExit) {
        circuitscr_uninitScreen();
        if (gCircuitScreenExit == CIRCUIT_EXIT_GO) {
            switch (gCircuitMenuSelect) {
            case kCircuitMenuContinue: setGameState(GS_TRACK); break;
            case kCircuitMenuNew: setGameState(GS_TRACK); break;
            }
        } else if (gCircuitScreenExit == CIRCUIT_EXIT_BACK) {
            setGameState(GS_MODE);
        }
        gCircuitScreenExit = 0;
    }
    circuitscr_updateButtons();
}

/* The finished circuit's trophy (MAT3 circuitscr.c controlShowTrophy): it turns 90 degrees a second; Start, A or B
 * puts it away and shows the menu. */
void controlShowTrophy(void)
{
    YawUV(-90.0f * gFrameSeconds * circuitscr_degrees_to_radians, (f32 (*)[3])gTrophyMat);
    if (gActiveEdges & (BUTTON_ACCEPT | BUTTON_B)) {
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

/* RemovePane, under the stale name image A's symbol table still has for it (symbol_addrs.us.txt) */
extern void sound_handles_array_clear(s8 *);

/* Take the screen down: its buttons, its text blit, its box, the trophy and the background. */
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

/* Remove the buttons (3D models) from the scene and mark them gone (handle -1); the scene objects are removed only
 * in the front end. */
void circuitscr_uninitButtons(void)
{
    s32 i;
    for (i = 0; i < CIRCUIT_BUTTONS; i++) {
        if (circuitscr_buttonInfo[i].handle != -1) {
            if (gGameState & GS_FRONTEND_MASK)
                ZOID_RemoveObject((s16)circuitscr_buttonInfo[i].handle);
            circuitscr_buttonInfo[i].handle = -1;
        }
    }
    circuitscr_buttonsInited = 0;
}

typedef union Color {u32 word;u8 channel[4];} Color;
VIEW(Color, circuitscr_button_color);
#define circuitscr_button_color circuitscr_button_color_view

extern void *memcpy(void *,const void *,u32);
extern f32 fabsf(f32);
#pragma intrinsic(fabsf)

/* Lay the buttons out for this frame. With the trophy up only the title and the A and B icons show. Otherwise each
 * of the four rows' buttons turns to its selected face when selected, the list scrolls to the selection (at most
 * 540 a second) and fades over a button's height past the top or bottom; rows not offered are hidden, and with the
 * code entry or the "new circuit" question up all are; the review and new rows have their value buttons; the
 * cursor spins beside the selected row and eases towards it. */
void circuitscr_updateButtons(void)
{
    static f32 cursorSpeed;
    static f32 scrollSpeed;
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
        for (buttonIdx = 0; buttonIdx < CIRCUIT_BUTTONS; buttonIdx++) {
            if (buttonIdx == CIRCUIT_BUTTON_TITLE || buttonIdx == CIRCUIT_BUTTON_A_ICON || buttonIdx == CIRCUIT_BUTTON_B_ICON)
                MBOX_ShowObject(circuitscr_buttonInfo[buttonIdx].handle, 0, 15);
            else {
                circuitscr_buttonInfo[buttonIdx].alpha = 0;
                MBOX_HideObject(circuitscr_buttonInfo[buttonIdx].handle, 0, 15);
            }
        }
        return;
    }
    for (buttonIdx = 0; buttonIdx < CIRCUIT_BUTTONS; buttonIdx++)
        MBOX_ShowObject(circuitscr_buttonInfo[buttonIdx].handle, 0, 15);
    buttonY = circuitscr_maxY;
    for (buttonIdx = 0; buttonIdx < kCircuitMenuCount; buttonIdx++) {
        if (gEnterCircuitCode || gConfirmNewCircuit) {
            MBOX_HideObject(circuitscr_buttonInfo[buttonIdx].handle, 0, 15);
            if (buttonIdx == kCircuitMenuStats)
                MBOX_HideObject(circuitscr_buttonInfo[CIRCUIT_BUTTON_REVIEW_VALUE].handle, 0, 15);
            else if (buttonIdx == kCircuitMenuNew)
                MBOX_HideObject(circuitscr_buttonInfo[CIRCUIT_BUTTON_TYPE_VALUE].handle, 0, 15);
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
            ScaleUV(unitmatrix, circuitscr_buttonInfo[buttonIdx].matrix, circuitscr_gButtonScale);
            WPitchUV(circuitscr_buttonInfo[buttonIdx].angle - 1.5707964f, circuitscr_buttonInfo[buttonIdx].matrix);
            if (circuitscr_buttonInfo[buttonIdx].angle > 1.5707964f)
                tex = MBOX_FindTexture_WORK(circuitscr_select_texture_name, &texIdx, 0, gNumModels[0] - 1, 1);
            else
                tex = MBOX_FindTexture_WORK(circuitscr_button_texture_name, &texIdx, 0, gNumModels[0] - 1, 1);
            ZOID_SetTexture(circuitscr_buttonInfo[buttonIdx].handle, tex, -1);
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
            if (buttonIdx == kCircuitMenuStats) {
                circuitscr_buttonInfo[CIRCUIT_BUTTON_REVIEW_VALUE].angle = circuitscr_buttonInfo[buttonIdx].angle;
                circuitscr_buttonInfo[CIRCUIT_BUTTON_REVIEW_VALUE].position[0] = circuitscr_buttonX + 100.0f * circuitscr_gButtonScale;
                circuitscr_buttonInfo[CIRCUIT_BUTTON_REVIEW_VALUE].position[1] = circuitscr_buttonInfo[buttonIdx].position[1];
                circuitscr_buttonInfo[CIRCUIT_BUTTON_REVIEW_VALUE].position[2] = circuitscr_buttonInfo[buttonIdx].position[2];
                ScaleUV(unitmatrix, circuitscr_buttonInfo[CIRCUIT_BUTTON_REVIEW_VALUE].matrix, circuitscr_gButtonScale);
                WPitchUV(circuitscr_buttonInfo[CIRCUIT_BUTTON_REVIEW_VALUE].angle - 1.5707964f, circuitscr_buttonInfo[CIRCUIT_BUTTON_REVIEW_VALUE].matrix);
                ZOID_SetTexture(circuitscr_buttonInfo[CIRCUIT_BUTTON_REVIEW_VALUE].handle, tex, -1);
                circuitscr_buttonInfo[CIRCUIT_BUTTON_REVIEW_VALUE].alpha = circuitscr_buttonInfo[buttonIdx].alpha;
                ZOID_SetColor(circuitscr_buttonInfo[CIRCUIT_BUTTON_REVIEW_VALUE].handle, (s32 *)&buttonColor);
                if (!menuItemValid(buttonIdx))
                    MBOX_HideObject(circuitscr_buttonInfo[CIRCUIT_BUTTON_REVIEW_VALUE].handle, 0, 15);
                else if (!circuitscr_buttonInfo[buttonIdx].alpha)
                    MBOX_HideObject(circuitscr_buttonInfo[CIRCUIT_BUTTON_REVIEW_VALUE].handle, 0, 15);
                else
                    MBOX_ShowObject(circuitscr_buttonInfo[CIRCUIT_BUTTON_REVIEW_VALUE].handle, 0, 15);
            } else if (buttonIdx == kCircuitMenuNew) {
                circuitscr_buttonInfo[CIRCUIT_BUTTON_TYPE_VALUE].angle = circuitscr_buttonInfo[buttonIdx].angle;
                circuitscr_buttonInfo[CIRCUIT_BUTTON_TYPE_VALUE].position[0] = circuitscr_buttonX + 100.0f * circuitscr_gButtonScale;
                circuitscr_buttonInfo[CIRCUIT_BUTTON_TYPE_VALUE].position[1] = circuitscr_buttonInfo[buttonIdx].position[1];
                circuitscr_buttonInfo[CIRCUIT_BUTTON_TYPE_VALUE].position[2] = circuitscr_buttonInfo[buttonIdx].position[2];
                ScaleUV(unitmatrix, circuitscr_buttonInfo[CIRCUIT_BUTTON_TYPE_VALUE].matrix, circuitscr_gButtonScale);
                WPitchUV(circuitscr_buttonInfo[CIRCUIT_BUTTON_TYPE_VALUE].angle - 1.5707964f, circuitscr_buttonInfo[CIRCUIT_BUTTON_TYPE_VALUE].matrix);
                ZOID_SetTexture(circuitscr_buttonInfo[CIRCUIT_BUTTON_TYPE_VALUE].handle, tex, -1);
                circuitscr_buttonInfo[CIRCUIT_BUTTON_TYPE_VALUE].alpha = circuitscr_buttonInfo[buttonIdx].alpha;
                ZOID_SetColor(circuitscr_buttonInfo[CIRCUIT_BUTTON_TYPE_VALUE].handle, (s32 *)&buttonColor);
                if (!menuItemValid(buttonIdx))
                    MBOX_HideObject(circuitscr_buttonInfo[CIRCUIT_BUTTON_TYPE_VALUE].handle, 0, 15);
                else if (!circuitscr_buttonInfo[buttonIdx].alpha)
                    MBOX_HideObject(circuitscr_buttonInfo[CIRCUIT_BUTTON_TYPE_VALUE].handle, 0, 15);
                else
                    MBOX_ShowObject(circuitscr_buttonInfo[CIRCUIT_BUTTON_TYPE_VALUE].handle, 0, 15);
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
    memcpy(prevPos, circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].position, 12);
    circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].angle += 6.2831855f * gFrameSeconds;
    if (circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].angle > 6.2831855f)
        circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].angle -= 6.2831855f;
    circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].position[0] = circuitscr_buttonX * circuitscr_cursorZ / circuitscr_buttonZ + -140.0f * circuitscr_gButtonScale;
    circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].position[1] = selectedPos * circuitscr_cursorZ / circuitscr_buttonZ;
    circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].position[2] = circuitscr_cursorZ;
    if (!circuitscr_buttonsJustInited) {
        if (cursorSpeed < fabsf(circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].position[1] - prevPos[1]) * 60.0f / 10.0f) {
            cursorSpeed = fabsf(circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].position[1] - prevPos[1]) * 60.0f / 10.0f;
            cursorSpeed = cursorSpeed < 540.0f * circuitscr_gButtonScale ? cursorSpeed : 540.0f * circuitscr_gButtonScale;
            cursorSpeed = cursorSpeed > 180.0f * circuitscr_gButtonScale ? cursorSpeed : 180.0f * circuitscr_gButtonScale;
        }
        if (circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].position[1] > prevPos[1] + cursorSpeed * gFrameSeconds)
            circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].position[1] = prevPos[1] + cursorSpeed * gFrameSeconds;
        else if (circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].position[1] < prevPos[1] - cursorSpeed * gFrameSeconds)
            circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].position[1] = prevPos[1] - cursorSpeed * gFrameSeconds;
        else
            cursorSpeed = 180.0f * circuitscr_gButtonScale;
    }
    ScaleUV(unitmatrix, circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].matrix, circuitscr_gButtonScale * 0.4f);
    WRollUV(circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].angle, circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].matrix);
    WPitchUV(1.5707964f, circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].matrix);
    if (gEnterCircuitCode || gConfirmNewCircuit) {
        MBOX_HideObject(circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].handle, 0, 15);
        MBOX_HideObject(circuitscr_buttonInfo[CIRCUIT_BUTTON_A_BACK].handle, 0, 15);
    } else {
        MBOX_ShowObject(circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].handle, 0, 15);
        MBOX_ShowObject(circuitscr_buttonInfo[CIRCUIT_BUTTON_A_BACK].handle, 0, 15);
    }
    if (clearScrollSpeed == 1)
        scrollSpeed = 0.0f;
    circuitscr_buttonsJustInited = 0;
}

/* Player's circuit points as text ("points: n ..."), into destination. */
void getPointsStr(s32 player,u8 *destination)
{
    u8 number[32];
    TB_Strcpy(destination,gText.single[80]);
    TB_Sprintf(number,circuitscr_points_format,gCircuitCars[player].totalPoints);
    TB_Strcat(destination,number);
    TB_Strcat(destination,gText.single[130]);
}

/* Player's place over the circuit so far as text ("place: 1st ..."), into destination. */
void getPlaceStr(s32 player,u8 *destination)
{
    TB_Strcpy(destination,gText.single[77]);
    TB_Strcat(destination,gText.taba[gCircuitCars[player].totalPlace + gText.tab[TEXT_PLACES]]);
    TB_Strcat(destination,circuitscr_place_separator);
    TB_Strcat(destination,gText.single[129]);
}

/* The races the circuit has left as text ("n race(s) to go"), into destination. */
void getToGoStr(u8 *destination)
{
    u8 number[32];
    TB_Strcpy(destination,gText.single[72]);
    TB_Sprintf(number,circuitscr_to_go_count_format,gCircuitSize - gCircuit->track);
    TB_Strcat(destination,number);
    TB_Strcat(destination,gCircuitSize - gCircuit->track != 1 ? gText.single[71] : gText.single[70]);
    TB_Strcat(destination,gText.single[73]);
}

/* The races the circuit has run as text ("n race(s) finished"), into destination. */
void getFinishedStr(u8 *destination)
{
    u8 number[32];
    TB_Strcpy(destination,gText.single[74]);
    TB_Sprintf(number,circuitscr_finished_count_format,gCircuit->track);
    TB_Strcat(destination,number);
    TB_Strcat(destination,gCircuit->track != 1 ? gText.single[71] : gText.single[70]);
    TB_Strcat(destination,gText.single[75]);
}

/* Player's profile name, into destination (MAT3 circuitscr.c getPlayerStr). */
void getPlayerStr(s32 player,u8 *destination)
{
    TB_Strcpy(destination,gPlayers[player].owner->body->name);
}

/* The progress box's size and place (gProgressWidth, gProgressHeight, gProgressTop): wide enough for the circuit's
 * progress lines and, for the circuit player (and the other player with two), the name, place and points lines;
 * centred between the menu's bottom and line 220. */
void calcProgressSize(void)
{
    u8 text[80];
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
            width = HAL_GetTextWidth(gText.single[67],-1);
            gProgressWidth = width < gProgressWidth ? gProgressWidth : width;
            getFinishedStr(text);
            width = HAL_GetTextWidth(text,-1);
            gProgressWidth = width < gProgressWidth ? gProgressWidth : width;
            getToGoStr(text);
            width = HAL_GetTextWidth(text,-1);
            gProgressWidth = width < gProgressWidth ? gProgressWidth : width;
        } else {
            gProgressHeight += 15;
            width = HAL_GetTextWidth(gText.single[76],-1);
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

VIEW(u8 *[], gDefaultNames);
#define gDefaultNames gDefaultNames_view

/* The review: the six cars' places, names and points, in two columns of three by place, the players' in their
 * colour, over the whole circuit so far (the review on its last race) or for the reviewed race (a race a car didn't
 * run: a line of text instead, and no more). */
void drawReview(void)
{
    u8 number[64],name[32];
    s32 i, rank, x, y;
    s32 count;
    HAL_SetFont(10);
    TB_SetTextColor(kTB_WhiteText);
    HAL_GetFontAscent();
    count=6;
    for(i=0;i<count;i++) {
        if(gReviewTrack==gCircuit->track)rank=gCircuitCars[i].totalPlace;
        else {
            rank=gCircuitCars[i].places[gReviewTrack];
            if(rank>=6) {
                HAL_SetFont(10);
                TB_SetTextColor(kTB_WhiteText);
                TB_DrawText(TB_CenterText(gText.single[81],160),165,gText.single[81]);
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
        if(i<gNumPlayers)TB_Strcpy(name,gPlayers[i].owner->body->name);
        else TB_Strcpy(name,gDefaultNames[gCircuitCars[i].name]);
        TB_DrawEllipseText((s16)(x+15),y+2,120,name);
        TB_Sprintf(number,circuitscr_review_points_format,gReviewTrack==gCircuit->track ?
                     gCircuitCars[i].totalPoints : gCircuitCars[i].points[gReviewTrack]);
        TB_Strcat(number,gText.single[130]);
        TB_DrawText((s16)(x+15),y+11,number);
    }
}

/* A line's width in the current font, as MAT3's HAL_GetTextWidth returns it: an unsigned int (the panes below
 * compare it unsigned). */
static u32 text_width(u8 *str) { return HAL_GetTextWidth(str, -1); }

/* The screen's text over its buttons: the title; then the code entry (the code so far, the letter grid with the
 * selection flashing, an error line), or the "new circuit" question (yes / no), or each row's name beside its
 * button with the review's race and the new circuit's type, and under the menu the trophy's text, the new
 * circuit's track count, the circuit's progress, or the review; each in the box, sized to it. */
void circuitscr_animButtonText(void)
{
    s32 i;
    u8 str1[192];
    u8 *str;
    s16 screenPos[2];
    s32 x;
    s32 y;

    HAL_SetTextDepth(0.0f);
    TB_SetJust(kTB_JustLeft, kTB_JustCenter);
    ZOID_ProjectWindow(0, circuitscr_buttonInfo[CIRCUIT_BUTTON_TITLE].position, &gCamera[0], 0, screenPos);
    x = screenPos[0];
    y = screenPos[1];
    HAL_SetFont(13);
    TB_SetTextColor(kTB_WhiteText);
    str = gText.single[42];
    TB_DrawText(TB_CenterText(str, x), y, str);
    if (gEnterCircuitCode) {
        static u8 *del = circuitscr_del_text;
        TB_SetJust(kTB_JustLeft, kTB_JustTop);
        HAL_SetFont(11);
        y = 59;
        x = 115;
        TB_SetTextColor(kTB_WhiteText);
        TB_DrawText(TB_CenterText(gText.single[44], 160), y, gText.single[44]);
        y += 16;
        if (gInvalidCode || gWrongNumPlayers) {
            TB_SetTextColor(5);
            TB_DrawText(TB_CenterText(gText.single[134], 160), y, gText.single[134]);
        }
        y += 16;
        TB_SetTextColor(12);
        str1[1] = 0;
        for (i = 0; i < CODE_LENGTH; i++) {
            str1[0] = circuitscr_gCircuitCode[i];
            TB_DrawText(TB_CenterText(str1, x + 4), y, str1);
            if ((i + 1) % 10 == 0) {
                x = 115;
                y += 9;
            } else
                x += 9;
        }
        TB_SetTextColor(kTB_WhiteText);
        x = 94;
        y = 126;
        for (i = 0; i < CODE_LETTERS; i++) {
            str1[0] = gPasswordAlphabet[i];
            TB_SetTextColor(gCodeSelect == i ? kTB_FlashText : kTB_WhiteText);
            TB_DrawText(TB_CenterText(str1, x + 5), y, str1);
            if ((i + 1) % 12 == 0) {
                x = 94;
                y += 11;
            } else
                x += 11;
        }
        x += 12;
        TB_SetTextColor(gCodeSelect == CODE_SEL_DELETE ? kTB_FlashText : kTB_WhiteText);
        TB_DrawText(TB_CenterText(del, x + 5), y, del);
        if (circuitscr_pane)
            ChangePaneSize(circuitscr_pane, 80, 55, 240, 165);
    } else if (gConfirmNewCircuit) {
        TB_SetJust(kTB_JustCenter, kTB_JustTop);
        HAL_SetFont(11);
        TB_SetTextColor(kTB_WhiteText);
        TB_DrawTextScrollBox(160, 59, 140, 78, -1, 0, gText.single[96]);
        y = 59 + 5 * TB_GetFontLineHeight();
        TB_SetTextColor(circuitscr_gYes ? kTB_FlashText : kTB_WhiteText);
        TB_DrawText(145, y, gText.taba[gText.tab[TEXT_YES_NO]]);
        TB_SetTextColor(circuitscr_gYes ? kTB_WhiteText : kTB_FlashText);
        TB_DrawText(175, y, gText.taba[gText.tab[TEXT_YES_NO] + 1]);
        TB_SetJust(kTB_JustLeft, kTB_JustTop);
        if (circuitscr_pane)
            ChangePaneSize(circuitscr_pane, 60, 55, 260, 130);
    } else {
        HAL_SetFont(11);
        for (i = 0; i < kCircuitMenuCount; i++) {
            if (circuitscr_buttonInfo[i].angle > 0.6981317f && circuitscr_buttonInfo[i].angle < 2.443461f)
                continue;
            if (!circuitscr_buttonInfo[i].alpha)
                continue;
            HAL_SetTextTranslucency(circuitscr_buttonInfo[i].alpha);
            ZOID_ProjectWindow(0, circuitscr_buttonInfo[i].position, &gCamera[0], 0, screenPos);
            x = screenPos[0] - 35.0f * circuitscr_gButtonScale;
            y = screenPos[1];
            if (circuitscr_buttonInfo[i].angle < 1.5707964f) {
                TB_FlashTextColors(flashColors[0], flashColors[1]);
                TB_SetTextColor(kTB_WhiteText);
            } else {
                TB_FlashTextColors(flashColors[0], flashColors[1]);
                TB_SetTextColor(kTB_FlashText);
            }
            str = gText.taba[gText.tab[TEXT_CIRCUIT_MENU] + i];
            TB_DrawText(x, y, str);
            if (i == kCircuitMenuStats) {
                ZOID_ProjectWindow(0, circuitscr_buttonInfo[CIRCUIT_BUTTON_REVIEW_VALUE].position, &gCamera[0], 0, screenPos);
                x = screenPos[0] - 35.0f * circuitscr_gButtonScale;
                y = screenPos[1];
                if (gReviewTrack == gCircuit->track)
                    str = gText.single[176];
                else {
                    TB_Sprintf(str1, circuitscr_review_track_format, gText.single[82], gReviewTrack + 1);
                    str = str1;
                }
                TB_DrawText(x, y, str);
            }
            if (i == kCircuitMenuNew) {
                ZOID_ProjectWindow(0, circuitscr_buttonInfo[CIRCUIT_BUTTON_TYPE_VALUE].position, &gCamera[0], 0, screenPos);
                x = screenPos[0] - 35.0f * circuitscr_gButtonScale;
                y = screenPos[1];
                str = gText.taba[gText.tab[TEXT_CIRCUIT_NAMES] + newCircuitType];
                TB_DrawText(x, y, str);
            }
        }
        HAL_SetTextTranslucency(-1.0f);
        if (gShowTrophy) {
            s16 width;
            str = str1;
            TB_SetJust(kTB_JustLeft, kTB_JustTop);
            HAL_SetFont(11);
            TB_SetTextColor(kTB_WhiteText);
            TB_Sprintf(str, circuitscr_trophy_title_format, gText.taba[gText.tab[TEXT_CIRCUIT_NAMES] + gCircuit->type], gText.single[76]);
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
        } else if (gCircuitMenuSelect == kCircuitMenuNew) {
            s32 numCircuitTracks;
            s16 width;
            str = str1;
            TB_SetJust(kTB_JustLeft, kTB_JustTop);
            HAL_SetFont(11);
            TB_SetTextColor(kTB_WhiteText);
            TB_Sprintf(str, circuitscr_new_circuit_title_format, gText.taba[gText.tab[TEXT_CIRCUIT_NAMES] + newCircuitType], gText.single[42]);
            width = text_width(str) < 20 ? 20 : text_width(str);
            TB_DrawText(TB_CenterText(str, 160), 129, str);
            y = 129 + TB_GetFontLineHeight();
            numCircuitTracks = gNumCircuitTracks[newCircuitType];
            if (!expansion_pak_present && numCircuitTracks >= 5)
                numCircuitTracks = 5;
            TB_Sprintf(str, circuitscr_new_circuit_tracks_format, numCircuitTracks * (newCircuitType == kCircuitExtreme ? 4 : 2), gText.single[71]);
            width = width > text_width(str) ? width : text_width(str);
            TB_DrawText(TB_CenterText(str, 160), y, str);
            y += TB_GetFontLineHeight();
            ChangePaneSize(circuitscr_pane, 160 - width / 2 - 4, 125, 160 + width / 2 + 4, y + 4);
        } else if (gStatsEnabled) {
            str = str1;
            TB_SetJust(kTB_JustLeft, kTB_JustTop);
            HAL_SetFont(11);
            TB_SetTextColor(kTB_WhiteText);
            if (gCircuitMenuSelect != kCircuitMenuStats) {
                s16 width;
                if (gContinueEnabled) {
                    TB_Sprintf(str, circuitscr_progress_title_format, gText.taba[gText.tab[TEXT_CIRCUIT_NAMES] + gCircuit->type], gText.single[67]);
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
                    TB_Sprintf(str, circuitscr_finished_title_format, gText.taba[gText.tab[TEXT_CIRCUIT_NAMES] + gCircuit->type], gText.single[76]);
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
                TB_SetJust(kTB_JustLeft, kTB_JustTop);
                drawReview();
                if (circuitscr_pane)
                    ChangePaneSize(circuitscr_pane, 12, 125, 308, 205);
            }
        } else
            ChangePaneSize(circuitscr_pane, -20, 100, -10, 120);
    }
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    HAL_SetTextDepth(-1.0f);
}

/* The screen's text blit callback: the text, once the buttons exist. Always 1. */
s32 circuitscr_animText(Blit *blit)
{
    if (circuitscr_buttonsInited == 1) {
        circuitscr_animButtonText();
        return 1;
    }
    return 1;
}

/* Player p's final place over the circuit as text, into str. */
void getFinishPlaceStr(s32 p, u8 *str)
{
    TB_Strcpy(str, gText.single[78]);
    TB_Strcat(str, gText.taba[gText.tab[TEXT_PLACES] + gCircuitCars[p].totalPlace]);
    TB_Strcat(str, circuitscr_finish_place_separator);
    TB_Strcat(str, gText.single[129]);
}

extern void *memcpy(void *,const void *,u32);

VIEW(Color, circuitscr_init_button_color);
#define circuitscr_init_button_color circuitscr_init_button_color_view

/* Make the buttons that don't exist yet, each a model by its type (circuitscr_buttonNames; types 0-2 draw
 * see-through), in circuitscr_init_button_color; then texture and place the title, the cursor and the A and B
 * icons. */
void circuitscr_initButtons(void)
{
    Color color = circuitscr_init_button_color;
    s32 i;
    s16 texture_index;
    f32 yaw;
    for (i = 0; i < CIRCUIT_BUTTONS; i++) {
        if (circuitscr_buttonInfo[i].handle == -1) {
            memcpy(circuitscr_buttonInfo[i].matrix, unitmatrix, 36);
            circuitscr_buttonInfo[i].handle = MBOX_NewObject(
                MBOX_FindObject_WORK(circuitscr_buttonNames[circuitscr_buttonInfo[i].type], 0, gNumModels[0] - 1, 1),
                circuitscr_buttonInfo[i].matrix, -1,
                (circuitscr_buttonInfo[i].type == 0 || circuitscr_buttonInfo[i].type == 1 || circuitscr_buttonInfo[i].type == 2) ? 0x42000 : 0);
            ZOID_SetColor((s16)circuitscr_buttonInfo[i].handle, (s32 *)&color);
        }
    }
    ZOID_SetTexture((s16)circuitscr_buttonInfo[CIRCUIT_BUTTON_TITLE].handle,
        MBOX_FindTexture_WORK(circuitscr_title_texture_name, &texture_index, 0, gNumModels[0] - 1, 1), -1);
    memcpy(circuitscr_buttonInfo[CIRCUIT_BUTTON_TITLE].matrix, unitmatrix, 36);
    yaw = -1.5707963705062866f;
    WPitchUV(yaw, circuitscr_buttonInfo[CIRCUIT_BUTTON_TITLE].matrix);
    circuitscr_buttonInfo[CIRCUIT_BUTTON_TITLE].position[0] = 0.0f;
    circuitscr_buttonInfo[CIRCUIT_BUTTON_TITLE].position[1] = 115.0f;
    circuitscr_buttonInfo[CIRCUIT_BUTTON_TITLE].position[2] = 200.0f;
    ZOID_SetTexture((s16)circuitscr_buttonInfo[CIRCUIT_BUTTON_CURSOR].handle,
        MBOX_FindTexture_WORK(circuitscr_cursor_texture_name, &texture_index, 0, gNumModels[0] - 1, 1), -1);
    ZOID_SetTexture((s16)circuitscr_buttonInfo[CIRCUIT_BUTTON_A_ICON].handle,
        MBOX_FindTexture_WORK(circuitscr_a_button_texture_name, &texture_index, 0, gNumModels[0] - 1, 1), -1);
    memcpy(circuitscr_buttonInfo[CIRCUIT_BUTTON_A_ICON].matrix, unitmatrix, 36);
    WPitchUV(yaw, circuitscr_buttonInfo[CIRCUIT_BUTTON_A_ICON].matrix);

    circuitscr_buttonInfo[CIRCUIT_BUTTON_A_ICON].position[0] = 75.1f;
    circuitscr_buttonInfo[CIRCUIT_BUTTON_A_ICON].position[1] = 55.2f;
    circuitscr_buttonInfo[CIRCUIT_BUTTON_A_ICON].position[2] = 100.0f;
    ScaleUV(circuitscr_buttonInfo[CIRCUIT_BUTTON_A_ICON].matrix, circuitscr_buttonInfo[CIRCUIT_BUTTON_A_ICON].matrix, 0.5f);
    memcpy(circuitscr_buttonInfo[CIRCUIT_BUTTON_A_BACK].matrix, unitmatrix, 36);
    WPitchUV(yaw, circuitscr_buttonInfo[CIRCUIT_BUTTON_A_BACK].matrix);
    WRollUV(3.1415927410125732f, circuitscr_buttonInfo[CIRCUIT_BUTTON_A_BACK].matrix);
    circuitscr_buttonInfo[CIRCUIT_BUTTON_A_BACK].position[0] = 87.1f;
    circuitscr_buttonInfo[CIRCUIT_BUTTON_A_BACK].position[1] = 55.2f;
    circuitscr_buttonInfo[CIRCUIT_BUTTON_A_BACK].position[2] = 100.0f;
    ZOID_SetTexture((s16)circuitscr_buttonInfo[CIRCUIT_BUTTON_B_ICON].handle,
        MBOX_FindTexture_WORK(circuitscr_b_button_texture_name, &texture_index, 0, gNumModels[0] - 1, 1), -1);
    memcpy(circuitscr_buttonInfo[CIRCUIT_BUTTON_B_ICON].matrix, unitmatrix, 36);
    WPitchUV(yaw, circuitscr_buttonInfo[CIRCUIT_BUTTON_B_ICON].matrix);

    circuitscr_buttonInfo[CIRCUIT_BUTTON_B_ICON].position[0] = -75.1f;
    circuitscr_buttonInfo[CIRCUIT_BUTTON_B_ICON].position[1] = 55.20f;
    circuitscr_buttonInfo[CIRCUIT_BUTTON_B_ICON].position[2] = 100.0f;
    ScaleUV(circuitscr_buttonInfo[CIRCUIT_BUTTON_B_ICON].matrix, circuitscr_buttonInfo[CIRCUIT_BUTTON_B_ICON].matrix, 0.5f);
    memcpy(circuitscr_buttonInfo[CIRCUIT_BUTTON_B_BACK].matrix, unitmatrix, 36);
    WPitchUV(yaw, circuitscr_buttonInfo[CIRCUIT_BUTTON_B_BACK].matrix);
    circuitscr_buttonInfo[CIRCUIT_BUTTON_B_BACK].position[0] = -87.1f;
    circuitscr_buttonInfo[CIRCUIT_BUTTON_B_BACK].position[1] = 55.20f;
    circuitscr_buttonInfo[CIRCUIT_BUTTON_B_BACK].position[2] = 100.0f;
    circuitscr_buttonsInited = 1;
    circuitscr_buttonsJustInited = 1;
    ZeroCameras();
}
