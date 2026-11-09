/* Profiles and ghosts on the paks: creating, reading, checking and repairing them, and the save server that reads a
 * pak's profiles and ghosts when it comes in (MAT3 save.c). The profile's file (tSaveData) is still reached here by
 * its byte offsets. */
#include "game/game.h"


#define F(expr,type,offset) (*(type *)((u8 *)(expr)+(offset)))
/* PC: the save records' host layouts (pc_prelude.h, docs/port/POINTERS.md 9.10): PSF_OFS for the pak file record
 * (*fileh), PROFILE_OFS for the profile record (*player), GHOST_OFS for a ghost. The ghost header is file data, so
 * it keeps its N64 layout; its sample buffer `start` is a window address. */
#define SLOT_DATA PROFILE_OFS(44)   /* SaveSlot.data */
#define N64W(x) PC_N64W(x)
typedef void **Handle;

void *memset(void *, s32, u32);
void *memcpy(void *, const void *, u32);
s32 sprintf(char *, const char *, ...);

void SavePlayerOption(Handle, u8, s8);

Handle PS_CreateFile(s32, char *, char *, u32, s32, s32);
void PS_WriteFile(Handle, void *, u32);

/* Gives the profile h a fresh file: its record cleared, the data zeroed, revision PROFILE_REVISION, the default
 * controls with rumble, every car's default settings, every option's default (the race type settings for every
 * track), the default car and view for every race type. */
void InitPlayer(Handle h) {
    u8 *data;
    s32 i;
    s8 saved;

    memset(*h, 0, SLOT_DATA);
    data = *F(*h, Handle, SLOT_DATA);
    memset(data, 0, 2064);
    data[12] = PROFILE_REVISION;
    SetDefaultControls(data + 60);
    data[70] = 1;
    F(data, s32, 56) = CalcChecksumInternal(data + 60, 11);
    memcpy((u8 *)*h + PROFILE_OFS(33), data + 60, 11);
    for (i = 0; i < 13; i++) {
        SetDefaultCarSettings(h, i);
    }
    for (i = 0; i < 21; i++) {
        SavePlayerOption(h, i, GetDefaultOption(i));
    }
    saved = gTrackNum;
    for (gTrackNum = 0; gTrackNum < 19; gTrackNum++) {
        for (i = 21; i < 41; i++) {
            SavePlayerOption(h, i, GetDefaultOption(i));
        }
    }
    gTrackNum = saved;
    for (data[71] = 0; data[71] < 7; data[71]++) {
        SavePlayerCar(h, 2);
        SavePlayerCarView(h, 2);
    }
    data[71] = 0;
}

VIEW(s8[], gOptions);
#define gOptions gOptions_view

VIEW(s16[], gCheatState);
#define gCheatState gCheatState_view

/* Checks the profile's file: its revision and every part's checksum. Returns 1 when something is wrong. With repair
 * each bad part is reset (the options and settings from the game's, the controls to the defaults, the records
 * zeroed) and written; a file of another revision is deleted instead (and 0 returned). Original game: only the first
 * 6 of the 12 race stats are checked (MAT3 alike); the other 6 (the tracks backwards) are never repaired. */
