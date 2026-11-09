/* The records screen (MAT3 recordscr.c): pick a profile on a pak (or the machine's totals: every profile and guest)
 * and view its statistics by track or circuit, its unlocked tracks, cars and parts; or clear a profile's records.
 * updateRecordScreen runs each frame in GS_RECORDS; recordscr_draw.c draws its text. */
#include "game/game.h"

/* The screen's state (gRecordState, MAT3 enum_kRecordStateCount). */
#define kRecordStateSelect 0
#define kRecordStateView 1

/* The select menu's rows (recordscr_gSelectMenuSelect; MAT3 names them as the player screen's). */
#define kSelectMenuTotals 0             /* the machine's totals */
#define kSelectMenuSelectPak 1
#define kSelectMenuPakStatus 2          /* the pak's status line: never selectable */
#define kSelectMenuSelectPlayer 3       /* 3-10: the pak's profiles */
#define kSelectMenuLastPlayer 10

/* The view menu's rows (gViewMenuSelect, MAT3 enum_kViewMenuCount): the track (or circuit) and the statistic. */
#define kViewMenuTrack 0
#define kViewMenuStats 1
#define kViewMenuCount 2
#define kViewStatTypeCount 13           /* gViewStatType[]: the statistics pages (recordscr_drawView) */

/* gRecordTrack: a record's track. 0-11 the six race tracks, each forward and backward; then the battle arenas, the
 * stunt tracks, the obstacle course, the four circuits and, for a profile, its totals. gTrackType is its kind
 * (MAT3 enum_kTrackTypeCount). */
#define kTrackTypeRace 0
#define kTrackTypeBattle 1
#define kTrackTypeStunt 2
#define kTrackTypeObstacle 3
#define kTrackTypeCircuit 4
#define kTrackTypeTotals 5

#define TEXT_GUESTS 177                 /* gText.single[]: the guests' name, when the totals' records are cleared */

/* Set gTrackType (and for a circuit gRecordCircuit) from gRecordTrack; both view rows are always offered. */
void updateViewInfo(void)
{
    s32 i;
    s32 flag;
    s8 level = gRecordTrack;

    if (level < RECORD_FIRST_BATTLE) {
        gTrackType = kTrackTypeRace;
        flag = 1;
    } else if (level < RECORD_FIRST_STUNT) {
        gTrackType = kTrackTypeBattle;
        flag = 1;
    } else if (level < RECORD_FIRST_OBSTACLE) {
        gTrackType = kTrackTypeStunt;
        flag = 1;
    } else if (level < RECORD_FIRST_CIRCUIT) {
        gTrackType = kTrackTypeObstacle;
        flag = 1;
    } else if (level < RECORD_TOTALS) {
        gTrackType = kTrackTypeCircuit;
        flag = 1;
        gRecordCircuit = level - RECORD_FIRST_CIRCUIT;
    } else {
        gTrackType = kTrackTypeTotals;
        flag = 1;
    }
    for (i = 0; i < kViewMenuCount; i++) {
        gViewMenuValid[i] = flag;
        gViewMenuVisible[i] = flag;
    }
}

/* Which select menu rows are offered and shown: the totals and the pak row always, the pak's status line (shown
 * only when the pak has no profiles) and a row per profile on the chosen pak. */
void recordscr_updateSelectInfo(void)
{
    unsigned int i;
    s32 count;

    recordscr_gSelectMenuValid[kSelectMenuTotals] = 1;
    recordscr_gSelectMenuVisible[kSelectMenuTotals] = 1;
    recordscr_gSelectMenuValid[kSelectMenuSelectPak] = 1;
    recordscr_gSelectMenuVisible[kSelectMenuSelectPak] = 1;
    recordscr_gSelectMenuValid[kSelectMenuPakStatus] = 0;
    count = recordscr_gNumPakPlayers[recordscr_gSelectPakIdx];
    recordscr_gSelectMenuVisible[kSelectMenuPakStatus] = count < 1;
    for (i = 0; i < 8; i++) {
        recordscr_gSelectMenuValid[i + kSelectMenuSelectPlayer] = (s32)i < count;
        recordscr_gSelectMenuVisible[i + kSelectMenuSelectPlayer] = (s32)i < count;
    }
}

typedef struct { s8 enabled; unsigned char remaining[15]; } PlayerSlot;   /* gControllerData: a controller */
/* A pak file record (MAT3 tPS_File) in its PC layout (pc_prelude.h PSF_OFS, docs/port/POINTERS.md 9.10): pak is
 * the pak it is on. */
typedef struct { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); unsigned char pak; } PakFile;
typedef struct { PakFile *file; } PakFileHandle;
typedef struct ListNode ProfileHandle;   /* struct ListNode: the name include/game gives it (gStatPlayer) */
/* A profile (MAT3 tPlayerProfile) in its PC layout (pc_prelude.h PROFILE_OFS): the links of gPlayerFiles, its pak
 * file, its id, its name, its save data (datah: NULL until ReadPlayerData reads it). */
typedef struct { ProfileHandle *next, *prev; PakFileHandle *fileh; unsigned char id[8]; char name[24]; void *datah; } PlayerProfile;
/* A profile handle (MAT3 tPlayerProfile **): the profiles are reached and listed through these. */
struct ListNode { PlayerProfile *body; };
typedef struct { s32 prefix[2]; ProfileHandle *head, *tail; } ListRoot;   /* gPlayerFiles: every profile on a pak */
VIEW(PlayerSlot[], gControllerData);
#define gControllerData gControllerData_view
VIEW(ListRoot, gPlayerFiles);
#define gPlayerFiles gPlayerFiles_view

/* Read the four paks: each one's profile count, or why it has none (no controller, no pak, damaged, a Rumble Pak). */
void recordscr_updatePakInfo(void) {
    s32 slot;
    ProfileHandle *node;
    for (slot = 0; slot < 4; slot++) {
        if (!gControllerData[slot].enabled) { recordscr_gNumPakPlayers[slot] = PAK_NO_CONTROLLER; }
        else if (!PS_PakInserted(slot)) { recordscr_gNumPakPlayers[slot] = PAK_NOT_INSERTED; }
        else if (PS_PakDamaged(slot)) { recordscr_gNumPakPlayers[slot] = PAK_DAMAGED; }
        else if (PS_PakRumble(slot)) { recordscr_gNumPakPlayers[slot] = PAK_RUMBLE; }
        else {
            node = gPlayerFiles.head;
            recordscr_gNumPakPlayers[slot] = 0;
            while (node != 0) {
                if (node->body->fileh->file->pak == slot) {
                    recordscr_gNumPakPlayers[slot]++;
                }
                node = node->body->next;
            }
            if (recordscr_gNumPakPlayers[slot] == 0) { recordscr_gNumPakPlayers[slot] = 0; }
        }
    }
}

/* Go to state mode. The select menu reads the paks again and puts any question or notice away; the view moves off
 * the totals track for the machine's totals (they have none) to the first page that has records, and works out
 * the track's kind. The buttons jump to their places. */
void setRecordState(s32 mode)
{
    gRecordState = mode;
    switch (gRecordState) {
    case kRecordStateSelect:
        recordscr_updatePakInfo();
        recordscr_updateSelectInfo();
        gDeleteConfirm = 0;
        gDeleteNotify = 0;
        recordscr_gYes = 0;
        gSelectMenuClear = 0;
        break;
    case kRecordStateView:
        if (gStatPlayer == 0 && gRecordTrack >= RECORD_TOTALS) {
            gRecordTrack = 0;
            while (gViewStatValid[gTrackType][gViewStatType[gTrackType]] == 0) {
                if (++gViewStatType[gTrackType] >= kViewStatTypeCount) {
                    gViewStatType[gTrackType] = 0;
                }
            }
        }
        updateViewInfo();
        break;
    }
    recordscr_buttonsJustInited = 1;
}

/* The buttons with their count: the optimizer orders the stores to a table of known size as the original
 * declaration had it. */
