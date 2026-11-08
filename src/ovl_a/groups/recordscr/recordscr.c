/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* superseding revision: claims three deleted-inline stubs, in the post-order the IPA layout uses
 * (updateRecordScreen's calls visited last first): updateViewButtons = updateViewButtons (the helper formerly named
 * recordscr_updateViewButtons, renamed after its stub), controlView = controlView (case 1 of updateRecordScreen
 * moved verbatim into a helper; MAT3 block-local step variables) and recordscr_uninitButtons = uninitButtons@recordscr.c
 * (the button loop of uninitRecordScreen uninitRecordScreen moved verbatim into a helper). Every old member MATCHes;
 * updateRecordScreen no longer needs its two unused residual slots or the `mode` carrier. */
/* superseding revision: recordscr_updateButtons is now a claimed member, a direct typed port of MAT3
 * recordscr.c updateButtons + inlined updateViewButtons (see its header below).  The six old members are unchanged
 * and still MATCH; the only shared-declaration change is `extern volatile u8 gNumModels` (gNumModels; its other
 * user here is the context initRecordScreen, whose retail code also has the volatile lui/addiu/lbu 0(reg) form). */
/* merge: revision as installed + (updateSelectButtons, recordscr_controlSelect) + (initRecordScreen,
 * recordscr_initButtons); each block is taken verbatim, shared declarations agree (volatile gNumModels/gFrameSeconds). */
/* superseding revision: initRecordScreen (GC initRecordScreen@recordscr.c + inlined initButtons) is now a member, with its
 * deleted inlined-procedure stub recordscr_initButtons; see the header at the function. Every earlier member is unchanged. */
/* Image A availability-screen input root, updateRecordScreen (1292 bytes).
 * Reconstructed from native targets; no proven arcade-source ancestor.
 * Complete genuine 923B0 renderer, 9338C input and 93B3C initializer are
 * nonmatching context. Their real IPA clobbers, particularly six FP save
 * pairs from the renderer, are required to reproduce the kept root's ABI.
 * The native mode read has its own s32 carrier. Two actual signed-byte
 * input steps retain separate homes; every root statement is inherited.
 * (the former SHAPING residual_slot0/residual_slot1 and the
 * `mode` carrier are gone. The 8 + 8 frame bytes they stood in for are
 * umerge's reservation for the inlined controlView call: case 1 is MAT3
 * controlView, now the non-kept helper controlView whose deleted procedure
 * is the retail stub 0x803932D4. Root frame 128 with no unused locals.)
 * Earlier strict helpers are preservation members, with no new credit.
 * Context FP/data discrepancies remain unclaimed; root owns no data.
 * : adds members/claims updateSelectButtons = updateSelectButtons@recordscr.c (MAT3
 * GameSource recordscr.c, GC 0x80084f38) and recordscr_controlSelect = controlSelect@recordscr.c (GC
 * 0x80080874; replaces the earlier native-led context body), direct typed ports. The family is
 * MAT3 recordscr.c: 94058 updateRecordScreen (controlView inlined), 9338C controlSelect,
 * 93B3C initRecordScreen, 923B0 updateButtons (updateViewButtons inlined), 931C0 setRecordState.
 * gFrameSeconds gFrameSeconds and gNumModels gNumModels are volatile (MAT3 rush_protos.h).
 * SHAPING in updateSelectButtons: unusedA/unusedB are two unused s32 locals before prevPos (the same
 * two-slot residual as updateButtons@playerscr playerscr_updateButtons): retail frame 128, prevPos at sp+100;
 * without them prevPos is at sp+108 and the frame is 120. 2 slots, 8 bytes. No padding arrays.
 * 255.f / 255.0f are deliberately different spellings (one shared 255 web is hoisted into $f30
 * and spills buttonY); the type-11 x is `buttonX + 0 * gButtonScale` (int zero, as 94DD8).
 */
/* Complete genuine A:931C0 -> {930B8,93004,92F28} call family. */
typedef signed char s8; typedef unsigned char u8; typedef short s16; typedef int s32; typedef float f32;
extern s8 gRecordTrack;
extern s32 gTrackType;
extern s8 gRecordCircuit;
extern s8 gViewMenuValid[];
extern s8 gViewMenuVisible[];

void updateViewInfo(void)
{
    s32 i;
    s32 flag;
    s8 level = gRecordTrack;

    if (level < 12) {
        gTrackType = 0;
        flag = 1;
    } else if (level < 20) {
        gTrackType = 1;
        flag = 1;
    } else if (level < 24) {
        gTrackType = 2;
        flag = 1;
    } else if (level < 25) {
        gTrackType = 3;
        flag = 1;
    } else if (level < 29) {
        gTrackType = 4;
        flag = 1;
        gRecordCircuit = level - 25;
    } else {
        gTrackType = 5;
        flag = 1;
    }
    for (i = 0; i < 2; i++) {
        gViewMenuValid[i] = flag;
        gViewMenuVisible[i] = flag;
    }
}

extern s8 recordscr_gSelectMenuValid[];
extern s8 recordscr_gSelectMenuVisible[];
extern s32 recordscr_gNumPakPlayers[];
extern s8 recordscr_gSelectPakIdx;

void recordscr_updateSelectInfo(void)
{
    unsigned int i;
    s32 count;

    recordscr_gSelectMenuValid[0] = 1;
    recordscr_gSelectMenuVisible[0] = 1;
    recordscr_gSelectMenuValid[1] = 1;
    recordscr_gSelectMenuVisible[1] = 1;
    recordscr_gSelectMenuValid[2] = 0;
    count = recordscr_gNumPakPlayers[recordscr_gSelectPakIdx];
    recordscr_gSelectMenuVisible[2] = count < 1;
    for (i = 0; i < 8; i++) {
        recordscr_gSelectMenuValid[i + 3] = (s32)i < count;
        recordscr_gSelectMenuVisible[i + 3] = (s32)i < count;
    }
}


/* Bounded storage views follow only observed offsets; names are descriptive. */
typedef struct { s8 enabled; unsigned char remaining[15]; } PlayerSlot;
/* PC: the pak file record's host layout (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); unsigned char slot; } ListOwner;
typedef struct { ListOwner *owner; } ListObject;
typedef struct ListNode ListNode;
/* PC: the profile's host layout (pc_prelude.h); flags2C is its save data handle */
typedef struct { ListNode *next, *prev; ListObject *object; unsigned char unknown0C[8]; char text[24]; void *flags2C; } ListPayload;
struct ListNode { ListPayload *payload; };
typedef struct { s32 prefix[2]; ListNode *head, *tail; } ListRoot;
extern PlayerSlot gControllerData[];
extern ListRoot gPlayerFiles;
extern s32 PS_PakInserted(s32);
extern s32 PS_PakDamaged(s32);
extern s32 PS_PakRumble(s32);
void recordscr_updatePakInfo(void) {
    s32 slot;
    ListNode *node;
    for (slot = 0; slot < 4; slot++) {
        if (!gControllerData[slot].enabled) { recordscr_gNumPakPlayers[slot] = -4; }
        else if (!PS_PakInserted(slot)) { recordscr_gNumPakPlayers[slot] = -1; }
        else if (PS_PakDamaged(slot)) { recordscr_gNumPakPlayers[slot] = -2; }
        else if (PS_PakRumble(slot)) { recordscr_gNumPakPlayers[slot] = -3; }
        else {
            node = gPlayerFiles.head;
            recordscr_gNumPakPlayers[slot] = 0;
            while (node != 0) {
                if (node->payload->object->owner->slot == slot) {
                    recordscr_gNumPakPlayers[slot]++;
                }
                node = node->payload->next;
            }
            if (recordscr_gNumPakPlayers[slot] == 0) { recordscr_gNumPakPlayers[slot] = 0; }
        }
    }
}