s32 CheckAndRepairPlayer(Handle h, s32 repair) {
    s32 dirty;
    u8 *save;
    u8 *p;
    s32 i;
    s32 j;
    s8 saved;

    dirty = 0;
    save = *F(*h, Handle, SLOT_DATA);
    if (save[12] != PROFILE_REVISION) {
        if (repair) {
            PS_DeleteFile(F(*h, Handle, PROFILE_OFS(8)));
            return 0;
        }
        dirty = 1;
    }
    if (CalcChecksumInternal(save + 4, (save + 28) - (save + 4)) != F(save, s32, 0)) {
        dirty = 1;
        if (repair) {
            F(save, s32, 0) = CalcChecksumInternal(save + 4, (save + 28) - (save + 4));
            PS_WriteFile(F(*h, Handle, PROFILE_OFS(8)), save, 4);
        }
    }
    if (CalcChecksumInternal(save + 32, 21) != F(save, s32, 28)) {
        dirty = 1;
        if (repair) {
            for (i = 0; i < 21; i++) {
                SavePlayerOption(h, i, gOptions[i]);
            }
        }
    }
    if (CalcChecksumInternal(save + 60, 11) != F(save, s32, 56)) {
        dirty = 1;
        if (repair) {
            SetDefaultControls((u8 *)*h + PROFILE_OFS(33));
            ((u8 *)*h)[PROFILE_OFS(43)] = 1;
            memcpy(save + 60, (u8 *)*h + PROFILE_OFS(33), 11);
            F(save, s32, 56) = CalcChecksumInternal(save + 60, 11);
            PS_WriteFile(F(*h, Handle, PROFILE_OFS(8)), save + 56, 15);
        }
    }
    if (CalcChecksumInternal(save + 76, 16) != F(save, s32, 72)) {
        dirty = 1;
        if (repair) {
            saved = gTrackNum;
            gTrackNum = 0;
            for (j = 21; j < 41; j++) {
                SavePlayerOption(h, j, gOptions[j]);
            }
            gTrackNum = saved;
            memset(save + 72, 0, 20);
            PS_WriteFile(F(*h, Handle, PROFILE_OFS(8)), save + 72, 20);
        }
    }
    if (CalcChecksumInternal(save + 96, 12) != F(save, s32, 92)) {
        dirty = 1;
        if (repair) {
            saved = gTrackNum;
            gTrackNum = 6;
            for (j = 21; j < 41; j++) {
                SavePlayerOption(h, j, gOptions[j]);
            }
            gTrackNum = saved;
            memset(save + 92, 0, 16);
            PS_WriteFile(F(*h, Handle, PROFILE_OFS(8)), save + 92, 16);
        }
    }
    if (CalcChecksumInternal(save + 112, 16) != F(save, s32, 108)) {
        dirty = 1;
        if (repair) {
            saved = gTrackNum;
            gTrackNum = 14;
            for (j = 21; j < 41; j++) {
                SavePlayerOption(h, j, gOptions[j]);
            }
            gTrackNum = saved;
            memset(save + 108, 0, 20);
            PS_WriteFile(F(*h, Handle, PROFILE_OFS(8)), save + 108, 20);
        }
    }
    if (CalcChecksumInternal(save + 132, 8) != F(save, s32, 128)) {
        dirty = 1;
        if (repair) {
            saved = gTrackNum;
            gTrackNum = 18;
            for (j = 21; j < 41; j++) {
                SavePlayerOption(h, j, gOptions[j]);
            }
            gTrackNum = saved;
            memset(save + 128, 0, 12);
            PS_WriteFile(F(*h, Handle, PROFILE_OFS(8)), save + 128, 12);
        }
    }
    for (i = 0; i < 6; i++) {
        if (CalcChecksumInternal(save + 144 + i * 96, 92) != F(save + i * 96, s32, 140)) {
            dirty = 1;
            if (repair) {
                memset(save + 140 + i * 96, 0, 96);
                PS_WriteFile(F(*h, Handle, PROFILE_OFS(8)), save + 140 + i * 96, 96);
            }
        }
    }
    for (i = 0; i < 4; i++) {
        if (CalcChecksumInternal(save + 1296 + i * 64, 60) != F(save + i * 64, s32, 1292)) {
            dirty = 1;
            if (repair) {
                memset(save + 1292 + i * 64, 0, 64);
                PS_WriteFile(F(*h, Handle, PROFILE_OFS(8)), save + 1292 + i * 64, 64);
            }
        }
    }
    for (i = 0; i < 8; i++) {
        if (CalcChecksumInternal(save + 1552 + i * 12, 8) != F(save + i * 12, s32, 1548)) {
            dirty = 1;
            if (repair) {
                memset(save + 1548 + i * 12, 0, 12);
                PS_WriteFile(F(*h, Handle, PROFILE_OFS(8)), save + 1548 + i * 12, 12);
            }
        }
    }
    for (i = 0; i < 1; i++) {
        if (CalcChecksumInternal(save + 1648 + i * 24, 20) != F(save + i * 24, s32, 1644)) {
            dirty = 1;
            if (repair) {
                memset(save + 1644 + i * 24, 0, 24);
                PS_WriteFile(F(*h, Handle, PROFILE_OFS(8)), save + 1644 + i * 24, 24);
            }
        }
    }
    for (i = 0; i < 4; i++) {
        if (CalcChecksumInternal(save + 1672 + i * 28, 24) != F(save + i * 28, s32, 1668)) {
            dirty = 1;
            if (repair) {
                memset(save + 1668 + i * 28, 0, 28);
                PS_WriteFile(F(*h, Handle, PROFILE_OFS(8)), save + 1668 + i * 28, 28);
            }
        }
    }
    if (CalcChecksumInternal(save + 1784, 72) != F(save, s32, 1780)) {
        dirty = 1;
        if (repair) {
            memset(save + 1780, 0, 76);
            PS_WriteFile(F(*h, Handle, PROFILE_OFS(8)), save + 1780, 76);
        }
    }
    for (i = 0; i < 13; i++) {
        p = save + i * 16;
        if (CalcChecksumInternal(p + 1860, 12) != F(p, s32, 1856)) {
            dirty = 1;
            if (repair) {
                SetDefaultCarSettings(h, i);
                PS_WriteFile(F(*h, Handle, PROFILE_OFS(8)), p + 1856, 16);
            }
        }
    }
    return dirty;
}