VIEW(MenuButton[RECORD_BUTTONS], recordscr_buttonInfo);
#define recordscr_buttonInfo recordscr_buttonInfo_view

/* RemovePane, under the stale name image A's symbol table still has for it (symbol_addrs.us.txt) */
extern void sound_handles_array_clear(void *);
extern void RemoveBlit(void *);

/* Take the screen down: its buttons, its box, its text blit and the background. */
void uninitRecordScreen(void) {
    recordscr_uninitButtons();
    if (recordscr_viewPane != 0) {
        sound_handles_array_clear(recordscr_viewPane);
        recordscr_viewPane = 0;
    }
    if (recordscr_blit != 0) {
        RemoveBlit(recordscr_blit);
        recordscr_blit = 0;
    }
    removeBG();
    recordscr_inited = 0;
}

/* The screen's frame (GS_RECORDS): set up on the first frame, read the paks, fall back to the totals if the
 * chosen pak has no profiles, then the state's input and the buttons; a soft reset goes back to the start menu. */
void updateRecordScreen(void)
{
    if (recordscr_inited == 0) {
        initRecordScreen();
    }
    recordscr_updatePakInfo();
    recordscr_updateSelectInfo();
    if (recordscr_gNumPakPlayers[recordscr_gSelectPakIdx] <= 0) {
        if (gStatPlayer != 0) {
            setRecordState(kRecordStateSelect);
        }
        gStatPlayer = 0;
    }
    if (gSoftReset != 0) {
        uninitRecordScreen();
        setGameState(GS_START);
    } else {
        switch (gRecordState) {
        case kRecordStateSelect:
            recordscr_controlSelect();
            break;
        case kRecordStateView:
            controlView();
            break;
        }
    }
    recordscr_updateButtons();
}

VIEW(char[], gDeleteName);
#define gDeleteName gDeleteName_view
VIEW(ProfileHandle[4], gJustPlayers);
#define gJustPlayers gJustPlayers_view

/* The select menu's input (MAT3 recordscr.c controlSelect). The selection first moves off a row that isn't offered.
 * With the "clear" button turned (left and right on a profile row) A asks to clear that profile's records (or all
 * the guests' for the totals), answers the question (left and right flip yes and no) and closes the notice. Else
 * A views the selected profile (read and locked) or the totals (the guests' and every profile's statistics added
 * up, each profile read for it and purged again unless it was already read). Left and right on the pak row change
 * the pak; up and down move through the offered rows, a profile row stepping through the pak's profiles. Start or B
 * backs out of a question or notice, or leaves for the start menu. */
void recordscr_controlSelect(void)
{
    s32 p, i;
    while (!recordscr_gSelectMenuValid[recordscr_gSelectMenuSelect]) {
        gSelectMenuClear = 0;
        recordscr_gSelectMenuSelect--;
        if (recordscr_gSelectMenuSelect < 0) recordscr_gSelectMenuSelect = RECORD_SELECT_ROWS - 1;
        if (recordscr_gSelectMenuSelect >= kSelectMenuSelectPlayer && recordscr_gSelectMenuSelect < RECORD_SELECT_ROWS && recordscr_gSelectMenuValid[recordscr_gSelectMenuSelect] == 1) {
            if (gStatPlayer) {
                do { gStatPlayer = gStatPlayer->body->prev; }
                while (gStatPlayer && gStatPlayer->body->fileh->file->pak != recordscr_gSelectPakIdx);
            }
            /* Original game: unlike every other step through the list, this one takes the last profile without
             * checking it is on the chosen pak (MAT3 the same). */
            if (!gStatPlayer) gStatPlayer = gPlayerFiles.tail;
        } else gStatPlayer = 0;
    }
    if (gDeleteNotify == 1) {
    } else if (gDeleteConfirm == 1) {
        if (gActiveAutoEdges & (BUTTON_LEFT | BUTTON_RIGHT)) {
            PlayLRSound(gActiveAutoEdges);
            recordscr_gYes = !recordscr_gYes;
        }
    } else if (recordscr_gSelectMenuSelect == kSelectMenuSelectPak && (gActiveLevels & (BUTTON_LEFT | BUTTON_RIGHT))) {
        if (gActiveAutoEdges & BUTTON_LEFT) {
            PlayLRSound(gActiveAutoEdges);
            recordscr_gSelectPakIdx--;
            if (recordscr_gSelectPakIdx < 0) recordscr_gSelectPakIdx = 3;
        } else if (gActiveAutoEdges & BUTTON_RIGHT) {
            PlayLRSound(gActiveAutoEdges);
            recordscr_gSelectPakIdx++;
            if (recordscr_gSelectPakIdx > 3) recordscr_gSelectPakIdx = 0;
        }
        recordscr_updateSelectInfo();
        recordscr_buttonsJustInited = 1;
    } else if (recordscr_gSelectMenuSelect >= kSelectMenuSelectPlayer && recordscr_gSelectMenuSelect <= RECORD_SELECT_ROWS && (gActiveLevels & (BUTTON_LEFT | BUTTON_RIGHT))) {
        if (gActiveEdges & (BUTTON_LEFT | BUTTON_RIGHT)) {
            PlayLRSound(gActiveAutoEdges);
            gSelectMenuClear = !gSelectMenuClear;
        }
    } else if (gActiveAutoEdges & BUTTON_UP) {
        PlayUDSound(gActiveAutoEdges);
        gSelectMenuClear = 0;
        do {
            if (recordscr_gSelectMenuSelect == kSelectMenuSelectPlayer && recordscr_gSelectMenuValid[recordscr_gSelectMenuSelect] == 1) {
                if (gStatPlayer) {
                    do { gStatPlayer = gStatPlayer->body->prev; }
                    while (gStatPlayer && gStatPlayer->body->fileh->file->pak != recordscr_gSelectPakIdx);
                }
                if (gStatPlayer) break;
            }
            recordscr_gSelectMenuSelect--;
            if (recordscr_gSelectMenuSelect < 0) recordscr_gSelectMenuSelect = RECORD_SELECT_ROWS - 1;
            if (recordscr_gSelectMenuSelect >= kSelectMenuSelectPlayer && recordscr_gSelectMenuSelect < RECORD_SELECT_ROWS && recordscr_gSelectMenuValid[recordscr_gSelectMenuSelect] == 1) {
                if (gStatPlayer) {
                    do { gStatPlayer = gStatPlayer->body->prev; }
                    while (gStatPlayer && gStatPlayer->body->fileh->file->pak != recordscr_gSelectPakIdx);
                }
                if (!gStatPlayer) {
                    gStatPlayer = gPlayerFiles.tail;
                    while (gStatPlayer && gStatPlayer->body->fileh->file->pak != recordscr_gSelectPakIdx) gStatPlayer = gStatPlayer->body->prev;
                }
            } else gStatPlayer = 0;
        } while (!recordscr_gSelectMenuValid[recordscr_gSelectMenuSelect]);
    } else if (gActiveAutoEdges & BUTTON_DOWN) {
        PlayUDSound(gActiveAutoEdges);
        gSelectMenuClear = 0;
        do {
            if (recordscr_gSelectMenuSelect == kSelectMenuLastPlayer && recordscr_gSelectMenuValid[recordscr_gSelectMenuSelect] == 1) {
                if (gStatPlayer) {
                    do { gStatPlayer = gStatPlayer->body->next; }
                    while (gStatPlayer && gStatPlayer->body->fileh->file->pak != recordscr_gSelectPakIdx);
                }
                if (gStatPlayer) break;
            }
            recordscr_gSelectMenuSelect++;
            if (recordscr_gSelectMenuSelect > RECORD_SELECT_ROWS - 1) recordscr_gSelectMenuSelect = 0;
            if (recordscr_gSelectMenuSelect >= kSelectMenuSelectPlayer && recordscr_gSelectMenuSelect < RECORD_SELECT_ROWS && recordscr_gSelectMenuValid[recordscr_gSelectMenuSelect] == 1) {
                if (gStatPlayer) {
                    do { gStatPlayer = gStatPlayer->body->next; }
                    while (gStatPlayer && gStatPlayer->body->fileh->file->pak != recordscr_gSelectPakIdx);
                }
                if (!gStatPlayer) {
                    gStatPlayer = gPlayerFiles.head;
                    while (gStatPlayer && gStatPlayer->body->fileh->file->pak != recordscr_gSelectPakIdx) gStatPlayer = gStatPlayer->body->next;
                }
            } else gStatPlayer = 0;
        } while (!recordscr_gSelectMenuValid[recordscr_gSelectMenuSelect]);
    }
    if (gActiveEdges & BUTTON_A) {
        if (gSelectMenuClear == 1) {
            PlayStartABSound(gActiveEdges);
            if (gDeleteNotify == 1) gDeleteNotify = 0;
            else if (gDeleteConfirm == 1) {
                if (recordscr_gYes == 1) {
                    if (gStatPlayer) {
                        if (ClearPlayerRecords(gStatPlayer)) gDeleteNotify = 1;
                    } else {
                        for (p = 0; p < 4; p++) ClearPlayerRecords(&gJustPlayers[p]);
                        gDeleteNotify = 1;
                    }
                }
                gDeleteConfirm = 0;
            } else {
                if (gStatPlayer) TB_Strcpy(gDeleteName, gStatPlayer->body->name);
                else TB_Strcpy(gDeleteName, gText.single[TEXT_GUESTS]);
                gDeleteConfirm = 1;
                recordscr_gYes = 0;
            }
        } else if (recordscr_gSelectMenuSelect == kSelectMenuTotals || (recordscr_gSelectMenuSelect >= kSelectMenuSelectPlayer && recordscr_gSelectMenuSelect < RECORD_SELECT_ROWS)) {
            PlayStartABSound(gActiveEdges);
            if (gStatPlayer) {
                if (ReadPlayerData(gStatPlayer)) {
                    PS_LockFile(gStatPlayer->body->fileh, 1);
                    setRecordState(kRecordStateView);
                }
            } else {
                ProfileHandle *node;
                ClearTotalStats();
                for (i = 0; i < 4; i++) {
                    AddPlayerStats(&gJustPlayers[i]);
                    AddPlayerScores(&gJustPlayers[i]);
                }
                for (node = gPlayerFiles.head; node; node = node->body->next) {
                    if (node->body->datah) {
                        AddPlayerStats(node); AddPlayerScores(node);
                    } else {
                        if (!ReadPlayerData(node)) break;
                        AddPlayerStats(node); AddPlayerScores(node);
                        PurgePlayerData(node);
                    }
                }
                if (!node) setRecordState(kRecordStateView);
            }
        }
    } else if (gActiveEdges & (BUTTON_START | BUTTON_B)) {
        PlayStartABSound(gActiveEdges);
        if (gDeleteNotify == 1) gDeleteNotify = 0;
        else if (gDeleteConfirm == 1) gDeleteConfirm = 0;
        else { uninitRecordScreen(); setGameState(GS_START); }
    }
}