void recordscr_updatePakInfo(void);
void recordscr_updateSelectInfo(void);
void updateViewInfo(void);
extern s32 gRecordState;
extern ListNode *gStatPlayer;
extern s8 gDeleteConfirm;
extern s8 gDeleteNotify;
extern s8 recordscr_gYes;
extern s8 gSelectMenuClear;
extern s8 gRecordTrack;
extern s32 gTrackType;
extern s32 gViewStatType[];
extern s8 gViewStatValid[][13];
extern s32 recordscr_buttonsJustInited;

void setRecordState(s32 mode)
{
    gRecordState = mode;
    switch (gRecordState) {
    case 0:
        recordscr_updatePakInfo();
        recordscr_updateSelectInfo();
        gDeleteConfirm = 0;
        gDeleteNotify = 0;
        recordscr_gYes = 0;
        gSelectMenuClear = 0;
        break;
    case 1:
        if (gStatPlayer == 0 && gRecordTrack >= 29) {
            gRecordTrack = 0;
            while (gViewStatValid[gTrackType][gViewStatType[gTrackType]] == 0) {
                if (++gViewStatType[gTrackType] >= 13) {
                    gViewStatType[gTrackType] = 0;
                }
            }
        }
        updateViewInfo();
        break;
    }
    recordscr_buttonsJustInited = 1;
}

/* Image A: complete 168-byte cleanup. Native clobbers s0-s2; standalone is research. */
typedef struct { s32 fields[2]; f32 angle; f32 matrix[3][3]; f32 position[3]; u8 alpha; u8 remaining[3]; } MenuRecord;
extern MenuRecord recordscr_buttonInfo[23];
extern s32 recordscr_buttonsInited;
extern void *recordscr_viewPane, *recordscr_blit;
extern s8 recordscr_inited;
extern void ZOID_RemoveObject(s16);
extern void sound_handles_array_clear(void *);
extern void RemoveBlit(void *);
extern void removeBG(void);
void recordscr_uninitButtons(void);
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

/* Full image-A entry root. Only witnessed fields and control flow are named. */
extern s8 gSoftReset;
extern s32 gActiveLevels, gActiveEdges, gActiveAutoEdges;
extern s32 gViewMenuSelect;
extern s8 gViewTrackValid[];
extern void initRecordScreen(void);
extern void recordscr_controlSelect(void);
extern void recordscr_updateButtons(void);
extern void setGameState(s32);
extern void PlayLRSound(s32);
extern void PlayUDSound(s32);
extern void PlayStartABSound(s32);
extern void PS_LockFile(ListObject *, s32);
extern void PurgePlayerData(ListNode *);

void controlView(void);
void updateRecordScreen(void)
{
    if (recordscr_inited == 0) {
        initRecordScreen();
    }
    recordscr_updatePakInfo();
    recordscr_updateSelectInfo();
    if (recordscr_gNumPakPlayers[recordscr_gSelectPakIdx] <= 0) {
        if (gStatPlayer != 0) {
            setRecordState(0);
        }
        gStatPlayer = 0;
    }
    if (gSoftReset != 0) {
        uninitRecordScreen();
        setGameState(16);
    } else {
        switch (gRecordState) {
        case 0:
            recordscr_controlSelect();
            break;
        case 1:
            controlView();
            break;
        }
    }
    recordscr_updateButtons();
}

extern s32 recordscr_gSelectMenuSelect;
extern char gDeleteName[];
extern ListNode gJustPlayers[4];
/* PC: gText's string tables hold host pointers (port/assets/text.c), so N64 byte offsets / 4 are indices */
typedef struct { char *prefix[0x2C4 / 4]; const char *label; } ResourceNames;
extern ResourceNames *gText_single;
extern s32 ClearPlayerRecords(ListNode *);
extern s32 ReadPlayerData(ListNode *);
extern void TB_Strcpy(char *, const char *);
extern void ClearTotalStats(void);
extern void AddPlayerStats(ListNode *);
extern void AddPlayerScores(ListNode *);

/* recordscr_controlSelect = controlSelect@recordscr.c (MAT3 GameSource recordscr.c, GC 0x80080874, size 0x788).
 * Direct typed port. Globals: recordscr_gSelectMenuSelect gSelectMenuSelect, recordscr_gSelectMenuValid gSelectMenuValid[11], gSelectMenuClear
 * gSelectMenuClear, gStatPlayer gStatPlayer (tPlayerProfile **), recordscr_gSelectPakIdx gSelectPakIdx, gPlayerFiles gPlayerFiles,
 * gDeleteConfirm gDeleteConfirm, recordscr_gYes gYes, gDeleteNotify gDeleteNotify, gDeleteName gDeleteName[13], recordscr_buttonsJustInited
 * buttonsJustInited, gJustPlayers gJustPlayers[4], gActiveAutoEdges gActiveAutoEdges, gActiveLevels gActiveLevels,
 * gActiveEdges gActiveEdges. Calls: PlayLRSound PlayLRSound, PlayUDSound PlayUDSound, PlayStartABSound
 * PlayStartABSound, recordscr_updateSelectInfo updateSelectInfo, ClearPlayerRecords ClearPlayerRecords, TB_Strcpy TB_Strcpy,
 * ReadPlayerData ReadPlayerData, PS_LockFile PS_LockFile, setRecordState setRecordState, ClearTotalStats
 * ClearTotalStats, AddPlayerStats AddPlayerStats, AddPlayerScores AddPlayerScores, PurgePlayerData PurgePlayerData,
 * uninitRecordScreen uninitRecordScreen, setGameState setGameState.
 * N64 differences: no MtapConnected test (pak index always wraps at 4), gText.single[177] (GC 187). */