/* Reads the profile's file into memory, unless it is (its data handle), and checks and repairs it; takes its
 * controls; the cheat profile turns every cheat on. Returns 0 when it can't be read or repaired. */
s32 ReadPlayerData(Handle h) {
    u8 *obj;
    void **data;
    s32 i;

    obj = *h;
    if (F(obj, void *, SLOT_DATA) != NULL) {
        return 1;
    }
    PS_ReadPartialFile(F(obj, Handle, PROFILE_OFS(8)), 0, F(*F(obj, Handle, PROFILE_OFS(8)), s32, PSF_OFS(64)));
    data = F(*F(obj, Handle, PROFILE_OFS(8)), void **, PSF_OFS(72));
    if (data == NULL) {
        return 0;
    }
    F(obj, void **, SLOT_DATA) = data;
    if (CheckAndRepairPlayer(h, 0) && !CheckAndRepairPlayer(h, 1)) {
        return 0;
    }
    SavePlayerControls(h, obj + PROFILE_OFS(33));
    if (game_strcmp(obj + PROFILE_OFS(20), cheat_profile_name) == 0) {
        gCheatActive[21] = 1;
        for (i = 1; i < 21; i++) {
        }
        gCheatState[14] = 1;
        gCheatState[15] = 1;
        gCheatState[16] = 1;
        gSaveGhostFiles = 1;
    }
    return 1;
}

/* Deletes the profile's file (once it can be read), or for a profile without one resets it (InitPlayer). */
void DeletePlayer(Handle h) {
    if (F(*h, intptr_t, PROFILE_OFS(8)) != 0) {
        if (ReadPlayerData(h)) {
            PS_DeleteFile(F(*h, Handle, PROFILE_OFS(8)));
        }
    } else {
        InitPlayer(h);
    }
}

typedef struct { char c[9]; } Name9;

VIEW(u8[], gPlayerFiles);
#define gPlayerFiles gPlayerFiles_view
VIEW(u8[][24], gPS_Files);
#define gPS_Files gPS_Files_view
VIEW(int[], gScheduler);
#define gScheduler gScheduler_view

/* 10 to the power exp (MAT3 _Pow). */
s32 Pow(s32 exp) {
    s32 result = 1;
    while (exp--) {
        result *= 10;
    }
    return result;
}

/* The profile record of pak's file number slot (player_profile_records), through its handle (MAT3 _NewPlayer). */
Handle NewPlayer(s32 pak, s32 slot) {
    Handle player;

    player = &player_profile_handles[pak][slot];
    *player = player_profile_records[pak][slot];
    return player;
}

