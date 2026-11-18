/* Profiles and ghosts on the paks: creating, reading, checking and repairing them, and the save server that reads a
 * pak's profiles and ghosts when it comes in (MAT3 save.c). A profile (tProfile) holds its file's data (tSaveData)
 * through the pak file's data handle; a ghost (tGhostFile) its file's header (tGhostHeader, the compressed recording
 * after it) the same way. */
#include "game/game.h"

void *memset(void *, s32, u32);
void *memcpy(void *, const void *, u32);
s32 sprintf(char *, const char *, ...);

/* Gives the profile h a fresh file: its record cleared (but for its data handle), the data zeroed, revision
 * PROFILE_REVISION, the default controls with rumble, every car's default settings, every option's default (the race
 * type settings for every track), the default car and view for every race type. The controls' checksum and the
 * copies of the controls take the rumble byte after them (11 bytes). */
void InitPlayer(tProfile **h) {
    tSaveData *data;
    s32 i;
    s8 saved;

    memset(*h, 0, __builtin_offsetof(tProfile, datah));
    data = *(*h)->datah;
    memset(data, 0, sizeof(tSaveData));
    data->revision = PROFILE_REVISION;
    SetDefaultControls(data->controls);
    data->rumble = 1;
    data->contChecksum = CalcChecksumInternal(data->controls, 11);
    memcpy((*h)->controls, data->controls, 11);
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
    for (data->raceType = 0; data->raceType < 7; data->raceType++) {
        SavePlayerCar(h, 2);
        SavePlayerCarView(h, 2);
    }
    data->raceType = 0;
}

/* Checks the profile's file: its revision and every part's checksum. Returns 1 when something is wrong. With repair
 * each bad part is reset (the options and settings from the game's, the controls to the defaults, the records
 * zeroed) and written; a file of another revision is deleted instead (and 0 returned). Original game: only the first
 * 6 of the 12 race stats are checked (MAT3 alike); the other 6 (the tracks backwards) are never repaired. */