void recordscr_controlSelect(void)
{
    s32 p, i;
    while (!recordscr_gSelectMenuValid[recordscr_gSelectMenuSelect]) {
        gSelectMenuClear = 0;
        recordscr_gSelectMenuSelect--;
        if (recordscr_gSelectMenuSelect < 0) recordscr_gSelectMenuSelect = 10;
        if (recordscr_gSelectMenuSelect >= 3 && recordscr_gSelectMenuSelect < 11 && recordscr_gSelectMenuValid[recordscr_gSelectMenuSelect] == 1) {
            if (gStatPlayer) {
                do { gStatPlayer = gStatPlayer->payload->prev; }
                while (gStatPlayer && gStatPlayer->payload->object->owner->slot != recordscr_gSelectPakIdx);
            }
            if (!gStatPlayer) gStatPlayer = gPlayerFiles.tail;
        } else gStatPlayer = 0;
    }
    if (gDeleteNotify == 1) {
    } else if (gDeleteConfirm == 1) {
        if (gActiveAutoEdges & 0x3000) {
            PlayLRSound(gActiveAutoEdges);
            recordscr_gYes = !recordscr_gYes;
        }
    } else if (recordscr_gSelectMenuSelect == 1 && (gActiveLevels & 0x3000)) {
        if (gActiveAutoEdges & 0x1000) {
            PlayLRSound(gActiveAutoEdges);
            recordscr_gSelectPakIdx--;
            if (recordscr_gSelectPakIdx < 0) recordscr_gSelectPakIdx = 3;
        } else if (gActiveAutoEdges & 0x2000) {
            PlayLRSound(gActiveAutoEdges);
            recordscr_gSelectPakIdx++;
            if (recordscr_gSelectPakIdx > 3) recordscr_gSelectPakIdx = 0;
        }
        recordscr_updateSelectInfo();
        recordscr_buttonsJustInited = 1;
    } else if (recordscr_gSelectMenuSelect >= 3 && recordscr_gSelectMenuSelect <= 11 && (gActiveLevels & 0x3000)) {
        if (gActiveEdges & 0x3000) {
            PlayLRSound(gActiveAutoEdges);
            gSelectMenuClear = !gSelectMenuClear;
        }
    } else if (gActiveAutoEdges & 0x400) {
        PlayUDSound(gActiveAutoEdges);
        gSelectMenuClear = 0;
        do {
            if (recordscr_gSelectMenuSelect == 3 && recordscr_gSelectMenuValid[recordscr_gSelectMenuSelect] == 1) {
                if (gStatPlayer) {
                    do { gStatPlayer = gStatPlayer->payload->prev; }
                    while (gStatPlayer && gStatPlayer->payload->object->owner->slot != recordscr_gSelectPakIdx);
                }
                if (gStatPlayer) break;
            }
            recordscr_gSelectMenuSelect--;
            if (recordscr_gSelectMenuSelect < 0) recordscr_gSelectMenuSelect = 10;
            if (recordscr_gSelectMenuSelect >= 3 && recordscr_gSelectMenuSelect < 11 && recordscr_gSelectMenuValid[recordscr_gSelectMenuSelect] == 1) {
                if (gStatPlayer) {
                    do { gStatPlayer = gStatPlayer->payload->prev; }
                    while (gStatPlayer && gStatPlayer->payload->object->owner->slot != recordscr_gSelectPakIdx);
                }
                if (!gStatPlayer) {
                    gStatPlayer = gPlayerFiles.tail;
                    while (gStatPlayer && gStatPlayer->payload->object->owner->slot != recordscr_gSelectPakIdx) gStatPlayer = gStatPlayer->payload->prev;
                }
            } else gStatPlayer = 0;
        } while (!recordscr_gSelectMenuValid[recordscr_gSelectMenuSelect]);
    } else if (gActiveAutoEdges & 0x800) {
        PlayUDSound(gActiveAutoEdges);
        gSelectMenuClear = 0;
        do {
            if (recordscr_gSelectMenuSelect == 10 && recordscr_gSelectMenuValid[recordscr_gSelectMenuSelect] == 1) {
                if (gStatPlayer) {
                    do { gStatPlayer = gStatPlayer->payload->next; }
                    while (gStatPlayer && gStatPlayer->payload->object->owner->slot != recordscr_gSelectPakIdx);
                }
                if (gStatPlayer) break;
            }
            recordscr_gSelectMenuSelect++;
            if (recordscr_gSelectMenuSelect > 10) recordscr_gSelectMenuSelect = 0;
            if (recordscr_gSelectMenuSelect >= 3 && recordscr_gSelectMenuSelect < 11 && recordscr_gSelectMenuValid[recordscr_gSelectMenuSelect] == 1) {
                if (gStatPlayer) {
                    do { gStatPlayer = gStatPlayer->payload->next; }
                    while (gStatPlayer && gStatPlayer->payload->object->owner->slot != recordscr_gSelectPakIdx);
                }
                if (!gStatPlayer) {
                    gStatPlayer = gPlayerFiles.head;
                    while (gStatPlayer && gStatPlayer->payload->object->owner->slot != recordscr_gSelectPakIdx) gStatPlayer = gStatPlayer->payload->next;
                }
            } else gStatPlayer = 0;
        } while (!recordscr_gSelectMenuValid[recordscr_gSelectMenuSelect]);
    }
    if (gActiveEdges & 2) {
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
                if (gStatPlayer) TB_Strcpy(gDeleteName, gStatPlayer->payload->text);
                else TB_Strcpy(gDeleteName, gText_single->label);
                gDeleteConfirm = 1;
                recordscr_gYes = 0;
            }
        } else if (recordscr_gSelectMenuSelect == 0 || (recordscr_gSelectMenuSelect >= 3 && recordscr_gSelectMenuSelect < 11)) {
            PlayStartABSound(gActiveEdges);
            if (gStatPlayer) {
                if (ReadPlayerData(gStatPlayer)) {
                    PS_LockFile(gStatPlayer->payload->object, 1);
                    setRecordState(1);
                }
            } else {
                ListNode *node;
                ClearTotalStats();
                for (i = 0; i < 4; i++) {
                    AddPlayerStats(&gJustPlayers[i]);
                    AddPlayerScores(&gJustPlayers[i]);
                }
                for (node = gPlayerFiles.head; node; node = node->payload->next) {
                    if (node->payload->flags2C) {
                        AddPlayerStats(node); AddPlayerScores(node);
                    } else {
                        if (!ReadPlayerData(node)) break;
                        AddPlayerStats(node); AddPlayerScores(node);
                        PurgePlayerData(node);
                    }
                }
                if (!node) setRecordState(1);
            }
        }
    } else if (gActiveEdges & 5) {
        PlayStartABSound(gActiveEdges);
        if (gDeleteNotify == 1) gDeleteNotify = 0;
        else if (gDeleteConfirm == 1) gDeleteConfirm = 0;
        else { uninitRecordScreen(); setGameState(16); }
    }
}

/* controlView@recordscr.c (MAT3 GC controlView): case 1 of updateRecordScreen updateRecordScreen, inlined; its deleted
 * procedure is the retail stub controlView. Body moved verbatim from updateRecordScreen. */
void controlView(void)
{
    if (gActiveLevels & 0x3000) {
        if (gViewMenuSelect == 0) {
            s8 level_step;
            level_step = gActiveAutoEdges & 0x1000 ? -1 : (gActiveAutoEdges & 0x2000 ? 1 : 0);
            if (level_step != 0) {
                PlayLRSound(gActiveAutoEdges);
                do {
                    gRecordTrack += level_step;
                    if (gRecordTrack < 0) {
                        gRecordTrack = gStatPlayer != 0 ? 29 : 28;
                    }
                    if (gRecordTrack > (gStatPlayer != 0 ? 29 : 28)) {
                        gRecordTrack = 0;
                    }
                } while (gViewTrackValid[gRecordTrack] == 0);
                updateViewInfo();
            }
        } else {
            s8 item_step;
            item_step = gActiveAutoEdges & 0x1000 ? -1 : (gActiveAutoEdges & 0x2000 ? 1 : 0);
            if (item_step != 0) {
                PlayLRSound(gActiveAutoEdges);
                do {
                    gViewStatType[gTrackType] += item_step;
                    if (gViewStatType[gTrackType] < 0) {
                        gViewStatType[gTrackType] = 12;
                    }
                    if (gViewStatType[gTrackType] >= 13) {
                        gViewStatType[gTrackType] = 0;
                    }
                } while (gViewStatValid[gTrackType][gViewStatType[gTrackType]] == 0);
                updateViewInfo();
            }
        }
    } else if (gActiveAutoEdges & 0x400) {
        PlayUDSound(gActiveAutoEdges);
        do {
            if (--gViewMenuSelect < 0) {
                gViewMenuSelect = 1;
            }
        } while (gViewMenuValid[gViewMenuSelect] == 0);
    } else if (gActiveAutoEdges & 0x800) {
        PlayUDSound(gActiveAutoEdges);
        do {
            if (++gViewMenuSelect >= 2) {
                gViewMenuSelect = 0;
            }
        } while (gViewMenuValid[gViewMenuSelect] == 0);
    }
    while (gViewStatValid[gTrackType][gViewStatType[gTrackType]] == 0) {
        if (++gViewStatType[gTrackType] >= 13) {
            gViewStatType[gTrackType] = 0;
        }
    }
    while (gViewMenuValid[gViewMenuSelect] == 0) {
        if (--gViewMenuSelect < 0) {
            gViewMenuSelect = 1;
        }
    }
    if (gActiveEdges & 7) {
        PlayStartABSound(gActiveEdges);
        setRecordState(0);
        if (gStatPlayer != 0) {
            PS_LockFile(gStatPlayer->payload->object, 0);
            PurgePlayerData(gStatPlayer);
        }
    }
}