/* A profile keeps its handle's N64 address: the handle's own (pc_n64_identity, pc_prelude.h; LOCKSTEP.md 5). */
#define N64A(x) pc_n64_identity(x)
/* Creates a profile named name on pak's pak: a file named after the name (its first six letters, others made '*',
 * numbered when the name is taken), a fresh file (InitPlayer) with the profile's id (the creation time, the
 * record and the pak) and name, written at once; the profile joins the profile list in name order. Returns its
 * handle, or NULL when the file can't be created. */
void **CreatePlayer(s32 pak, u8 *name) {
    Handle player;
    Handle before;
    u8 *p;
    u8 *data;
    Handle fileh;
    char file[17];
    s32 i;
    s32 attempt;
    char *src;
    s32 c;
    char digits[32];

    for (attempt = 0; ; attempt++) {
        memset(file, 0, 17);
        *(Name9 *)file = *(Name9 *)player_file_prefix;
        for (i = 0; i < 6; i++) {
            c = name[i];
            if (c == 0) {
                break;
            }
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == ':' || c == '!' ||
                c == '@' || c == '*' || c == '-' || c == '=' || c == '\'' || c == ' ') {
                file[8 + i] = name[i];
            } else {
                file[8 + i] = '*';
            }
        }
        if (attempt > 0) {
            sprintf(digits, player_file_digits_format, 5, attempt);
            src = &digits[4];
            for (i = 4; i >= 0; i--) {
                if (attempt >= Pow(4 - i) || *src != '0' || file[9 + i] == 0) {
                    file[9 + i] = *src;
                }
                src--;
            }
        }
        for (before = F(gPS_Files[pak], Handle, 8); before != NULL; before = *(Handle *)*before) {
            if (game_strcmp((u8 *)*before + PSF_OFS(18), file) == 0 && game_strcmp((u8 *)*before + PSF_OFS(53), player_file_ext_match) == 0) {
                break;
            }
        }
        if (before == NULL) {
            break;
        }
    }
    fileh = PS_CreateFile(pak, file, player_file_ext, PROFILE_FILE_SIZE, GAME_COMPANY_CODE, GAME_CODE);
    if (fileh == NULL) {
        return NULL;
    }
    player = NewPlayer(((u8 *)*fileh)[PSF_OFS(16)], ((u8 *)*fileh)[PSF_OFS(17)]);
    p = *player;
    data = *(F(p, Handle, SLOT_DATA) = F(*fileh, Handle, PSF_OFS(72)));
    InitPlayer(player);
    F(p, Handle, PROFILE_OFS(8)) = fileh;
    F(*fileh, void *, PSF_OFS(8)) = RemovePlayerFile;
    F(*F(p, Handle, PROFILE_OFS(8)), void *, PSF_OFS(12)) = MovePlayerFile;
    F(data, s32, 4) = gScheduler[159];
    F(data, s32, 8) = (N64A(player) << 3) | pak;
    game_strcpy(data + 13, name);
    F(data, s32, 0) = CalcChecksumInternal(data + 4, (data + 28) - (data + 4));
    memcpy(p + PROFILE_OFS(20), data + 13, 13);
    memcpy(p + PROFILE_OFS(12), data + 4, 8);
    PS_WriteFile(F(p, Handle, PROFILE_OFS(8)), data, PROFILE_FILE_SIZE);
    PS_FlushFile(F(p, Handle, PROFILE_OFS(8)), 0);
    PS_FlushPak(((u8 *)*F(p, Handle, PROFILE_OFS(8)))[PSF_OFS(16)], 1);
    for (before = F(gPlayerFiles, Handle, 8); before != NULL; before = *(Handle *)*before) {
        if (game_strcmp((u8 *)*before + PROFILE_OFS(20), p + PROFILE_OFS(20)) > 0) {
            break;
        }
    }
    InsertLLE(gPlayerFiles, player, before);
    return player;
}



s8 GetPlayerOption(Handle player, u8 option);      /* GC GetPlayerOption */