/* The view's input (MAT3 recordscr.c controlView): left and right change the track (through the ones with records;
 * a profile also has its totals) or the statistics page (through the ones the track's kind has); up and down move
 * between the two rows. Start, A or B goes back to the select menu, unlocking and purging the profile's data. */
void controlView(void)
{
    if (gActiveLevels & (BUTTON_LEFT | BUTTON_RIGHT)) {
        if (gViewMenuSelect == kViewMenuTrack) {
            s8 level_step;
            level_step = gActiveAutoEdges & BUTTON_LEFT ? -1 : (gActiveAutoEdges & BUTTON_RIGHT ? 1 : 0);
            if (level_step != 0) {
                PlayLRSound(gActiveAutoEdges);
                do {
                    gRecordTrack += level_step;
                    if (gRecordTrack < 0) {
                        gRecordTrack = gStatPlayer != 0 ? RECORD_TOTALS : RECORD_TOTALS - 1;
                    }
                    if (gRecordTrack > (gStatPlayer != 0 ? RECORD_TOTALS : RECORD_TOTALS - 1)) {
                        gRecordTrack = 0;
                    }
                } while (gViewTrackValid[gRecordTrack] == 0);
                updateViewInfo();
            }
        } else {
            s8 item_step;
            item_step = gActiveAutoEdges & BUTTON_LEFT ? -1 : (gActiveAutoEdges & BUTTON_RIGHT ? 1 : 0);
            if (item_step != 0) {
                PlayLRSound(gActiveAutoEdges);
                do {
                    gViewStatType[gTrackType] += item_step;
                    if (gViewStatType[gTrackType] < 0) {
                        gViewStatType[gTrackType] = kViewStatTypeCount - 1;
                    }
                    if (gViewStatType[gTrackType] >= kViewStatTypeCount) {
                        gViewStatType[gTrackType] = 0;
                    }
                } while (gViewStatValid[gTrackType][gViewStatType[gTrackType]] == 0);
                updateViewInfo();
            }
        }
    } else if (gActiveAutoEdges & BUTTON_UP) {
        PlayUDSound(gActiveAutoEdges);
        do {
            if (--gViewMenuSelect < 0) {
                gViewMenuSelect = kViewMenuCount - 1;
            }
        } while (gViewMenuValid[gViewMenuSelect] == 0);
    } else if (gActiveAutoEdges & BUTTON_DOWN) {
        PlayUDSound(gActiveAutoEdges);
        do {
            if (++gViewMenuSelect >= kViewMenuCount) {
                gViewMenuSelect = 0;
            }
        } while (gViewMenuValid[gViewMenuSelect] == 0);
    }
    while (gViewStatValid[gTrackType][gViewStatType[gTrackType]] == 0) {
        if (++gViewStatType[gTrackType] >= kViewStatTypeCount) {
            gViewStatType[gTrackType] = 0;
        }
    }
    while (gViewMenuValid[gViewMenuSelect] == 0) {
        if (--gViewMenuSelect < 0) {
            gViewMenuSelect = kViewMenuCount - 1;
        }
    }
    if (gActiveEdges & (BUTTON_ACCEPT | BUTTON_B)) {
        PlayStartABSound(gActiveEdges);
        setRecordState(kRecordStateSelect);
        if (gStatPlayer != 0) {
            PS_LockFile(gStatPlayer->body->fileh, 0);
            PurgePlayerData(gStatPlayer);
        }
    }
}

typedef struct { u8 red, green, blue, alpha; } Color4;

VIEW(f32[3][3], unitmatrix);
#define unitmatrix unitmatrix_view
VIEW(volatile u8, gNumModels);
#define gNumModels gNumModels_view

VIEW(struct pc_multiblit[1], recordscr_mblit);
#define recordscr_mblit recordscr_mblit_view
extern void particle_lifetime_set ( void );
extern void *memcpy(void *, const void *, u32);

/* PC: ZOID_SetColor reads a colour as one word, so the colours passed to it are word-aligned (docs/port/SANITIZE.md
 * 3.2) */
#define PC_WORD_ALIGNED __attribute__((aligned(4)))

extern void *MBOX_FindTexture_WORK(char *, s16 *, s32, s8, s32);
extern void ZOID_SetTexture(s16, void *, s32);

/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
extern void *InitPane(s32, s32, s32, s32, s32, intptr_t, s32, s32);
extern void *NewMultiBlit(s16, s16, void *, s16);

/* Set the screen up on the totals: the select menu, the buttons, a box (off screen until a page sizes it) and the
 * text blit. */
void initRecordScreen(void)
{
    recordscr_gSelectMenuSelect = kSelectMenuTotals;
    gStatPlayer = 0;
    setRecordState(kRecordStateSelect);
    particle_lifetime_set();
    recordscr_initButtons();
    recordscr_viewPane = InitPane(-20, 0, -10, 10, 192, 0, 0, 0);
    recordscr_blit = NewMultiBlit(0, 0, recordscr_mblit, 1);
    gDeleteConfirm = 0;
    gDeleteNotify = 0;
    recordscr_gYes = 0;
    gSelectMenuClear = 0;
    recordscr_inited = 1;
}