s32 CheckAndRepairPlayer(tProfile **h, s32 repair) {
    s32 dirty;
    tSaveData *save;
    tCarSettings *p;
    s32 i;
    s32 j;
    s8 saved;

    dirty = 0;
    save = *(*h)->datah;
    if (save->revision != PROFILE_REVISION) {
        if (repair) {
            PS_DeleteFile((*h)->fileh);
            return 0;
        }
        dirty = 1;
    }
    if (CalcChecksumInternal(save->id, (u8 *)&save->optChecksum - (u8 *)save->id) != save->nameChecksum) {
        dirty = 1;
        if (repair) {
            save->nameChecksum = CalcChecksumInternal(save->id, (u8 *)&save->optChecksum - (u8 *)save->id);
            PS_WriteFile((*h)->fileh, save, 4);
        }
    }
    if (CalcChecksumInternal(save->options, 21) != save->optChecksum) {
        dirty = 1;
        if (repair) {
            for (i = 0; i < 21; i++) {
                SavePlayerOption(h, i, gOptions[i]);
            }
        }
    }
    if (CalcChecksumInternal(save->controls, 11) != save->contChecksum) {
        dirty = 1;
        if (repair) {
            SetDefaultControls((*h)->controls);
            (*h)->rumble = 1;
            memcpy(save->controls, (*h)->controls, 11);
            save->contChecksum = CalcChecksumInternal(save->controls, 11);
            PS_WriteFile((*h)->fileh, &save->contChecksum, 15);
        }
    }
    if (CalcChecksumInternal(&save->raceSettings.track, sizeof(tRaceSettings) - 4) != save->raceSettings.checksum) {
        dirty = 1;
        if (repair) {
            saved = gTrackNum;
            gTrackNum = 0;
            for (j = 21; j < 41; j++) {
                SavePlayerOption(h, j, gOptions[j]);
            }
            gTrackNum = saved;
            memset(&save->raceSettings, 0, sizeof(tRaceSettings));
            PS_WriteFile((*h)->fileh, &save->raceSettings, sizeof(tRaceSettings));
        }
    }
    if (CalcChecksumInternal(&save->battleSettings.track, sizeof(tBattleSettings) - 4) !=
        save->battleSettings.checksum) {
        dirty = 1;
        if (repair) {
            saved = gTrackNum;
            gTrackNum = 6;
            for (j = 21; j < 41; j++) {
                SavePlayerOption(h, j, gOptions[j]);
            }
            gTrackNum = saved;
            memset(&save->battleSettings, 0, sizeof(tBattleSettings));
            PS_WriteFile((*h)->fileh, &save->battleSettings, sizeof(tBattleSettings));
        }
    }
    if (CalcChecksumInternal(&save->stuntSettings.track, sizeof(tStuntSettings) - 4) != save->stuntSettings.checksum) {
        dirty = 1;
        if (repair) {
            saved = gTrackNum;
            gTrackNum = 14;
            for (j = 21; j < 41; j++) {
                SavePlayerOption(h, j, gOptions[j]);
            }
            gTrackNum = saved;
            memset(&save->stuntSettings, 0, sizeof(tStuntSettings));
            PS_WriteFile((*h)->fileh, &save->stuntSettings, sizeof(tStuntSettings));
        }
    }
    if (CalcChecksumInternal(&save->obstacleSettings.track, sizeof(tObstacleSettings) - 4) !=
        save->obstacleSettings.checksum) {
        dirty = 1;
        if (repair) {
            saved = gTrackNum;
            gTrackNum = 18;
            for (j = 21; j < 41; j++) {
                SavePlayerOption(h, j, gOptions[j]);
            }
            gTrackNum = saved;
            memset(&save->obstacleSettings, 0, sizeof(tObstacleSettings));
            PS_WriteFile((*h)->fileh, &save->obstacleSettings, sizeof(tObstacleSettings));
        }
    }
    for (i = 0; i < 6; i++) {
        if (CalcChecksumInternal(save->raceStats[i].scores, sizeof(tRaceStats) - 4) != save->raceStats[i].checksum) {
            dirty = 1;
            if (repair) {
                memset(&save->raceStats[i], 0, sizeof(tRaceStats));
                PS_WriteFile((*h)->fileh, &save->raceStats[i], sizeof(tRaceStats));
            }
        }
    }
    for (i = 0; i < 4; i++) {
        if (CalcChecksumInternal(&save->stuntStats[i].bestStunt, sizeof(tStuntStats) - 4) !=
            save->stuntStats[i].checksum) {
            dirty = 1;
            if (repair) {
                memset(&save->stuntStats[i], 0, sizeof(tStuntStats));
                PS_WriteFile((*h)->fileh, &save->stuntStats[i], sizeof(tStuntStats));
            }
        }
    }
    for (i = 0; i < 8; i++) {
        if (CalcChecksumInternal(&save->battleStats[i].totalMatches, sizeof(tBattleStats) - 4) !=
            save->battleStats[i].checksum) {
            dirty = 1;
            if (repair) {
                memset(&save->battleStats[i], 0, sizeof(tBattleStats));
                PS_WriteFile((*h)->fileh, &save->battleStats[i], sizeof(tBattleStats));
            }
        }
    }
    for (i = 0; i < 1; i++) {
        if (CalcChecksumInternal(&save->obstacleStats[i].bestTime, sizeof(tObstacleStats) - 4) !=
            save->obstacleStats[i].checksum) {
            dirty = 1;
            if (repair) {
                memset(&save->obstacleStats[i], 0, sizeof(tObstacleStats));
                PS_WriteFile((*h)->fileh, &save->obstacleStats[i], sizeof(tObstacleStats));
            }
        }
    }
    for (i = 0; i < 4; i++) {
        if (CalcChecksumInternal(&save->circuitStats[i].numCircuits, sizeof(tCircuitStats) - 4) !=
            save->circuitStats[i].checksum) {
            dirty = 1;
            if (repair) {
                memset(&save->circuitStats[i], 0, sizeof(tCircuitStats));
                PS_WriteFile((*h)->fileh, &save->circuitStats[i], sizeof(tCircuitStats));
            }
        }
    }
    if (CalcChecksumInternal(&save->circuit.active, sizeof(tCircuit) - 4) != save->circuit.checksum) {
        dirty = 1;
        if (repair) {
            memset(&save->circuit, 0, sizeof(tCircuit));
            PS_WriteFile((*h)->fileh, &save->circuit, sizeof(tCircuit));
        }
    }
    for (i = 0; i < 13; i++) {
        p = &save->carSettings[i];
        if (CalcChecksumInternal(p->settings, 12) != p->checksum) {
            dirty = 1;
            if (repair) {
                SetDefaultCarSettings(h, i);
                PS_WriteFile((*h)->fileh, p, sizeof(tCarSettings));
            }
        }
    }
    return dirty;
}