/* Opens the profile in the pak file fileh (nothing if it is open already): reads and checks it, takes its name, id
 * and controls; the first profile read when no options are set gives the game its options (and language and screen
 * offset). It joins the profile list in name order and the high score tables, and its data is dropped from memory.
 * Returns its handle, or NULL. */
void **ReadPlayer(void **fileh) {
    Handle player;
    u8 *p;
    Handle before;
    s32 i;

    for (player = F(gPlayerFiles, Handle, 8); player != NULL; player = *(Handle *)*player) {
        if (F(*player, Handle, PROFILE_OFS(8)) == fileh) {
            return NULL;
        }
    }
    PS_ReadPartialFile(fileh, 0, F(*fileh, s32, PSF_OFS(64)));
    if (F(*fileh, void *, PSF_OFS(72)) == NULL) {
        return NULL;
    }
    player = NewPlayer(((u8 *)*fileh)[PSF_OFS(16)], ((u8 *)*fileh)[PSF_OFS(17)]);
    p = *player;
    F(p, Handle, PROFILE_OFS(8)) = fileh;
    F(*fileh, void *, PSF_OFS(8)) = RemovePlayerFile;
    F(*F(p, Handle, PROFILE_OFS(8)), void *, PSF_OFS(12)) = MovePlayerFile;
    F(p, Handle, SLOT_DATA) = F(*F(p, Handle, PROFILE_OFS(8)), Handle, PSF_OFS(72));
    if (CheckAndRepairPlayer(player, 0) && !CheckAndRepairPlayer(player, 1)) {
        return NULL;
    }
    memcpy(p + PROFILE_OFS(20), (u8 *)*F(p, Handle, SLOT_DATA) + 13, 13);
    memcpy(p + PROFILE_OFS(12), (u8 *)*F(p, Handle, SLOT_DATA) + 4, 8);
    memcpy(p + PROFILE_OFS(33), (u8 *)*F(p, Handle, SLOT_DATA) + 60, 11);
    if (!gGlobalsSet && F(gPlayerFiles, Handle, 8) == NULL) {
        for (i = 0; i < 21; i++) {
            gOptions[i] = GetPlayerOption(player, i);
        }
        gOptions[17] = 1;
        gLang = gOptions[16] = GetPlayerOption(player, 16);
        LoadTextWave();
        video_set_offset(gOptions[19], gOptions[20]);
    }
    for (before = F(gPlayerFiles, Handle, 8); before != NULL; before = *(Handle *)*before) {
        if (game_strcmp((u8 *)*before + PROFILE_OFS(20), p + PROFILE_OFS(20)) > 0) {
            break;
        }
    }
    InsertLLE(gPlayerFiles, player, before);
    AddPlayerScores(player);
    PurgePlayerData(player);
    return player;
}


typedef struct GhostHeader {      /* tGhostHeader's layout */
    u32 headerChecksum;           /* 0x00 */
    u8 revision;                  /* 0x04 */
    s8 flag;                      /* 0x05 */
    s8 track;                     /* 0x06 */
    u8 backward;                  /* 0x07 */
    u8 pad08[0x16 - 0x08];
    u8 name[13];                  /* 0x16 */
    u8 pad23[0x28 - 0x23];
    u32 playerId[2];              /* 0x28 */
    u8 pad30[0x38 - 0x30];
    f32 finish;                   /* 0x38 */
    u32 zippedSize;               /* 0x3C */
    u32 unzippedSize;             /* 0x40 */
    u32 zippedChecksum;           /* 0x44 */
    u32 unzippedChecksum;         /* 0x48 */
    u32 start;                    /* 0x4C: a heap block (the window), 32 bits as in the file */
    u32 offset;                   /* 0x50 */
    u32 size;                     /* 0x54 */
} GhostHeader;

typedef struct Ghost {            /* GC __anon_49a9c; N64 layout 0x2C */
    void *ll;                     /* 0x00 */
    void **fileh;                 /* 0x04 */
    s8 track;                     /* 0x08 */
    u8 backward;                  /* 0x09 */
    u8 name[13];                  /* 0x0A */
    u8 pad17;
    u32 playerId[2];              /* 0x18 */
    f32 finish;                   /* 0x20 */
    u8 **datah;                   /* 0x24 */
    GhostHeader **header;         /* 0x28 */
} Ghost;