/* Complete initializer; slot geometry follows the witnessed 64-byte records. */
typedef unsigned int u32;
typedef struct { u8 red, green, blue, alpha; } Color4;
extern Color4 recordscr_init_button_color;
extern char *recordscr_buttonNames[];
extern f32 unitmatrix[3][3];
extern volatile u8 gNumModels;
extern char recordscr_title_texture_name[], recordscr_cursor_texture_name[], recordscr_a_button_texture_name[], recordscr_b_button_texture_name[];
extern struct pc_multiblit recordscr_mblit[1];   /* NewMultiBlit's descriptor (pc_prelude.h) */
extern void particle_lifetime_set(void), ZeroCameras(void);
extern void *memcpy(void *, const void *, u32);
extern s32 MBOX_FindObject_WORK(char *, s8, s8, s8);
extern s32 MBOX_NewObject(s32, f32 (*)[3], s32, u32);
extern void ZOID_SetColor(s16, Color4 *);
/* PC: ZOID_SetColor reads a colour as one word, so the colours passed to it are word-aligned, as IDO's
 * frame has them (docs/port/SANITIZE.md 3.2) */
#define PC_WORD_ALIGNED __attribute__((aligned(4)))
extern void MBOX_HideObject(s32, s32, s32), MBOX_ShowObject(s32, s32, s32);
extern void *MBOX_FindTexture_WORK(char *, s16 *, s32, s8, s32);
extern void ZOID_SetTexture(s16, void *, s32);
extern void WPitchUV(f32, f32 (*)[3]), WRollUV(f32, f32 (*)[3]);
extern void ScaleUV(f32 (*)[3], f32 (*)[3], f32);
/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
extern void *InitPane(s32, s32, s32, s32, s32, intptr_t, s32, s32);
extern void *NewMultiBlit(s16, s16, void *, s16);

/* initRecordScreen = initRecordScreen@recordscr.c (MAT3 reference/repos/mat3/decomp/src/GameSource/src/recordscr.c, GC 0x8008077C)
 * with initButtons@recordscr.c (GC 0x80084A5C) inlined; direct typed port. The caller-less `jr ra; nop` at 0x80393B34
 * is the deleted procedure of the inlined initButtons: it is defined here as the non-static recordscr_initButtons (not on keep, so
 * uld/umerge inline it exactly as a static) so that its stub carries a symbol and cannot be counted as extra words of a
 * neighbouring member (packaging rule). Names: gSelectMenuSelect recordscr_gSelectMenuSelect, gStatPlayer gStatPlayer,
 * setRecordState setRecordState, createBG particle_lifetime_set, buttonInfo recordscr_buttonInfo (23 x 64 B), buttonNames recordscr_buttonNames,
 * MBOX_NewObject MBOX_NewObject, MBOX_FindObject_WORK MBOX_FindObject_WORK, ZOID_SetColor ZOID_SetColor, MBOX_HideObject
 * MBOX_HideObject, MBOX_ShowObject MBOX_ShowObject, MBOX_FindTexture_WORK MBOX_FindTexture_WORK, ZOID_SetTexture ZOID_SetTexture,
 * WPitchUV/WRollUV WPitchUV/WRollUV, ScaleUV ScaleUV, buttonsInited/JustInited recordscr_buttonsInited/recordscr_buttonsJustInited,
 * ZeroCameras ZeroCameras, InitPane InitPane (viewPane recordscr_viewPane), NewMultiBlit NewMultiBlit (blit
 * recordscr_blit, mblit recordscr_mblit), gDeleteConfirm/gDeleteNotify/gYes/gSelectMenuClear gDeleteConfirm/854/850/80C, inited recordscr_inited.
 * N64 differences from the GC bodies (from the retail words): no gSelectPakIdx store; the button y is 55.2f (GC 59.2f);
 * where the GC object has its "no-op copies" (records 17, 21, 22) the N64 multiplies all three position coordinates by a
 * constant 1.0: written `*= (1.0f / 1.0f)`, a constant expression that cfe folds to 1.0f but does not treat as the literal
 * 1.0f (a plain `*= 1.0f` is deleted; a propagated `f32 scale = 1.0f` keeps the mul but stores through absolute addresses,
 * retail stores through the s0 base). initRecordScreen is defined before initButtons, as in the MAT3 file: as1 breaks
 * scheduling ties by source line and the &buttonColor address (caller line) must win over the loop preheader.
 * Own data: .data white colour template {255,255,255,255} (retail 0x803B65A8 = FFFFFFFF) and 7 .rodata floats
 * (-1.5707964f 55.2f 75.1f pi 87.1f -75.1f -87.1f at 0x803B9454..0x803B946C), all checked against image A (tools/rocheck.py). */
void recordscr_initButtons(void);