/* Reads the profile's file into memory, unless it is (its data handle), and checks and repairs it; takes its
 * controls; the cheat profile turns every cheat on. Returns 0 when it can't be read or repaired. */
s32 ReadPlayerData(tProfile **h) {
    tProfile *obj;
    tSaveData **data;
    s32 i;

    obj = *h;
    if (obj->datah != NULL) {
        return 1;
    }
    PS_ReadPartialFile(obj->fileh, 0, obj->fileh->file->size);
    data = (tSaveData **)obj->fileh->file->data;
    if (data == NULL) {
        return 0;
    }
    obj->datah = data;
    if (CheckAndRepairPlayer(h, 0) && !CheckAndRepairPlayer(h, 1)) {
        return 0;
    }
    SavePlayerControls(h, obj->controls);
    if (game_strcmp(obj->name, cheat_profile_name) == 0) {
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
void DeletePlayer(tProfile **h) {
    if ((*h)->fileh != NULL) {
        if (ReadPlayerData(h)) {
            PS_DeleteFile((*h)->fileh);
        }
    } else {
        InitPlayer(h);
    }
}

typedef struct { char c[9]; } Name9;

/* 10 to the power exp (MAT3 _Pow). */
s32 Pow(s32 exp) {
    s32 result = 1;
    while (exp--) {
        result *= 10;
    }
    return result;
}

/* The profile record of pak's file number slot (player_profile_records), through its handle (MAT3 _NewPlayer). */
tProfile **NewPlayer(s32 pak, s32 slot) {
    tProfile **player;

    player = &player_profile_handles[pak][slot];
    *player = &player_profile_records[pak][slot];
    return player;
}

/* Creates a profile named name on pak's pak: a file named after the name (its first six letters, others made '*',
 * numbered when the name is taken), a fresh file (InitPlayer) with the profile's id (the creation time, the
 * record and the pak) and name, written at once; the profile joins the profile list in name order. Returns its
 * handle, or NULL when the file can't be created. */
tProfile **CreatePlayer(s32 pak, u8 *name) {
    tProfile **player;
    tProfile **before;
    tPakHandle *other;
    tProfile *p;
    tSaveData *data;
    tPakHandle *fileh;
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
        for (other = gPS_Files[pak].head; other != NULL; other = other->file->next) {
            if (game_strcmp(other->file->name, file) == 0 && game_strcmp(other->file->ext, player_file_ext_match) == 0) {
                break;
            }
        }
        if (other == NULL) {
            break;
        }
    }
    fileh = PS_CreateFile(pak, file, player_file_ext, PROFILE_FILE_SIZE, GAME_COMPANY_CODE, GAME_CODE);
    if (fileh == NULL) {
        return NULL;
    }
    player = NewPlayer(fileh->file->pak, fileh->file->fileNo);
    p = *player;
    data = *(p->datah = (tSaveData **)fileh->file->data);
    InitPlayer(player);
    p->fileh = fileh;
    fileh->file->RemoveFile = RemovePlayerFile;
    p->fileh->file->MoveFile = MovePlayerFile;
    data->id[0] = gScheduler.retraceCount;
    data->id[1] = (pc_n64_identity(player) << 3) | pak;
    game_strcpy(data->name, name);
    data->nameChecksum = CalcChecksumInternal(data->id, (u8 *)&data->optChecksum - (u8 *)data->id);
    memcpy(p->name, data->name, 13);
    memcpy(p->id, data->id, 8);
    PS_WriteFile(p->fileh, data, PROFILE_FILE_SIZE);
    PS_FlushFile(p->fileh, 0);
    PS_FlushPak(p->fileh->file->pak, 1);
    for (before = gPlayerFiles.head; before != NULL; before = (*before)->next) {
        if (game_strcmp((*before)->name, p->name) > 0) {
            break;
        }
    }
    InsertLLE(&gPlayerFiles, player, before);
    return player;
}


/* Opens the profile in the pak file fileh (nothing if it is open already): reads and checks it, takes its name, id
 * and controls; the first profile read when no options are set gives the game its options (and language and screen
 * offset). It joins the profile list in name order and the high score tables, and its data is dropped from memory.
 * Returns its handle, or NULL. */
tProfile **ReadPlayer(tPakHandle *fileh) {
    tProfile **player;
    tProfile *p;
    tProfile **before;
    s32 i;

    for (player = gPlayerFiles.head; player != NULL; player = (*player)->next) {
        if ((*player)->fileh == fileh) {
            return NULL;
        }
    }
    PS_ReadPartialFile(fileh, 0, fileh->file->size);
    if (fileh->file->data == NULL) {
        return NULL;
    }
    player = NewPlayer(fileh->file->pak, fileh->file->fileNo);
    p = *player;
    p->fileh = fileh;
    fileh->file->RemoveFile = RemovePlayerFile;
    p->fileh->file->MoveFile = MovePlayerFile;
    p->datah = (tSaveData **)p->fileh->file->data;
    if (CheckAndRepairPlayer(player, 0) && !CheckAndRepairPlayer(player, 1)) {
        return NULL;
    }
    memcpy(p->name, (*p->datah)->name, 13);
    memcpy(p->id, (*p->datah)->id, 8);
    memcpy(p->controls, (*p->datah)->controls, 11);
    if (!gGlobalsSet && gPlayerFiles.head == NULL) {
        for (i = 0; i < 21; i++) {
            gOptions[i] = GetPlayerOption(player, i);
        }
        gOptions[17] = 1;
        gLang = gOptions[16] = GetPlayerOption(player, 16);
        LoadTextWave();
        video_set_offset(gOptions[19], gOptions[20]);
    }
    for (before = gPlayerFiles.head; before != NULL; before = (*before)->next) {
        if (game_strcmp((*before)->name, p->name) > 0) {
            break;
        }
    }
    InsertLLE(&gPlayerFiles, player, before);
    AddPlayerScores(player);
    PurgePlayerData(player);
    return player;
}

/* Checks a ghost's file: its revision (GHOST_REVISION) and header checksum, and with data the recording's checksum.
 * Returns 1 when something is wrong; with repair a bad ghost's file is deleted (and 0 returned). */
s32 CheckAndRepairGhost(tGhostFile **ghosth, s32 data, s32 repair) {
    s32 result;
    tGhostHeader *header;

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

/* Reads the ghost's file (again: the data already read is purged first) and checks it, repairing it if need be; with
 * decode decompresses the recording; with select moves the header and the recording low in the heap (for the race)
 * and marks the header as having its samples (the recording, MAT3's header->start = *datah). Returns 0 when it
 * can't be read. */
s32 ReadGhostDataInternal(tGhostFile **ghosth, s32 decode, s32 select) {
    tGhostFile *ghost;

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
    PS_ReadPartialFile(ghost->fileh, 0, ghost->fileh->file->size);
    if (ghost->fileh->file->data == NULL) {
        return 0;
    }
    ghost->header = (tGhostHeader **)ghost->fileh->file->data;
    (*ghost->header)->hasSamples = 0;
    if (CheckAndRepairGhost(ghosth, 1, 0) && !CheckAndRepairGhost(ghosth, 1, 1)) {
        return 0;
    }
    if (decode) {
        DecodeGhost(ghosth);
    }
    if (select) {
        ReallocateMemory(ghost->header, sizeof(tGhostHeader));
        heap_move_block_low(ghost->header);
        heap_move_block_low(ghost->datah);
        (*ghost->header)->hasSamples = 1;   /* the samples: its data, *ghost->datah */
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

/* SaveServer calls it before touching a0/a1, so on the N64 it receives SaveServer's own forceUpdate and
 * rumbleOnly; the decomp's call passes nothing (docs/port/PROTOTYPES.md). */
#define PS_PakServer() PS_PakServer(forceUpdate, rumbleOnly)

/* Drops a ghost (MAT3 _FreeGhost). */
void FreeGhost(tGhostFile **ghosth) {
    *ghosth = NULL;
}

/* Opens the ghost in the pak file fileh (nothing if it is open already; MAT3 _ReadGhostHeader with _AlreadyReadGhost
 * inline): reads its header, checks and repairs it, takes its track, direction, driver and time, drops the data
 * from memory, and puts it in the ghost list in order of its time. Returns its handle, or NULL. */
tGhostFile **_ReadGhostHeader(tPakHandle *fileh) {
    tGhostFile **ghosth;
    tGhostFile *ghost;
    tGhostFile **before;
    tGhostHeader *header;

    for (ghosth = gGhostFiles.head; ghosth != NULL; ghosth = (*ghosth)->next) {
        if ((*ghosth)->fileh == fileh) {
            return NULL;
        }
    }
    PS_ReadPartialFile(fileh, 0, sizeof(tGhostHeader));
    if (fileh->file->data == NULL) {
        return NULL;
    }
    ghosth = NewGhost();
    ghost = *ghosth;
    memset(ghost, 0, sizeof(tGhostFile));
    ghost->fileh = fileh;
    fileh->file->RemoveFile = RemovePlayerFile;
    ghost->fileh->file->MoveFile = MovePlayerFile;
    ghost->datah = NULL;
    ghost->header = (tGhostHeader **)fileh->file->data;
    header = *ghost->header;
    header->hasSamples = 0;
    if (CheckAndRepairGhost(ghosth, 0, 0) && !CheckAndRepairGhost(ghosth, 0, 1)) {
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
    for (before = gGhostFiles.head; before != NULL; before = (*before)->next) {
        if (ghost->finish < (*before)->finish) {
            break;
        }
    }
    InsertLLE(&gGhostFiles, ghosth, before);
    return ghosth;
}

/* Serves the paks (PS_PakServer), then, after the first three calls, opens the profiles and ghosts (by their file
 * names) of every pak that changed and was read and isn't a Rumble Pak; a pak whose reading the error screen
 * interrupted is read again on the next call. */
void SaveServer(s32 forceUpdate, s32 rumbleOnly) {
    s32 pak;
    tPakHandle *fileh;
    tPakHandle *next;
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
        fileh = gPS_Files[pak].head;
        sLastPak = pak;
        while (fileh != NULL) {
            if (PS_PakInserted(pak) == 0) {
                break;
            }
            next = fileh->file->next;
            if (game_strncmp(save_file_name_prefix, fileh->file->name, 8) == 0) {
                if (game_strncmp(player_file_ext_prefix, fileh->file->ext, 1) == 0) {
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