/* Checks a ghost's file: its revision (GHOST_REVISION) and header checksum, and with data the recording's checksum.
 * Returns 1 when something is wrong; with repair a bad ghost's file is deleted (and 0 returned). */
s32 CheckAndRepairGhost(Ghost **ghosth, s32 data, s32 repair) {
    s32 result;
    GhostHeader *header;

    result = 0;
    header = *(*ghosth)->header;
    if (header->revision != GHOST_REVISION) {
        if (repair) {
            if ((*ghosth)->fileh) {
                PS_DeleteFile((*ghosth)->fileh);
            }
            return 0;
        }
        result = 1;
    }
    if (header->headerChecksum != CalcChecksumInternal(&header->revision, (u8 *)&header->zippedChecksum - (u8 *)header - 4)) {
        if (repair) {
            if ((*ghosth)->fileh) {
                PS_DeleteFile((*ghosth)->fileh);
            }
            return 0;
        }
        result = 1;
    }
    if (!data) {
        return result;
    }
    if (header->zippedChecksum != CalcChecksumInternal((u8 *)(header + 1), header->zippedSize)) {
        if (repair) {
            if ((*ghosth)->fileh) {
                PS_DeleteFile((*ghosth)->fileh);
            }
            return 0;
        }
        result = 1;
    }
    return result;
}

#define N64A0(x) PC_N64A(x)

/* Reads the ghost's file (again: the data already read is purged first) and checks it, repairing it if need be; with
 * decode decompresses the recording; with select moves the header and the recording low in the heap (for the race)
 * and points the header at the recording. Returns 0 when it can't be read. */
s32 ReadGhostDataInternal(Ghost **ghosth, s32 decode, s32 select) {
    Ghost *ghost;

    ghost = *ghosth;
    if (ghost->header != NULL) {
        if (ghost->fileh != NULL) {
            PurgeGhostData(ghosth);
        } else {
            if (decode) {
                DecodeGhost(ghosth);
            }
            return 1;
        }
    }
    PS_ReadPartialFile(ghost->fileh, 0, F(*ghost->fileh, s32, PSF_OFS(64)));
    if (F(*ghost->fileh, GhostHeader **, PSF_OFS(72)) == NULL) {
        return 0;
    }
    ghost->header = F(*ghost->fileh, GhostHeader **, PSF_OFS(72));
    (*ghost->header)->start = NULL;
    if (CheckAndRepairGhost(ghosth, 1, 0) && !CheckAndRepairGhost(ghosth, 1, 1)) {
        return 0;
    }
    if (decode) {
        DecodeGhost(ghosth);
    }
    if (select) {
        ReallocateMemory(N64A0(ghost->header), 88);
        heap_move_block_low(ghost->header);
        heap_move_block_low(ghost->datah);
        (*ghost->header)->start = N64W(*ghost->datah);
    }
    return 1;
}

/* Whether pak's pak changed at the last PS_PakServer (MAT3 paksysdc.c). */
s8 PS_PakChanged(s32 pak) {
    return gPaks[pak].changed;
}

/* Whether a pak the pak system has read is in pak's controller. */
s8 PS_PakInserted(s32 pak) {
    return gPaks[pak].inserted;
}

/* Whether pak's pak is a Rumble Pak. */
s8 PS_PakRumble(s32 pak) {
    return gPaks[pak].rumble;
}
VIEW(u8[], gGhostFiles);
#define gGhostFiles gGhostFiles_view

/* SaveServer calls it before touching a0/a1, so on the N64 it receives SaveServer's own forceUpdate and
 * rumbleOnly; the decomp's call passes nothing (docs/port/PROTOTYPES.md). */
#define PS_PakServer() PS_PakServer(forceUpdate, rumbleOnly)

/* Drops a ghost (MAT3 _FreeGhost). */
void FreeGhost(Handle ghosth) {
    *ghosth = NULL;
}