/* Remove the screen's buttons (3D models) from the scene and mark them gone (handle -1). */
void recordscr_uninitButtons(void)
{
    s32 i;
    for (i = 0; i < RECORD_BUTTONS; i++) {
        if (recordscr_buttonInfo[i].handle != -1) {
            ZOID_RemoveObject((s16)recordscr_buttonInfo[i].handle);
            recordscr_buttonInfo[i].handle = -1;
        }
    }
    recordscr_buttonsInited = 0;
}

/* Make the buttons that don't exist yet, each a model by its type (recordscr_buttonNames; types 0-2 and 7-11 draw
 * see-through), white; then texture and place the title, the cursor and the A and B icons. */
void recordscr_initButtons(void)
{
    Color4 buttonColor PC_WORD_ALIGNED = {255, 255, 255, 255};
    void *tex;
    s16 texIdx;
    s32 buttonIdx;
    for (buttonIdx = 0; buttonIdx < RECORD_BUTTONS; buttonIdx++) {
        if (recordscr_buttonInfo[buttonIdx].handle == -1) {
            memcpy(recordscr_buttonInfo[buttonIdx].matrix, unitmatrix, 36);
            recordscr_buttonInfo[buttonIdx].handle = MBOX_NewObject(
                MBOX_FindObject_WORK(recordscr_buttonNames[recordscr_buttonInfo[buttonIdx].type], 0, gNumModels - 1, 1),
                recordscr_buttonInfo[buttonIdx].matrix, -1,
                (recordscr_buttonInfo[buttonIdx].type == 0 || recordscr_buttonInfo[buttonIdx].type == 1 ||
                 recordscr_buttonInfo[buttonIdx].type == 2 || recordscr_buttonInfo[buttonIdx].type == 7 ||
                 recordscr_buttonInfo[buttonIdx].type == 8 || recordscr_buttonInfo[buttonIdx].type == 9 ||
                 recordscr_buttonInfo[buttonIdx].type == 10 || recordscr_buttonInfo[buttonIdx].type == 11) ? 0x42000 : 0);
            ZOID_SetColor(recordscr_buttonInfo[buttonIdx].handle, &buttonColor);
            MBOX_HideObject(recordscr_buttonInfo[buttonIdx].handle, 1, 15);
            MBOX_ShowObject(recordscr_buttonInfo[buttonIdx].handle, 0, 15);
        }
    }
    tex = MBOX_FindTexture_WORK(recordscr_title_texture_name, &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(recordscr_buttonInfo[RECORD_BUTTON_TITLE].handle, tex, -1);
    memcpy(recordscr_buttonInfo[RECORD_BUTTON_TITLE].matrix, unitmatrix, 36);
    WPitchUV(-1.5707964f, recordscr_buttonInfo[RECORD_BUTTON_TITLE].matrix);
    recordscr_buttonInfo[RECORD_BUTTON_TITLE].position[0] = 0.0f;
    recordscr_buttonInfo[RECORD_BUTTON_TITLE].position[1] = 115.0f;
    recordscr_buttonInfo[RECORD_BUTTON_TITLE].position[2] = 200.0f;
    recordscr_buttonInfo[RECORD_BUTTON_TITLE].position[0] *= 1.0f;
    recordscr_buttonInfo[RECORD_BUTTON_TITLE].position[1] *= 1.0f;
    recordscr_buttonInfo[RECORD_BUTTON_TITLE].position[2] *= 1.0f;
    tex = MBOX_FindTexture_WORK(recordscr_cursor_texture_name, &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(recordscr_buttonInfo[RECORD_BUTTON_CURSOR].handle, tex, -1);
    tex = MBOX_FindTexture_WORK(recordscr_a_button_texture_name, &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(recordscr_buttonInfo[RECORD_BUTTON_A_ICON].handle, tex, -1);
    memcpy(recordscr_buttonInfo[RECORD_BUTTON_A_ICON].matrix, unitmatrix, 36);
    WPitchUV(-1.5707964f, recordscr_buttonInfo[RECORD_BUTTON_A_ICON].matrix);
    recordscr_buttonInfo[RECORD_BUTTON_A_ICON].position[0] = 75.1f;
    recordscr_buttonInfo[RECORD_BUTTON_A_ICON].position[1] = 55.2f;
    recordscr_buttonInfo[RECORD_BUTTON_A_ICON].position[2] = 100.0f;
    ScaleUV(recordscr_buttonInfo[RECORD_BUTTON_A_ICON].matrix, recordscr_buttonInfo[RECORD_BUTTON_A_ICON].matrix, 0.5f);
    memcpy(recordscr_buttonInfo[RECORD_BUTTON_A_BACK].matrix, unitmatrix, 36);
    WPitchUV(-1.5707964f, recordscr_buttonInfo[RECORD_BUTTON_A_BACK].matrix);
    WRollUV(3.1415927f, recordscr_buttonInfo[RECORD_BUTTON_A_BACK].matrix);
    recordscr_buttonInfo[RECORD_BUTTON_A_BACK].position[0] = 87.1f;
    recordscr_buttonInfo[RECORD_BUTTON_A_BACK].position[1] = 55.2f;
    recordscr_buttonInfo[RECORD_BUTTON_A_BACK].position[2] = 100.0f;
    tex = MBOX_FindTexture_WORK(recordscr_b_button_texture_name, &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(recordscr_buttonInfo[RECORD_BUTTON_B_ICON].handle, tex, -1);
    memcpy(recordscr_buttonInfo[RECORD_BUTTON_B_ICON].matrix, unitmatrix, 36);
    WPitchUV(-1.5707964f, recordscr_buttonInfo[RECORD_BUTTON_B_ICON].matrix);
    recordscr_buttonInfo[RECORD_BUTTON_B_ICON].position[0] = -75.1f;
    recordscr_buttonInfo[RECORD_BUTTON_B_ICON].position[1] = 55.2f;
    recordscr_buttonInfo[RECORD_BUTTON_B_ICON].position[2] = 100.0f;
    recordscr_buttonInfo[RECORD_BUTTON_B_ICON].position[0] *= 1.0f;
    recordscr_buttonInfo[RECORD_BUTTON_B_ICON].position[1] *= 1.0f;
    recordscr_buttonInfo[RECORD_BUTTON_B_ICON].position[2] *= 1.0f;
    ScaleUV(recordscr_buttonInfo[RECORD_BUTTON_B_ICON].matrix, recordscr_buttonInfo[RECORD_BUTTON_B_ICON].matrix, 0.5f);
    memcpy(recordscr_buttonInfo[RECORD_BUTTON_B_BACK].matrix, unitmatrix, 36);
    WPitchUV(-1.5707964f, recordscr_buttonInfo[RECORD_BUTTON_B_BACK].matrix);
    recordscr_buttonInfo[RECORD_BUTTON_B_BACK].position[0] = -87.1f;
    recordscr_buttonInfo[RECORD_BUTTON_B_BACK].position[1] = 55.2f;
    recordscr_buttonInfo[RECORD_BUTTON_B_BACK].position[2] = 100.0f;
    recordscr_buttonInfo[RECORD_BUTTON_B_BACK].position[0] *= 1.0f;
    recordscr_buttonInfo[RECORD_BUTTON_B_BACK].position[1] *= 1.0f;
    recordscr_buttonInfo[RECORD_BUTTON_B_BACK].position[2] *= 1.0f;
    recordscr_buttonsInited = 1;
    recordscr_buttonsJustInited = 1;
    ZeroCameras();
}

VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

extern f32 fabsf(f32);
#pragma intrinsic(fabsf)

/* Lay the select menu's buttons out for this frame. With a question or notice up only the title and the A and B
 * icons show. Otherwise the cursor spins beside the selected row and eases towards it, each row's button turns to its
 * selected face when selected (not while the clear button is), the list scrolls to keep it in view and fades at the
 * screen's top and bottom; the pak row has its value button, and a selected profile the clear button. */
void updateSelectButtons(void)
{
    s32 buttonIdx;
    s32 i;
    f32 prevPos[3];
    s32 gSelectMenuCount;
    s32 whichItem;
    f32 buttonY;
    f32 selectedPos;
    s8 clearScrollSpeed = 1;
    void *tex;
    s16 texIdx;
    Color4 buttonColor PC_WORD_ALIGNED = {255, 255, 255, 255};

    if (gDeleteConfirm == 1 || gDeleteNotify == 1) {
        for (buttonIdx = 0; buttonIdx < 23; buttonIdx++) {
            if (buttonIdx == RECORD_BUTTON_TITLE || buttonIdx == RECORD_BUTTON_A_ICON || buttonIdx == RECORD_BUTTON_B_ICON || buttonIdx == RECORD_BUTTON_B_BACK)
                MBOX_ShowObject(recordscr_buttonInfo[buttonIdx].handle, 0, 15);
            else MBOX_HideObject(recordscr_buttonInfo[buttonIdx].handle, 1, 15);
        }
    } else {
        for (buttonIdx = 0; buttonIdx < 23; buttonIdx++) {
            if ((buttonIdx > RECORD_VIEW_ROW_BUTTON && buttonIdx < RECORD_BUTTON_CLEAR) || buttonIdx == RECORD_BUTTON_TRACK_VALUE || buttonIdx == RECORD_BUTTON_STAT_VALUE)
                MBOX_HideObject(recordscr_buttonInfo[buttonIdx].handle, 1, 15);
            else MBOX_ShowObject(recordscr_buttonInfo[buttonIdx].handle, 0, 15);
        }
        gSelectMenuCount = 0;
        whichItem = 0;
        for (i = 0; i < RECORD_SELECT_ROWS; i++) {
            if (recordscr_gSelectMenuVisible[i]) {
                gSelectMenuCount++;
                if (i < recordscr_gSelectMenuSelect) whichItem++;
            }
        }
        if (recordscr_gButtonScale * ((gSelectMenuCount - 1) * recordscr_gButtonSpace) > recordscr_maxY - recordscr_minY)
            selectedPos = recordscr_maxY - whichItem * (recordscr_maxY - recordscr_minY) / (gSelectMenuCount - 1);
        else selectedPos = recordscr_maxY - recordscr_gButtonScale * (whichItem * recordscr_gButtonSpace);
        memcpy(prevPos, recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position, 12);
        recordscr_buttonInfo[RECORD_BUTTON_CURSOR].angle += 6.2831855f * gFrameSeconds;
        if (recordscr_buttonInfo[RECORD_BUTTON_CURSOR].angle > 6.2831855f) recordscr_buttonInfo[RECORD_BUTTON_CURSOR].angle -= 6.2831855f;
        memcpy(recordscr_buttonInfo[RECORD_BUTTON_CURSOR].matrix, unitmatrix, 36);
        recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position[0] = recordscr_buttonX * recordscr_cursorZ / recordscr_buttonZ + -165.0f * recordscr_gButtonScale;
        recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position[1] = selectedPos * recordscr_cursorZ / recordscr_buttonZ;
        recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position[2] = recordscr_cursorZ;
        if (!recordscr_buttonsJustInited) {
            if (fabsf(recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position[1] - prevPos[1]) * 60 * recordscr_gButtonScale / 10 > recordscr_cursorSpeed) {
                recordscr_cursorSpeed = fabsf(recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position[1] - prevPos[1]) * 60 * recordscr_gButtonScale / 10;
                recordscr_cursorSpeed = recordscr_cursorSpeed < 540 * recordscr_gButtonScale ? recordscr_cursorSpeed : 540 * recordscr_gButtonScale;
                recordscr_cursorSpeed = recordscr_cursorSpeed > 180 * recordscr_gButtonScale ? recordscr_cursorSpeed : 180 * recordscr_gButtonScale;
            }
            if (recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position[1] > prevPos[1] + recordscr_cursorSpeed * gFrameSeconds)
                recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position[1] = prevPos[1] + recordscr_cursorSpeed * gFrameSeconds;
            else if (recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position[1] < prevPos[1] - recordscr_cursorSpeed * gFrameSeconds)
                recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position[1] = prevPos[1] - recordscr_cursorSpeed * gFrameSeconds;
            else recordscr_cursorSpeed = 180 * recordscr_gButtonScale;
        }
        WRollUV(recordscr_buttonInfo[RECORD_BUTTON_CURSOR].angle, recordscr_buttonInfo[RECORD_BUTTON_CURSOR].matrix);
        WPitchUV(1.5707964f, recordscr_buttonInfo[RECORD_BUTTON_CURSOR].matrix);
        ScaleUV(recordscr_buttonInfo[RECORD_BUTTON_CURSOR].matrix, recordscr_buttonInfo[RECORD_BUTTON_CURSOR].matrix, recordscr_gButtonScale * 0.4f);
        buttonY = selectedPos + recordscr_gButtonScale * (whichItem * recordscr_gButtonSpace);
        for (buttonIdx = 0; buttonIdx < RECORD_SELECT_ROWS; buttonIdx++) {
            if (buttonIdx == recordscr_gSelectMenuSelect && !gSelectMenuClear) {
                if (recordscr_buttonInfo[buttonIdx].angle < 3.1415927f) recordscr_buttonInfo[buttonIdx].angle += 12.566371f * gFrameSeconds;
                if (recordscr_buttonInfo[buttonIdx].angle > 3.1415927f) recordscr_buttonInfo[buttonIdx].angle = 3.1415927f;
                if (recordscr_buttonsJustInited) recordscr_buttonInfo[buttonIdx].angle = 3.1415927f;
            } else {
                if (recordscr_buttonInfo[buttonIdx].angle > 0) recordscr_buttonInfo[buttonIdx].angle -= 12.566371f * gFrameSeconds;
                if (recordscr_buttonInfo[buttonIdx].angle < 0) recordscr_buttonInfo[buttonIdx].angle = 0;
                if (recordscr_buttonsJustInited) recordscr_buttonInfo[buttonIdx].angle = 0;
            }
            memcpy(prevPos, recordscr_buttonInfo[buttonIdx].position, 12);
            if (recordscr_buttonInfo[buttonIdx].type == 11) recordscr_buttonInfo[buttonIdx].position[0] = recordscr_buttonX + 0 * recordscr_gButtonScale;
            else if (recordscr_buttonInfo[buttonIdx].type == 9) recordscr_buttonInfo[buttonIdx].position[0] = recordscr_buttonX + -80 * recordscr_gButtonScale;
            else recordscr_buttonInfo[buttonIdx].position[0] = recordscr_buttonX + -120 * recordscr_gButtonScale;
            recordscr_buttonInfo[buttonIdx].position[1] = buttonY;
            recordscr_buttonInfo[buttonIdx].position[2] = recordscr_buttonZ;
            if (!recordscr_buttonsJustInited) {
                if (fabsf(recordscr_buttonInfo[buttonIdx].position[1] - prevPos[1]) * 60 / 10 > recordscr_scrollSpeed) {
                    recordscr_scrollSpeed = fabsf(recordscr_buttonInfo[buttonIdx].position[1] - prevPos[1]) * 60 / 10;
                    recordscr_scrollSpeed = recordscr_scrollSpeed < 540 * recordscr_gButtonScale ? recordscr_scrollSpeed : 540 * recordscr_gButtonScale;
                    recordscr_scrollSpeed = recordscr_scrollSpeed > 180 * recordscr_gButtonScale ? recordscr_scrollSpeed : 180 * recordscr_gButtonScale;
                }
                if (recordscr_buttonInfo[buttonIdx].position[1] > prevPos[1] + recordscr_scrollSpeed * gFrameSeconds) {
                    recordscr_buttonInfo[buttonIdx].position[1] = prevPos[1] + recordscr_scrollSpeed * gFrameSeconds;
                    clearScrollSpeed = 0;
                } else if (recordscr_buttonInfo[buttonIdx].position[1] < prevPos[1] - recordscr_scrollSpeed * gFrameSeconds) {
                    recordscr_buttonInfo[buttonIdx].position[1] = prevPos[1] - recordscr_scrollSpeed * gFrameSeconds;
                    clearScrollSpeed = 0;
                }
            }
            ScaleUV(unitmatrix, recordscr_buttonInfo[buttonIdx].matrix, recordscr_gButtonScale);
            WPitchUV(recordscr_buttonInfo[buttonIdx].angle - 1.5707964f, recordscr_buttonInfo[buttonIdx].matrix);
            if (recordscr_buttonInfo[buttonIdx].angle > 1.5707964f)
                tex = MBOX_FindTexture_WORK("BUTTON_SELECT", &texIdx, 0, gNumModels - 1, 1);
            else tex = MBOX_FindTexture_WORK("BUTTON", &texIdx, 0, gNumModels - 1, 1);
            ZOID_SetTexture(recordscr_buttonInfo[buttonIdx].handle, tex, -1);
            if (recordscr_buttonInfo[buttonIdx].position[1] > recordscr_maxY + recordscr_gButtonSpace * recordscr_gButtonScale ||
                recordscr_buttonInfo[buttonIdx].position[1] < recordscr_minY - recordscr_gButtonSpace * recordscr_gButtonScale) recordscr_buttonInfo[buttonIdx].alpha = 0;
            else if (recordscr_buttonInfo[buttonIdx].position[1] > recordscr_maxY)
                recordscr_buttonInfo[buttonIdx].alpha = 255.f - (recordscr_buttonInfo[buttonIdx].position[1] - recordscr_maxY) * 255.0f / (recordscr_gButtonSpace * recordscr_gButtonScale);
            else if (recordscr_buttonInfo[buttonIdx].position[1] < recordscr_minY)
                recordscr_buttonInfo[buttonIdx].alpha = 255.f - (recordscr_minY - recordscr_buttonInfo[buttonIdx].position[1]) * 255.0f / (recordscr_gButtonSpace * recordscr_gButtonScale);
            else recordscr_buttonInfo[buttonIdx].alpha = 255;
            if (!recordscr_gSelectMenuVisible[buttonIdx]) recordscr_buttonInfo[buttonIdx].alpha = 0;
            buttonColor.alpha = recordscr_buttonInfo[buttonIdx].alpha;
            ZOID_SetColor(recordscr_buttonInfo[buttonIdx].handle, &buttonColor);
            if (!recordscr_gSelectMenuVisible[buttonIdx] || !recordscr_buttonInfo[buttonIdx].alpha) MBOX_HideObject(recordscr_buttonInfo[buttonIdx].handle, 1, 15);
            else MBOX_ShowObject(recordscr_buttonInfo[buttonIdx].handle, 0, 15);
            if (buttonIdx == kSelectMenuSelectPak) {
                recordscr_buttonInfo[RECORD_BUTTON_PAK_VALUE].position[0] = recordscr_buttonX + 160 * recordscr_gButtonScale;
                recordscr_buttonInfo[RECORD_BUTTON_PAK_VALUE].position[1] = recordscr_buttonInfo[buttonIdx].position[1];
                recordscr_buttonInfo[RECORD_BUTTON_PAK_VALUE].position[2] = recordscr_buttonInfo[buttonIdx].position[2];
                recordscr_buttonInfo[RECORD_BUTTON_PAK_VALUE].angle = recordscr_buttonInfo[buttonIdx].angle;
                ScaleUV(unitmatrix, recordscr_buttonInfo[RECORD_BUTTON_PAK_VALUE].matrix, recordscr_gButtonScale);
                WPitchUV(recordscr_buttonInfo[RECORD_BUTTON_PAK_VALUE].angle - 1.5707964f, recordscr_buttonInfo[RECORD_BUTTON_PAK_VALUE].matrix);
                ZOID_SetTexture(recordscr_buttonInfo[RECORD_BUTTON_PAK_VALUE].handle, tex, -1);
                recordscr_buttonInfo[RECORD_BUTTON_PAK_VALUE].alpha = recordscr_buttonInfo[buttonIdx].alpha;
                ZOID_SetColor(recordscr_buttonInfo[RECORD_BUTTON_PAK_VALUE].handle, &buttonColor);
                if (!recordscr_gSelectMenuVisible[buttonIdx] || !recordscr_buttonInfo[buttonIdx].alpha) MBOX_HideObject(recordscr_buttonInfo[RECORD_BUTTON_PAK_VALUE].handle, 1, 15);
                else MBOX_ShowObject(recordscr_buttonInfo[RECORD_BUTTON_PAK_VALUE].handle, 0, 15);
            } else if (recordscr_gSelectMenuSelect >= kSelectMenuSelectPlayer && recordscr_gSelectMenuSelect < RECORD_SELECT_ROWS) {
                if (buttonIdx == recordscr_gSelectMenuSelect) {
                    recordscr_buttonInfo[RECORD_BUTTON_CLEAR].position[0] = recordscr_buttonX + 120 * recordscr_gButtonScale;
                    recordscr_buttonInfo[RECORD_BUTTON_CLEAR].position[1] = recordscr_buttonInfo[buttonIdx].position[1];
                    recordscr_buttonInfo[RECORD_BUTTON_CLEAR].position[2] = recordscr_buttonInfo[buttonIdx].position[2];
                    if (gSelectMenuClear) {
                        if (recordscr_buttonInfo[RECORD_BUTTON_CLEAR].angle < 3.1415927f) recordscr_buttonInfo[RECORD_BUTTON_CLEAR].angle += 12.566371f * gFrameSeconds;
                        if (recordscr_buttonInfo[RECORD_BUTTON_CLEAR].angle > 3.1415927f) recordscr_buttonInfo[RECORD_BUTTON_CLEAR].angle = 3.1415927f;
                        if (recordscr_buttonsJustInited) recordscr_buttonInfo[RECORD_BUTTON_CLEAR].angle = 3.1415927f;
                    } else {
                        if (recordscr_buttonInfo[RECORD_BUTTON_CLEAR].angle > 0) recordscr_buttonInfo[RECORD_BUTTON_CLEAR].angle -= 12.566371f * gFrameSeconds;
                        if (recordscr_buttonInfo[RECORD_BUTTON_CLEAR].angle < 0) recordscr_buttonInfo[RECORD_BUTTON_CLEAR].angle = 0;
                        if (recordscr_buttonsJustInited) recordscr_buttonInfo[RECORD_BUTTON_CLEAR].angle = 0;
                    }
                    ScaleUV(unitmatrix, recordscr_buttonInfo[RECORD_BUTTON_CLEAR].matrix, recordscr_gButtonScale);
                    WPitchUV(recordscr_buttonInfo[RECORD_BUTTON_CLEAR].angle - 1.5707964f, recordscr_buttonInfo[RECORD_BUTTON_CLEAR].matrix);
                    if (recordscr_buttonInfo[RECORD_BUTTON_CLEAR].angle > 1.5707964f)
                        tex = MBOX_FindTexture_WORK("BUTTON_SELECT", &texIdx, 0, gNumModels - 1, 1);
                    else tex = MBOX_FindTexture_WORK("BUTTON", &texIdx, 0, gNumModels - 1, 1);
                    ZOID_SetTexture(recordscr_buttonInfo[RECORD_BUTTON_CLEAR].handle, tex, -1);
                    recordscr_buttonInfo[RECORD_BUTTON_CLEAR].alpha = recordscr_buttonInfo[buttonIdx].alpha;
                    ZOID_SetColor(recordscr_buttonInfo[RECORD_BUTTON_CLEAR].handle, &buttonColor);
                    if (!recordscr_gSelectMenuVisible[buttonIdx] || !recordscr_buttonInfo[RECORD_BUTTON_CLEAR].alpha) MBOX_HideObject(recordscr_buttonInfo[RECORD_BUTTON_CLEAR].handle, 1, 15);
                    else MBOX_ShowObject(recordscr_buttonInfo[RECORD_BUTTON_CLEAR].handle, 0, 15);
                }
            } else {
                recordscr_buttonInfo[RECORD_BUTTON_CLEAR].angle = 0;
                MBOX_HideObject(recordscr_buttonInfo[RECORD_BUTTON_CLEAR].handle, 1, 15);
            }
            if (recordscr_gSelectMenuVisible[buttonIdx] == 1) buttonY -= recordscr_gButtonSpace * recordscr_gButtonScale;
        }
        for (; buttonIdx < RECORD_BUTTON_CLEAR; buttonIdx++) MBOX_HideObject(recordscr_buttonInfo[buttonIdx].handle, 1, 15);
        if (clearScrollSpeed == 1) recordscr_scrollSpeed = 0;
        recordscr_buttonsJustInited = 0;
    }
}

/* Lay the buttons out for the state the screen is in, once they exist. */
void recordscr_updateButtons(void)
{
    if (recordscr_buttonsInited) {
        switch (gRecordState) {
        case kRecordStateSelect: updateSelectButtons(); break;
        case kRecordStateView: updateViewButtons(); break;
        }
    }
}

/* Lay the view's buttons out for this frame: the select menu's rows hide, the view's two rows (track, statistic)
 * and their value buttons show, with the cursor beside the selected one, as updateSelectButtons. */
void updateViewButtons(void)
{
    s32 buttonIdx, i;
    f32 prevPos[3];
    s32 gViewMenuCount;
    s32 whichItem;
    f32 buttonY, selectedPos;
    s8 clearScrollSpeed = 1;
    void *tex;
    u16 texIdx;
    Color4 buttonColor PC_WORD_ALIGNED = {255, 255, 255, 255};
    gViewMenuCount = 0;
    whichItem = 0;
    for (i = 0; i < kViewMenuCount; i++) {
        if (gViewMenuVisible[i]) {
            gViewMenuCount++;
            if (i < gViewMenuSelect) whichItem++;
        }
    }
    if ((gViewMenuCount - 1) * recordscr_gButtonSpace * recordscr_gButtonScale > recordscr_maxY - recordscr_minY)
        selectedPos = recordscr_maxY - whichItem * (recordscr_maxY - recordscr_minY) / (gViewMenuCount - 1);
    else selectedPos = recordscr_maxY - whichItem * recordscr_gButtonSpace * recordscr_gButtonScale;
    memcpy(prevPos, recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position, 12);
    recordscr_buttonInfo[RECORD_BUTTON_CURSOR].angle += 6.2831855f * gFrameSeconds;
    if (recordscr_buttonInfo[RECORD_BUTTON_CURSOR].angle > 6.2831855f) recordscr_buttonInfo[RECORD_BUTTON_CURSOR].angle -= 6.2831855f;
    memcpy(recordscr_buttonInfo[RECORD_BUTTON_CURSOR].matrix, unitmatrix, 36);
    recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position[0] = recordscr_buttonX * recordscr_cursorZ / recordscr_buttonZ + -165.0f * recordscr_gButtonScale;
    recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position[1] = selectedPos * recordscr_cursorZ / recordscr_buttonZ;
    recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position[2] = recordscr_cursorZ;
    if (!recordscr_buttonsJustInited) {
        if (recordscr_cursorSpeed < fabsf(recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position[1] - prevPos[1]) * 60.0f * recordscr_gButtonScale / 10.0f) {
            recordscr_cursorSpeed = fabsf(recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position[1] - prevPos[1]) * 60.0f * recordscr_gButtonScale / 10.0f;
            recordscr_cursorSpeed = recordscr_cursorSpeed < 540.0f * recordscr_gButtonScale ? recordscr_cursorSpeed : 540.0f * recordscr_gButtonScale;
            recordscr_cursorSpeed = recordscr_cursorSpeed > 180.0f * recordscr_gButtonScale ? recordscr_cursorSpeed : 180.0f * recordscr_gButtonScale;
        }
        if (recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position[1] > prevPos[1] + recordscr_cursorSpeed * gFrameSeconds)
            recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position[1] = prevPos[1] + recordscr_cursorSpeed * gFrameSeconds;
        else if (recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position[1] < prevPos[1] - recordscr_cursorSpeed * gFrameSeconds)
            recordscr_buttonInfo[RECORD_BUTTON_CURSOR].position[1] = prevPos[1] - recordscr_cursorSpeed * gFrameSeconds;
        else recordscr_cursorSpeed = 180.0f * recordscr_gButtonScale;
    }
    WRollUV(recordscr_buttonInfo[RECORD_BUTTON_CURSOR].angle, recordscr_buttonInfo[RECORD_BUTTON_CURSOR].matrix);
    WPitchUV(1.5707964f, recordscr_buttonInfo[RECORD_BUTTON_CURSOR].matrix);
    ScaleUV(recordscr_buttonInfo[RECORD_BUTTON_CURSOR].matrix, recordscr_buttonInfo[RECORD_BUTTON_CURSOR].matrix, recordscr_gButtonScale * 0.4f);
    buttonY = selectedPos + whichItem * recordscr_gButtonSpace * recordscr_gButtonScale;
    for (buttonIdx = 0; buttonIdx < RECORD_SELECT_ROWS; buttonIdx++) MBOX_HideObject(recordscr_buttonInfo[buttonIdx].handle, 1, 15);
    for (buttonIdx = RECORD_VIEW_ROW_BUTTON; buttonIdx < RECORD_VIEW_ROW_BUTTON + kViewMenuCount; buttonIdx++) {
        if (buttonIdx - RECORD_VIEW_ROW_BUTTON == gViewMenuSelect) {
            if (recordscr_buttonInfo[buttonIdx].angle < 3.1415927f) recordscr_buttonInfo[buttonIdx].angle += 12.566371f * gFrameSeconds;
            if (recordscr_buttonInfo[buttonIdx].angle > 3.1415927f) recordscr_buttonInfo[buttonIdx].angle = 3.1415927f;
            if (recordscr_buttonsJustInited) recordscr_buttonInfo[buttonIdx].angle = 3.1415927f;
        } else {
            if (recordscr_buttonInfo[buttonIdx].angle > 0.0f) recordscr_buttonInfo[buttonIdx].angle -= 12.566371f * gFrameSeconds;
            if (recordscr_buttonInfo[buttonIdx].angle < 0.0f) recordscr_buttonInfo[buttonIdx].angle = 0.0f;
            if (recordscr_buttonsJustInited) recordscr_buttonInfo[buttonIdx].angle = 0.0f;
        }
        memcpy(prevPos, recordscr_buttonInfo[buttonIdx].position, 12);
        recordscr_buttonInfo[buttonIdx].position[0] = -140.0f * recordscr_gButtonScale + recordscr_buttonX;
        recordscr_buttonInfo[buttonIdx].position[1] = buttonY;
        recordscr_buttonInfo[buttonIdx].position[2] = recordscr_buttonZ;
        if (!recordscr_buttonsJustInited) {
            if (recordscr_scrollSpeed < fabsf(recordscr_buttonInfo[buttonIdx].position[1] - prevPos[1]) * 60.0f / 10.0f) {
                recordscr_scrollSpeed = fabsf(recordscr_buttonInfo[buttonIdx].position[1] - prevPos[1]) * 60.0f / 10.0f;
                recordscr_scrollSpeed = recordscr_scrollSpeed < 540.0f * recordscr_gButtonScale ? recordscr_scrollSpeed : 540.0f * recordscr_gButtonScale;
                recordscr_scrollSpeed = recordscr_scrollSpeed > 180.0f * recordscr_gButtonScale ? recordscr_scrollSpeed : 180.0f * recordscr_gButtonScale;
            }
            if (recordscr_buttonInfo[buttonIdx].position[1] > prevPos[1] + recordscr_scrollSpeed * gFrameSeconds) {
                recordscr_buttonInfo[buttonIdx].position[1] = prevPos[1] + recordscr_scrollSpeed * gFrameSeconds;
                clearScrollSpeed = 0;
            } else if (recordscr_buttonInfo[buttonIdx].position[1] < prevPos[1] - recordscr_scrollSpeed * gFrameSeconds) {
                recordscr_buttonInfo[buttonIdx].position[1] = prevPos[1] - recordscr_scrollSpeed * gFrameSeconds;
                clearScrollSpeed = 0;
            }
        }
        ScaleUV(unitmatrix, recordscr_buttonInfo[buttonIdx].matrix, recordscr_gButtonScale);
        WPitchUV(recordscr_buttonInfo[buttonIdx].angle - 1.5707964f, recordscr_buttonInfo[buttonIdx].matrix);
        if (recordscr_buttonInfo[buttonIdx].angle > 1.5707964f) tex = MBOX_FindTexture_WORK(recordscr_select_texture_name, (s16 *)&texIdx, 0, gNumModels - 1, 1);
        else tex = MBOX_FindTexture_WORK(recordscr_button_texture_name, (s16 *)&texIdx, 0, gNumModels - 1, 1);
        ZOID_SetTexture((s16)recordscr_buttonInfo[buttonIdx].handle, tex, -1);
        if (recordscr_buttonInfo[buttonIdx].position[1] > recordscr_maxY + recordscr_gButtonSpace * recordscr_gButtonScale || recordscr_buttonInfo[buttonIdx].position[1] < recordscr_minY - recordscr_gButtonSpace * recordscr_gButtonScale) recordscr_buttonInfo[buttonIdx].alpha = 0;
        else if (recordscr_buttonInfo[buttonIdx].position[1] > recordscr_maxY) recordscr_buttonInfo[buttonIdx].alpha = 255.0f - (recordscr_buttonInfo[buttonIdx].position[1] - recordscr_maxY) * 255 / (recordscr_gButtonSpace * recordscr_gButtonScale);
        else if (recordscr_buttonInfo[buttonIdx].position[1] < recordscr_minY) recordscr_buttonInfo[buttonIdx].alpha = 255.0f - (recordscr_minY - recordscr_buttonInfo[buttonIdx].position[1]) * 255 / (recordscr_gButtonSpace * recordscr_gButtonScale);
        else recordscr_buttonInfo[buttonIdx].alpha = 255;
        if (!gViewMenuVisible[buttonIdx - RECORD_VIEW_ROW_BUTTON]) recordscr_buttonInfo[buttonIdx].alpha = 0;
        buttonColor.alpha = recordscr_buttonInfo[buttonIdx].alpha;
        ZOID_SetColor((s16)recordscr_buttonInfo[buttonIdx].handle, &buttonColor);
        if (!gViewMenuVisible[buttonIdx - RECORD_VIEW_ROW_BUTTON] || !recordscr_buttonInfo[buttonIdx].alpha) MBOX_HideObject(recordscr_buttonInfo[buttonIdx].handle, 1, 15);
        else MBOX_ShowObject(recordscr_buttonInfo[buttonIdx].handle, 0, 15);
        if (buttonIdx - RECORD_VIEW_ROW_BUTTON == 0) {
            recordscr_buttonInfo[RECORD_BUTTON_TRACK_VALUE].position[0] = 100.0f * recordscr_gButtonScale + recordscr_buttonX;
            recordscr_buttonInfo[RECORD_BUTTON_TRACK_VALUE].position[1] = recordscr_buttonInfo[buttonIdx].position[1];
            recordscr_buttonInfo[RECORD_BUTTON_TRACK_VALUE].position[2] = recordscr_buttonInfo[buttonIdx].position[2];
            recordscr_buttonInfo[RECORD_BUTTON_TRACK_VALUE].angle = recordscr_buttonInfo[buttonIdx].angle;
            ScaleUV(unitmatrix, recordscr_buttonInfo[RECORD_BUTTON_TRACK_VALUE].matrix, recordscr_gButtonScale);
            WPitchUV(recordscr_buttonInfo[RECORD_BUTTON_TRACK_VALUE].angle - 1.5707964f, recordscr_buttonInfo[RECORD_BUTTON_TRACK_VALUE].matrix);
            ZOID_SetTexture((s16)recordscr_buttonInfo[RECORD_BUTTON_TRACK_VALUE].handle, tex, -1);
            recordscr_buttonInfo[RECORD_BUTTON_TRACK_VALUE].alpha = recordscr_buttonInfo[buttonIdx].alpha;
            ZOID_SetColor((s16)recordscr_buttonInfo[RECORD_BUTTON_TRACK_VALUE].handle, &buttonColor);
            /* Original game: tests gViewMenuVisible[buttonIdx] (11, past the 2-entry array) where the other tests
             * have buttonIdx - RECORD_VIEW_ROW_BUTTON; MAT3 the same. It reads the byte that follows the array. */
            if (!gViewMenuVisible[buttonIdx] || !recordscr_buttonInfo[buttonIdx].alpha) MBOX_HideObject(recordscr_buttonInfo[RECORD_BUTTON_TRACK_VALUE].handle, 1, 15);
            else MBOX_ShowObject(recordscr_buttonInfo[RECORD_BUTTON_TRACK_VALUE].handle, 0, 15);
        } else if (buttonIdx - RECORD_VIEW_ROW_BUTTON == 1) {
            recordscr_buttonInfo[RECORD_BUTTON_STAT_VALUE].position[0] = 100.0f * recordscr_gButtonScale + recordscr_buttonX;
            recordscr_buttonInfo[RECORD_BUTTON_STAT_VALUE].position[1] = recordscr_buttonInfo[buttonIdx].position[1];
            recordscr_buttonInfo[RECORD_BUTTON_STAT_VALUE].position[2] = recordscr_buttonInfo[buttonIdx].position[2];
            recordscr_buttonInfo[RECORD_BUTTON_STAT_VALUE].angle = recordscr_buttonInfo[buttonIdx].angle;
            ScaleUV(unitmatrix, recordscr_buttonInfo[RECORD_BUTTON_STAT_VALUE].matrix, recordscr_gButtonScale);
            WPitchUV(recordscr_buttonInfo[RECORD_BUTTON_STAT_VALUE].angle - 1.5707964f, recordscr_buttonInfo[RECORD_BUTTON_STAT_VALUE].matrix);
            ZOID_SetTexture((s16)recordscr_buttonInfo[RECORD_BUTTON_STAT_VALUE].handle, tex, -1);
            recordscr_buttonInfo[RECORD_BUTTON_STAT_VALUE].alpha = recordscr_buttonInfo[buttonIdx].alpha;
            ZOID_SetColor((s16)recordscr_buttonInfo[RECORD_BUTTON_STAT_VALUE].handle, &buttonColor);
            if (!gViewMenuVisible[buttonIdx - RECORD_VIEW_ROW_BUTTON] || !recordscr_buttonInfo[buttonIdx].alpha) MBOX_HideObject(recordscr_buttonInfo[RECORD_BUTTON_STAT_VALUE].handle, 1, 15);
            else MBOX_ShowObject(recordscr_buttonInfo[RECORD_BUTTON_STAT_VALUE].handle, 0, 15);
        }
        if (gViewMenuVisible[buttonIdx - RECORD_VIEW_ROW_BUTTON] == 1) buttonY -= recordscr_gButtonSpace * recordscr_gButtonScale;
    }
    for (; buttonIdx < RECORD_BUTTONS; buttonIdx++) {
        if (buttonIdx < RECORD_BUTTON_TRACK_VALUE) MBOX_HideObject(recordscr_buttonInfo[buttonIdx].handle, 1, 15);
        else MBOX_ShowObject(recordscr_buttonInfo[buttonIdx].handle, 0, 15);
    }
    if (clearScrollSpeed == 1) recordscr_scrollSpeed = 0.0f;
    recordscr_buttonsJustInited = 0;
}