void initRecordScreen(void)
{
    recordscr_gSelectMenuSelect = 0;
    gStatPlayer = 0;
    setRecordState(0);
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

/* uninitButtons@recordscr.c (MAT3 GC uninitButtons): inlined into uninitRecordScreen uninitRecordScreen; its deleted
 * procedure is the retail stub recordscr_uninitButtons. Loop moved verbatim from uninitRecordScreen. */
void recordscr_uninitButtons(void)
{
    s32 i;
    for (i = 0; i < 23; i++) {
        if (recordscr_buttonInfo[i].fields[1] != -1) {
            ZOID_RemoveObject((s16)recordscr_buttonInfo[i].fields[1]);
            recordscr_buttonInfo[i].fields[1] = -1;
        }
    }
    recordscr_buttonsInited = 0;
}


void recordscr_initButtons(void)
{
    Color4 buttonColor PC_WORD_ALIGNED = {255, 255, 255, 255};
    void *tex;
    s16 texIdx;
    s32 buttonIdx;
    for (buttonIdx = 0; buttonIdx < 23; buttonIdx++) {
        if (recordscr_buttonInfo[buttonIdx].fields[1] == -1) {
            memcpy(recordscr_buttonInfo[buttonIdx].matrix, unitmatrix, 36);
            recordscr_buttonInfo[buttonIdx].fields[1] = MBOX_NewObject(
                MBOX_FindObject_WORK(recordscr_buttonNames[recordscr_buttonInfo[buttonIdx].fields[0]], 0, gNumModels - 1, 1),
                recordscr_buttonInfo[buttonIdx].matrix, -1,
                (recordscr_buttonInfo[buttonIdx].fields[0] == 0 || recordscr_buttonInfo[buttonIdx].fields[0] == 1 ||
                 recordscr_buttonInfo[buttonIdx].fields[0] == 2 || recordscr_buttonInfo[buttonIdx].fields[0] == 7 ||
                 recordscr_buttonInfo[buttonIdx].fields[0] == 8 || recordscr_buttonInfo[buttonIdx].fields[0] == 9 ||
                 recordscr_buttonInfo[buttonIdx].fields[0] == 10 || recordscr_buttonInfo[buttonIdx].fields[0] == 11) ? 0x42000 : 0);
            ZOID_SetColor(recordscr_buttonInfo[buttonIdx].fields[1], &buttonColor);
            MBOX_HideObject(recordscr_buttonInfo[buttonIdx].fields[1], 1, 15);
            MBOX_ShowObject(recordscr_buttonInfo[buttonIdx].fields[1], 0, 15);
        }
    }
    tex = MBOX_FindTexture_WORK(recordscr_title_texture_name, &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(recordscr_buttonInfo[17].fields[1], tex, -1);
    memcpy(recordscr_buttonInfo[17].matrix, unitmatrix, 36);
    WPitchUV(-1.5707964f, recordscr_buttonInfo[17].matrix);
    recordscr_buttonInfo[17].position[0] = 0.0f;
    recordscr_buttonInfo[17].position[1] = 115.0f;
    recordscr_buttonInfo[17].position[2] = 200.0f;
    recordscr_buttonInfo[17].position[0] *= (1.0f / 1.0f);
    recordscr_buttonInfo[17].position[1] *= (1.0f / 1.0f);
    recordscr_buttonInfo[17].position[2] *= (1.0f / 1.0f);
    tex = MBOX_FindTexture_WORK(recordscr_cursor_texture_name, &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(recordscr_buttonInfo[18].fields[1], tex, -1);
    tex = MBOX_FindTexture_WORK(recordscr_a_button_texture_name, &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(recordscr_buttonInfo[19].fields[1], tex, -1);
    memcpy(recordscr_buttonInfo[19].matrix, unitmatrix, 36);
    WPitchUV(-1.5707964f, recordscr_buttonInfo[19].matrix);
    recordscr_buttonInfo[19].position[0] = 75.1f;
    recordscr_buttonInfo[19].position[1] = 55.2f;
    recordscr_buttonInfo[19].position[2] = 100.0f;
    ScaleUV(recordscr_buttonInfo[19].matrix, recordscr_buttonInfo[19].matrix, 0.5f);
    memcpy(recordscr_buttonInfo[20].matrix, unitmatrix, 36);
    WPitchUV(-1.5707964f, recordscr_buttonInfo[20].matrix);
    WRollUV(3.1415927f, recordscr_buttonInfo[20].matrix);
    recordscr_buttonInfo[20].position[0] = 87.1f;
    recordscr_buttonInfo[20].position[1] = 55.2f;
    recordscr_buttonInfo[20].position[2] = 100.0f;
    tex = MBOX_FindTexture_WORK(recordscr_b_button_texture_name, &texIdx, 0, gNumModels - 1, 1);
    ZOID_SetTexture(recordscr_buttonInfo[21].fields[1], tex, -1);
    memcpy(recordscr_buttonInfo[21].matrix, unitmatrix, 36);
    WPitchUV(-1.5707964f, recordscr_buttonInfo[21].matrix);
    recordscr_buttonInfo[21].position[0] = -75.1f;
    recordscr_buttonInfo[21].position[1] = 55.2f;
    recordscr_buttonInfo[21].position[2] = 100.0f;
    recordscr_buttonInfo[21].position[0] *= (1.0f / 1.0f);
    recordscr_buttonInfo[21].position[1] *= (1.0f / 1.0f);
    recordscr_buttonInfo[21].position[2] *= (1.0f / 1.0f);
    ScaleUV(recordscr_buttonInfo[21].matrix, recordscr_buttonInfo[21].matrix, 0.5f);
    memcpy(recordscr_buttonInfo[22].matrix, unitmatrix, 36);
    WPitchUV(-1.5707964f, recordscr_buttonInfo[22].matrix);
    recordscr_buttonInfo[22].position[0] = -87.1f;
    recordscr_buttonInfo[22].position[1] = 55.2f;
    recordscr_buttonInfo[22].position[2] = 100.0f;
    recordscr_buttonInfo[22].position[0] *= (1.0f / 1.0f);
    recordscr_buttonInfo[22].position[1] *= (1.0f / 1.0f);
    recordscr_buttonInfo[22].position[2] *= (1.0f / 1.0f);
    recordscr_buttonsInited = 1;
    recordscr_buttonsJustInited = 1;
    ZeroCameras();
}

/* updateSelectButtons = updateSelectButtons@recordscr.c (MAT3 GC 0x80084f38, size 0xa60). Direct typed port.
 * Globals (N64 address = MAT3 name): recordscr_buttonInfo buttonInfo[23] (64-byte tMenuButton: buttonType, objIdx, angle,
 * mat.uvs[3][3], mat.uvs[3] position, alpha), recordscr_gButtonScale gButtonScale, recordscr_gButtonSpace gButtonSpace, recordscr_buttonX buttonX,
 * recordscr_maxY maxY, recordscr_minY minY, recordscr_buttonZ buttonZ, recordscr_cursorZ cursorZ, recordscr_cursorSpeed cursorSpeed,
 * recordscr_scrollSpeed scrollSpeed, recordscr_buttonsJustInited buttonsJustInited, recordscr_gSelectMenuVisible gSelectMenuVisible[11], recordscr_gSelectMenuSelect
 * gSelectMenuSelect, gSelectMenuClear gSelectMenuClear, gDeleteConfirm gDeleteConfirm, gDeleteNotify gDeleteNotify,
 * gFrameSeconds gFrameSeconds (volatile), gNumModels gNumModels (volatile), unitmatrix unitmatrix.
 * Calls: MBOX_ShowObject, MBOX_HideObject, MBOX_FindTexture_WORK =
 * MBOX_FindTexture_WORK, ZOID_SetTexture, ZOID_SetColor, WRollUV,
 * WPitchUV, ScaleUV = ScaleUV. */
extern f32 recordscr_gButtonScale, recordscr_gButtonSpace, recordscr_buttonX, recordscr_maxY, recordscr_minY, recordscr_buttonZ, recordscr_cursorZ;
extern f32 recordscr_cursorSpeed, recordscr_scrollSpeed;
extern volatile f32 gFrameSeconds;
extern f32 unitmatrix[3][3];
extern s32 recordscr_buttonsJustInited, recordscr_gSelectMenuSelect;
extern s8 recordscr_gSelectMenuVisible[], gSelectMenuClear, gDeleteConfirm, gDeleteNotify;
extern void *memcpy(void *, const void *, u32);
extern void MBOX_HideObject(s32, s32, s32), MBOX_ShowObject(s32, s32, s32);
extern void *MBOX_FindTexture_WORK(char *, s16 *, s32, s8, s32);
extern void ZOID_SetTexture(s16, void *, s32);
extern void WPitchUV(f32, f32 (*)[3]), WRollUV(f32, f32 (*)[3]);
extern void ScaleUV(f32 (*)[3], f32 (*)[3], f32);
extern f32 fabsf(f32);
#pragma intrinsic(fabsf)

void updateSelectButtons(void)
{
    s32 buttonIdx;
    s32 i;
    s32 unusedA, unusedB;
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
            if (buttonIdx == 17 || buttonIdx == 19 || buttonIdx == 21 || buttonIdx == 22)
                MBOX_ShowObject(recordscr_buttonInfo[buttonIdx].fields[1], 0, 15);
            else MBOX_HideObject(recordscr_buttonInfo[buttonIdx].fields[1], 1, 15);
        }
    } else {
        for (buttonIdx = 0; buttonIdx < 23; buttonIdx++) {
            if ((buttonIdx > 11 && buttonIdx < 13) || buttonIdx == 15 || buttonIdx == 16)
                MBOX_HideObject(recordscr_buttonInfo[buttonIdx].fields[1], 1, 15);
            else MBOX_ShowObject(recordscr_buttonInfo[buttonIdx].fields[1], 0, 15);
        }
        gSelectMenuCount = 0;
        whichItem = 0;
        for (i = 0; i < 11; i++) {
            if (recordscr_gSelectMenuVisible[i]) {
                gSelectMenuCount++;
                if (i < recordscr_gSelectMenuSelect) whichItem++;
            }
        }
        if (recordscr_gButtonScale * ((gSelectMenuCount - 1) * recordscr_gButtonSpace) > recordscr_maxY - recordscr_minY)
            selectedPos = recordscr_maxY - whichItem * (recordscr_maxY - recordscr_minY) / (gSelectMenuCount - 1);
        else selectedPos = recordscr_maxY - recordscr_gButtonScale * (whichItem * recordscr_gButtonSpace);
        memcpy(prevPos, recordscr_buttonInfo[18].position, 12);
        recordscr_buttonInfo[18].angle += 6.2831855f * gFrameSeconds;
        if (recordscr_buttonInfo[18].angle > 6.2831855f) recordscr_buttonInfo[18].angle -= 6.2831855f;
        memcpy(recordscr_buttonInfo[18].matrix, unitmatrix, 36);
        recordscr_buttonInfo[18].position[0] = recordscr_buttonX * recordscr_cursorZ / recordscr_buttonZ + -165.0f * recordscr_gButtonScale;
        recordscr_buttonInfo[18].position[1] = selectedPos * recordscr_cursorZ / recordscr_buttonZ;
        recordscr_buttonInfo[18].position[2] = recordscr_cursorZ;
        if (!recordscr_buttonsJustInited) {
            if (fabsf(recordscr_buttonInfo[18].position[1] - prevPos[1]) * 60 * recordscr_gButtonScale / 10 > recordscr_cursorSpeed) {
                recordscr_cursorSpeed = fabsf(recordscr_buttonInfo[18].position[1] - prevPos[1]) * 60 * recordscr_gButtonScale / 10;
                recordscr_cursorSpeed = recordscr_cursorSpeed < 540 * recordscr_gButtonScale ? recordscr_cursorSpeed : 540 * recordscr_gButtonScale;
                recordscr_cursorSpeed = recordscr_cursorSpeed > 180 * recordscr_gButtonScale ? recordscr_cursorSpeed : 180 * recordscr_gButtonScale;
            }
            if (recordscr_buttonInfo[18].position[1] > prevPos[1] + recordscr_cursorSpeed * gFrameSeconds)
                recordscr_buttonInfo[18].position[1] = prevPos[1] + recordscr_cursorSpeed * gFrameSeconds;
            else if (recordscr_buttonInfo[18].position[1] < prevPos[1] - recordscr_cursorSpeed * gFrameSeconds)
                recordscr_buttonInfo[18].position[1] = prevPos[1] - recordscr_cursorSpeed * gFrameSeconds;
            else recordscr_cursorSpeed = 180 * recordscr_gButtonScale;
        }
        WRollUV(recordscr_buttonInfo[18].angle, recordscr_buttonInfo[18].matrix);
        WPitchUV(1.5707964f, recordscr_buttonInfo[18].matrix);
        ScaleUV(recordscr_buttonInfo[18].matrix, recordscr_buttonInfo[18].matrix, recordscr_gButtonScale * 0.4f);
        buttonY = selectedPos + recordscr_gButtonScale * (whichItem * recordscr_gButtonSpace);
        for (buttonIdx = 0; buttonIdx < 11; buttonIdx++) {
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
            if (recordscr_buttonInfo[buttonIdx].fields[0] == 11) recordscr_buttonInfo[buttonIdx].position[0] = recordscr_buttonX + 0 * recordscr_gButtonScale;
            else if (recordscr_buttonInfo[buttonIdx].fields[0] == 9) recordscr_buttonInfo[buttonIdx].position[0] = recordscr_buttonX + -80 * recordscr_gButtonScale;
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
            ZOID_SetTexture(recordscr_buttonInfo[buttonIdx].fields[1], tex, -1);
            if (recordscr_buttonInfo[buttonIdx].position[1] > recordscr_maxY + recordscr_gButtonSpace * recordscr_gButtonScale ||
                recordscr_buttonInfo[buttonIdx].position[1] < recordscr_minY - recordscr_gButtonSpace * recordscr_gButtonScale) recordscr_buttonInfo[buttonIdx].alpha = 0;
            else if (recordscr_buttonInfo[buttonIdx].position[1] > recordscr_maxY)
                recordscr_buttonInfo[buttonIdx].alpha = 255.f - (recordscr_buttonInfo[buttonIdx].position[1] - recordscr_maxY) * 255.0f / (recordscr_gButtonSpace * recordscr_gButtonScale);
            else if (recordscr_buttonInfo[buttonIdx].position[1] < recordscr_minY)
                recordscr_buttonInfo[buttonIdx].alpha = 255.f - (recordscr_minY - recordscr_buttonInfo[buttonIdx].position[1]) * 255.0f / (recordscr_gButtonSpace * recordscr_gButtonScale);
            else recordscr_buttonInfo[buttonIdx].alpha = 255;
            if (!recordscr_gSelectMenuVisible[buttonIdx]) recordscr_buttonInfo[buttonIdx].alpha = 0;
            buttonColor.alpha = recordscr_buttonInfo[buttonIdx].alpha;
            ZOID_SetColor(recordscr_buttonInfo[buttonIdx].fields[1], &buttonColor);
            if (!recordscr_gSelectMenuVisible[buttonIdx] || !recordscr_buttonInfo[buttonIdx].alpha) MBOX_HideObject(recordscr_buttonInfo[buttonIdx].fields[1], 1, 15);
            else MBOX_ShowObject(recordscr_buttonInfo[buttonIdx].fields[1], 0, 15);
            if (buttonIdx == 1) {
                recordscr_buttonInfo[14].position[0] = recordscr_buttonX + 160 * recordscr_gButtonScale;
                recordscr_buttonInfo[14].position[1] = recordscr_buttonInfo[buttonIdx].position[1];
                recordscr_buttonInfo[14].position[2] = recordscr_buttonInfo[buttonIdx].position[2];
                recordscr_buttonInfo[14].angle = recordscr_buttonInfo[buttonIdx].angle;
                ScaleUV(unitmatrix, recordscr_buttonInfo[14].matrix, recordscr_gButtonScale);
                WPitchUV(recordscr_buttonInfo[14].angle - 1.5707964f, recordscr_buttonInfo[14].matrix);
                ZOID_SetTexture(recordscr_buttonInfo[14].fields[1], tex, -1);
                recordscr_buttonInfo[14].alpha = recordscr_buttonInfo[buttonIdx].alpha;
                ZOID_SetColor(recordscr_buttonInfo[14].fields[1], &buttonColor);
                if (!recordscr_gSelectMenuVisible[buttonIdx] || !recordscr_buttonInfo[buttonIdx].alpha) MBOX_HideObject(recordscr_buttonInfo[14].fields[1], 1, 15);
                else MBOX_ShowObject(recordscr_buttonInfo[14].fields[1], 0, 15);
            } else if (recordscr_gSelectMenuSelect >= 3 && recordscr_gSelectMenuSelect < 11) {
                if (buttonIdx == recordscr_gSelectMenuSelect) {
                    recordscr_buttonInfo[13].position[0] = recordscr_buttonX + 120 * recordscr_gButtonScale;
                    recordscr_buttonInfo[13].position[1] = recordscr_buttonInfo[buttonIdx].position[1];
                    recordscr_buttonInfo[13].position[2] = recordscr_buttonInfo[buttonIdx].position[2];
                    if (gSelectMenuClear) {
                        if (recordscr_buttonInfo[13].angle < 3.1415927f) recordscr_buttonInfo[13].angle += 12.566371f * gFrameSeconds;
                        if (recordscr_buttonInfo[13].angle > 3.1415927f) recordscr_buttonInfo[13].angle = 3.1415927f;
                        if (recordscr_buttonsJustInited) recordscr_buttonInfo[13].angle = 3.1415927f;
                    } else {
                        if (recordscr_buttonInfo[13].angle > 0) recordscr_buttonInfo[13].angle -= 12.566371f * gFrameSeconds;
                        if (recordscr_buttonInfo[13].angle < 0) recordscr_buttonInfo[13].angle = 0;
                        if (recordscr_buttonsJustInited) recordscr_buttonInfo[13].angle = 0;
                    }
                    ScaleUV(unitmatrix, recordscr_buttonInfo[13].matrix, recordscr_gButtonScale);
                    WPitchUV(recordscr_buttonInfo[13].angle - 1.5707964f, recordscr_buttonInfo[13].matrix);
                    if (recordscr_buttonInfo[13].angle > 1.5707964f)
                        tex = MBOX_FindTexture_WORK("BUTTON_SELECT", &texIdx, 0, gNumModels - 1, 1);
                    else tex = MBOX_FindTexture_WORK("BUTTON", &texIdx, 0, gNumModels - 1, 1);
                    ZOID_SetTexture(recordscr_buttonInfo[13].fields[1], tex, -1);
                    recordscr_buttonInfo[13].alpha = recordscr_buttonInfo[buttonIdx].alpha;
                    ZOID_SetColor(recordscr_buttonInfo[13].fields[1], &buttonColor);
                    if (!recordscr_gSelectMenuVisible[buttonIdx] || !recordscr_buttonInfo[13].alpha) MBOX_HideObject(recordscr_buttonInfo[13].fields[1], 1, 15);
                    else MBOX_ShowObject(recordscr_buttonInfo[13].fields[1], 0, 15);
                }
            } else {
                recordscr_buttonInfo[13].angle = 0;
                MBOX_HideObject(recordscr_buttonInfo[13].fields[1], 1, 15);
            }
            if (recordscr_gSelectMenuVisible[buttonIdx] == 1) buttonY -= recordscr_gButtonSpace * recordscr_gButtonScale;
        }
        for (; buttonIdx < 13; buttonIdx++) MBOX_HideObject(recordscr_buttonInfo[buttonIdx].fields[1], 1, 15);
        if (clearScrollSpeed == 1) recordscr_scrollSpeed = 0;
        recordscr_buttonsJustInited = 0;
    }
}

/* : recordscr_updateButtons = MAT3 recordscr.c updateButtons (GC 0x80084EE8, 0x50 B) with updateViewButtons
 * (GC 0x80085998, 0x8B4 B) inlined by umerge; updateSelectButtons (GC 0x80084F38) stays the call updateSelectButtons.
 * Direct typed port of the MAT3 bodies; local list and ORDER from the GC DWARF (rush_d_dwarf.c: buttonIdx, i,
 * prevPos, gViewMenuCount, whichItem, buttonY, selectedPos, clearScrollSpeed, tex, texIdx, buttonColor; the MAT3 C
 * moved clearScrollSpeed up for CodeWarrior).  clearScrollSpeed keeps its initialiser, gViewMenuCount/whichItem are
 * assigned after the declarations (as in MAT3 updateSelectButtons): that statement order puts `li t7,1` in the
 * dispatch delay slot.  updateViewButtons (symbol updateViewButtons, unique so it cannot clash with other
 * groups) is a NON-static function off `keep` (umerge inlines it; its deleted stub is retail's caller-less 0x803915E0).
 * N64 differences (from the retail displacements/constants): 23 buttons at recordscr_buttonInfo (0x40 stride), statics
 * gButtonScale, gButtonSpace, buttonX, maxY, minY, buttonZ, cursorZ = recordscr_gButtonScale..recordscr_cursorZ, cursorSpeed/scrollSpeed
 * = recordscr_cursorSpeed/DC, buttonsInited/JustInited = recordscr_buttonsInited/D4, gRecordState, gViewMenuVisible =
 * gViewMenuVisible, gViewMenuSelect; the alpha fades linearly over one pitch
 * (255 - d * 255 / (space * scale)) instead of GC's flat 80; the two trailing MBOX_HideObject(buttonInfo[19/20])
 * calls are absent; associations follow the matched carscr updateButtons (`-140.0f * scale + buttonX`,
 * `scale * 0.4f`, fabsf on the right of the speed comparisons).  Callees: MBOX_HideObject,
 * MBOX_ShowObject, ZOID_SetTexture, ZOID_SetColor,
 * MBOX_FindTexture_WORK, WPitchUV, WRollUV, ScaleUV = ScaleUV.
 * SHAPING (disclosed frame residual): pad0 and pad1 are unused s32 locals of updateButtons.  Retail frame 144; the
 * same source without them 136 (every other offset already retail: prevPos 108, clearScrollSpeed 91, texIdx 82,
 * buttonColor 76, temp 52); 2 slots x 4 B = 8 B above the inlined callee's locals.  Two unused int parameters of
 * updateViewButtons give the identical object.  No pad arrays.
 */
typedef unsigned short u16;
extern void updateSelectButtons(void);
extern f32 recordscr_gButtonScale, recordscr_gButtonSpace, recordscr_buttonX, recordscr_maxY;
extern f32 recordscr_minY, recordscr_buttonZ, recordscr_cursorZ;
extern f32 recordscr_cursorSpeed, recordscr_scrollSpeed;
extern volatile f32 gFrameSeconds;
extern char recordscr_select_texture_name[], recordscr_button_texture_name[];
extern f32 fabsf(f32);
#pragma intrinsic(fabsf)

void updateViewButtons(void);

void recordscr_updateButtons(void)
{
    s32 pad0, pad1;
    if (recordscr_buttonsInited) {
        switch (gRecordState) {
        case 0: updateSelectButtons(); break;
        case 1: updateViewButtons(); break;
        }
    }
}

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
    for (i = 0; i < 2; i++) {
        if (gViewMenuVisible[i]) {
            gViewMenuCount++;
            if (i < gViewMenuSelect) whichItem++;
        }
    }
    if ((gViewMenuCount - 1) * recordscr_gButtonSpace * recordscr_gButtonScale > recordscr_maxY - recordscr_minY)
        selectedPos = recordscr_maxY - whichItem * (recordscr_maxY - recordscr_minY) / (gViewMenuCount - 1);
    else selectedPos = recordscr_maxY - whichItem * recordscr_gButtonSpace * recordscr_gButtonScale;
    memcpy(prevPos, recordscr_buttonInfo[18].position, 12);
    recordscr_buttonInfo[18].angle += 6.2831855f * gFrameSeconds;
    if (recordscr_buttonInfo[18].angle > 6.2831855f) recordscr_buttonInfo[18].angle -= 6.2831855f;
    memcpy(recordscr_buttonInfo[18].matrix, unitmatrix, 36);
    recordscr_buttonInfo[18].position[0] = recordscr_buttonX * recordscr_cursorZ / recordscr_buttonZ + -165.0f * recordscr_gButtonScale;
    recordscr_buttonInfo[18].position[1] = selectedPos * recordscr_cursorZ / recordscr_buttonZ;
    recordscr_buttonInfo[18].position[2] = recordscr_cursorZ;
    if (!recordscr_buttonsJustInited) {
        if (recordscr_cursorSpeed < fabsf(recordscr_buttonInfo[18].position[1] - prevPos[1]) * 60.0f * recordscr_gButtonScale / 10.0f) {
            recordscr_cursorSpeed = fabsf(recordscr_buttonInfo[18].position[1] - prevPos[1]) * 60.0f * recordscr_gButtonScale / 10.0f;
            recordscr_cursorSpeed = recordscr_cursorSpeed < 540.0f * recordscr_gButtonScale ? recordscr_cursorSpeed : 540.0f * recordscr_gButtonScale;
            recordscr_cursorSpeed = recordscr_cursorSpeed > 180.0f * recordscr_gButtonScale ? recordscr_cursorSpeed : 180.0f * recordscr_gButtonScale;
        }
        if (recordscr_buttonInfo[18].position[1] > prevPos[1] + recordscr_cursorSpeed * gFrameSeconds)
            recordscr_buttonInfo[18].position[1] = prevPos[1] + recordscr_cursorSpeed * gFrameSeconds;
        else if (recordscr_buttonInfo[18].position[1] < prevPos[1] - recordscr_cursorSpeed * gFrameSeconds)
            recordscr_buttonInfo[18].position[1] = prevPos[1] - recordscr_cursorSpeed * gFrameSeconds;
        else recordscr_cursorSpeed = 180.0f * recordscr_gButtonScale;
    }
    WRollUV(recordscr_buttonInfo[18].angle, recordscr_buttonInfo[18].matrix);
    WPitchUV(1.5707964f, recordscr_buttonInfo[18].matrix);
    ScaleUV(recordscr_buttonInfo[18].matrix, recordscr_buttonInfo[18].matrix, recordscr_gButtonScale * 0.4f);
    buttonY = selectedPos + whichItem * recordscr_gButtonSpace * recordscr_gButtonScale;
    for (buttonIdx = 0; buttonIdx < 11; buttonIdx++) MBOX_HideObject(recordscr_buttonInfo[buttonIdx].fields[1], 1, 15);
    for (buttonIdx = 11; buttonIdx < 13; buttonIdx++) {
        if (buttonIdx - 11 == gViewMenuSelect) {
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
        ZOID_SetTexture((s16)recordscr_buttonInfo[buttonIdx].fields[1], tex, -1);
        if (recordscr_buttonInfo[buttonIdx].position[1] > recordscr_maxY + recordscr_gButtonSpace * recordscr_gButtonScale || recordscr_buttonInfo[buttonIdx].position[1] < recordscr_minY - recordscr_gButtonSpace * recordscr_gButtonScale) recordscr_buttonInfo[buttonIdx].alpha = 0;
        else if (recordscr_buttonInfo[buttonIdx].position[1] > recordscr_maxY) recordscr_buttonInfo[buttonIdx].alpha = 255.0f - (recordscr_buttonInfo[buttonIdx].position[1] - recordscr_maxY) * 255 / (recordscr_gButtonSpace * recordscr_gButtonScale);
        else if (recordscr_buttonInfo[buttonIdx].position[1] < recordscr_minY) recordscr_buttonInfo[buttonIdx].alpha = 255.0f - (recordscr_minY - recordscr_buttonInfo[buttonIdx].position[1]) * 255 / (recordscr_gButtonSpace * recordscr_gButtonScale);
        else recordscr_buttonInfo[buttonIdx].alpha = 255;
        if (!gViewMenuVisible[buttonIdx - 11]) recordscr_buttonInfo[buttonIdx].alpha = 0;
        buttonColor.alpha = recordscr_buttonInfo[buttonIdx].alpha;
        ZOID_SetColor((s16)recordscr_buttonInfo[buttonIdx].fields[1], &buttonColor);
        if (!gViewMenuVisible[buttonIdx - 11] || !recordscr_buttonInfo[buttonIdx].alpha) MBOX_HideObject(recordscr_buttonInfo[buttonIdx].fields[1], 1, 15);
        else MBOX_ShowObject(recordscr_buttonInfo[buttonIdx].fields[1], 0, 15);
        if (buttonIdx - 11 == 0) {
            recordscr_buttonInfo[15].position[0] = 100.0f * recordscr_gButtonScale + recordscr_buttonX;
            recordscr_buttonInfo[15].position[1] = recordscr_buttonInfo[buttonIdx].position[1];
            recordscr_buttonInfo[15].position[2] = recordscr_buttonInfo[buttonIdx].position[2];
            recordscr_buttonInfo[15].angle = recordscr_buttonInfo[buttonIdx].angle;
            ScaleUV(unitmatrix, recordscr_buttonInfo[15].matrix, recordscr_gButtonScale);
            WPitchUV(recordscr_buttonInfo[15].angle - 1.5707964f, recordscr_buttonInfo[15].matrix);
            ZOID_SetTexture((s16)recordscr_buttonInfo[15].fields[1], tex, -1);
            recordscr_buttonInfo[15].alpha = recordscr_buttonInfo[buttonIdx].alpha;
            ZOID_SetColor((s16)recordscr_buttonInfo[15].fields[1], &buttonColor);
            if (!gViewMenuVisible[buttonIdx] || !recordscr_buttonInfo[buttonIdx].alpha) MBOX_HideObject(recordscr_buttonInfo[15].fields[1], 1, 15);
            else MBOX_ShowObject(recordscr_buttonInfo[15].fields[1], 0, 15);
        } else if (buttonIdx - 11 == 1) {
            recordscr_buttonInfo[16].position[0] = 100.0f * recordscr_gButtonScale + recordscr_buttonX;
            recordscr_buttonInfo[16].position[1] = recordscr_buttonInfo[buttonIdx].position[1];
            recordscr_buttonInfo[16].position[2] = recordscr_buttonInfo[buttonIdx].position[2];
            recordscr_buttonInfo[16].angle = recordscr_buttonInfo[buttonIdx].angle;
            ScaleUV(unitmatrix, recordscr_buttonInfo[16].matrix, recordscr_gButtonScale);
            WPitchUV(recordscr_buttonInfo[16].angle - 1.5707964f, recordscr_buttonInfo[16].matrix);
            ZOID_SetTexture((s16)recordscr_buttonInfo[16].fields[1], tex, -1);
            recordscr_buttonInfo[16].alpha = recordscr_buttonInfo[buttonIdx].alpha;
            ZOID_SetColor((s16)recordscr_buttonInfo[16].fields[1], &buttonColor);
            if (!gViewMenuVisible[buttonIdx - 11] || !recordscr_buttonInfo[buttonIdx].alpha) MBOX_HideObject(recordscr_buttonInfo[16].fields[1], 1, 15);
            else MBOX_ShowObject(recordscr_buttonInfo[16].fields[1], 0, 15);
        }
        if (gViewMenuVisible[buttonIdx - 11] == 1) buttonY -= recordscr_gButtonSpace * recordscr_gButtonScale;
    }
    for (; buttonIdx < 23; buttonIdx++) {
        if (buttonIdx < 15) MBOX_HideObject(recordscr_buttonInfo[buttonIdx].fields[1], 1, 15);
        else MBOX_ShowObject(recordscr_buttonInfo[buttonIdx].fields[1], 0, 15);
    }
    if (clearScrollSpeed == 1) recordscr_scrollSpeed = 0.0f;
    recordscr_buttonsJustInited = 0;
}