/* Opens the ghost in the pak file fileh (nothing if it is open already; MAT3 _ReadGhostHeader with _AlreadyReadGhost
 * inline): reads its header, checks and repairs it, takes its track, direction, driver and time, drops the data
 * from memory, and puts it in the ghost list in order of its time. Returns its handle, or NULL. */
Handle _ReadGhostHeader(Handle fileh) {
    Handle ghosth;
    Ghost *ghost;
    Handle before;
    GhostHeader *header;

    for (ghosth = F(gGhostFiles, Handle, 8); ghosth != NULL; ghosth = *(Handle *)*ghosth) {
        if (((Ghost *)*ghosth)->fileh == fileh) {
            return NULL;
        }
    }
    PS_ReadPartialFile(fileh, 0, 88);
    if (F(*fileh, void *, PSF_OFS(72)) == NULL) {
        return NULL;
    }
    ghosth = NewGhost();
    ghost = *ghosth;
    memset(ghost, 0, GHOST_OFS(44));
    ghost->fileh = fileh;
    F(*fileh, void *, PSF_OFS(8)) = RemovePlayerFile;
    F(*ghost->fileh, void *, PSF_OFS(12)) = MovePlayerFile;
    ghost->datah = NULL;
    ghost->header = F(*fileh, GhostHeader **, PSF_OFS(72));
    header = *ghost->header;
    header->start = NULL;
    if (CheckAndRepairGhost((Ghost **)ghosth, 0, 0) && !CheckAndRepairGhost((Ghost **)ghosth, 0, 1)) {
        FreeGhost(ghosth);
        return NULL;
    }
    ghost->track = header->track;
    ghost->backward = header->backward;
    memcpy(ghost->name, header->name, 13);
    ghost->playerId[0] = header->playerId[0];
    ghost->playerId[1] = header->playerId[1];
    ghost->finish = header->finish;
    PS_PurgeFile(fileh);
    ghost->header = NULL;
    for (before = F(gGhostFiles, Handle, 8); before != NULL; before = *(Handle *)*before) {
        if (ghost->finish < ((Ghost *)*before)->finish) {
            break;
        }
    }
    InsertLLE(gGhostFiles, ghosth, before);
    return ghosth;
}

/* Serves the paks (PS_PakServer), then, after the first three calls, opens the profiles and ghosts (by their file
 * names) of every pak that changed and was read and isn't a Rumble Pak; a pak whose reading the error screen
 * interrupted is read again on the next call. */
void SaveServer(s32 forceUpdate, s32 rumbleOnly) {
    s32 pak;
    Handle fileh;
    Handle next;
    static s8 sInitCountdown = 3;
    static s8 sLastPak = -1;
    static s8 sReadingPak[4] = { 0, 0, 0, 0 };

    if (forceUpdate | rumbleOnly) {
    }
    PS_PakServer();
    if (sInitCountdown > 1) {
        sInitCountdown--;
        return;
    }
    for (pak = 0; pak != 4; pak++) {
        if (sInitCountdown == 0 && sReadingPak[pak] == 0) {
            if (PS_PakChanged(pak) == 0 || PS_PakInserted(pak) == 0 || PS_PakRumble(pak) != 0) {
                continue;
            }
        }
        sReadingPak[pak] = 1;
        if (sLastPak == pak) {
            continue;
        }
        fileh = F(gPS_Files[pak], Handle, 8);
        sLastPak = pak;
        while (fileh != NULL) {
            if (PS_PakInserted(pak) == 0) {
                break;
            }
            next = *(Handle *)*fileh;
            if (game_strncmp(save_file_name_prefix, (char *)*fileh + PSF_OFS(18), 8) == 0) {
                if (game_strncmp(player_file_ext_prefix, (char *)*fileh + PSF_OFS(53), 1) == 0) {
                    ReadPlayer(fileh);
                } else {
                    _ReadGhostHeader(fileh);
                }
            }
            fileh = next;
        }
        sReadingPak[pak] = 0;
        sLastPak = -1;
    }
    sInitCountdown = 0;
}
