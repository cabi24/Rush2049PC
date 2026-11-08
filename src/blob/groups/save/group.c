/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * SaveInit: the N64 save unit (GC GameSource/src/save.c counterparts).
 * Supersedes the locked group of the same name (members DeletePlayer, ReadPlayerData, InitPlayer,
 * SaveInit, InitLL, ClearLL: unchanged; this file was its group.c, history).
 * New members (all EQUAL in the whole-program unit, 0 locked bodies differing):
 * CheckAndRepairPlayer = GC _CheckAndRepairPlayer(player, repair); IPA player in s8 (separate `j` counter for the
 *                       option-repair loops, stats loops indexed by `i`).
 * ReadPlayer = GC _ReadPlayer(fileh); IPA fileh in s0.
 *   NewPlayer     = caller-less stub: GC _NewPlayer, N64 form (pak, slot), inlined into ReadPlayer and CreatePlayer.
 * CheckAndRepairGhost = GC _CheckAndRepairGhost(ghosth, data, repair); IPA t0, t2, t1.
 *   ReadGhostDataInternal     = GC _ReadGhostData(ghosth, decode) + N64 flag `select`; IPA decode s0, ghosth s1, select s3.
 *   FreeGhost     = caller-less stub: GC _FreeGhost(ghosth) (`*ghosth = NULL`), inlined into _ReadGhostHeader.
 *   Pow     = caller-less stub: GC _Pow(exp) (num 10 folded), inlined into CreatePlayer (context).
 *   PS_PakChanged, PS_PakInserted, PS_PakRumble = locked pak-TU getters of gPaks[pak] bytes 0/1/6, re-typed
 *                       here on the gPaks record (same bodies) so umerge's inlined copies in SaveServer share
 *                       one &gPaks[pak] address web; their single files are superseded (--revert-single).
 * adds two members: SaveServer (GC SaveServer) and func_800CC040 (GC SaveGhost, in ctx_slot.c).
 *   SaveServer's group-compile miss (176/225: sInitCountdown not reloaded in the pak loop) came from game_strncmp
 *   (strncmp) being an UNDEFINED external in the group program: uopt assumes an undefined callee cannot modify the
 *   TU's function-local statics, a defined one can. In the whole-program unit game_strncmp is defined (locked
 *   single, kept), so retail reloads. ctx_strncmp.c = byte-identical copy of src/blob/game_strncmp.c (context, keep).
 * Context (not members): CreatePlayer (GC CreatePlayer, draft, 241/272 words), SetDefaultCarSettings (locked elsewhere),
 *   game_strncmp (strncmp, ctx_strncmp.c).
 * Context files: ctx_hash.c, ctx_slot.c, ctx_newghost.c (locked group sources; see group.json note).
 * Non-member helper: _ReadGhostHeader (GC static _ReadGhostHeader with GC _AlreadyReadGhost's loop inside it):
 *   non-static, not keep; umerge inlines it into SaveServer; retail has no stub for it.
 * Decisions / devices to review:
 *   - SaveServer is defined with GC's two parameters (s32 forceUpdate, s32 rumbleOnly), matching the locked callers'
 * declared prototype (PS_ErrorFunc/input_aux call (0, 0), updateHighScoreScreen (1, 0) and (0, 0)). Retail never
 *     reads them; DEVICE (disclosed): a compiled-out `if (forceUpdate | rumbleOnly) {}` at the top -- without it ugen
 *     stores a0/a1 to their homes (210/225). The (void) form is also MATCH without any device.
 *   - SaveServer's sInitCountdown/sLastPak/sReadingPak are function-local statics: the TU's own .data at 0x8011742C
 *     (= 3), 0x80117430 (= -1), 0x80117434 ({0,0,0,0}); only SaveServer references them in the image.
 *   - _ReadGhostHeader declares GC's `pak` local nowhere; no unused locals in any member.
 * GC-derived names: _CalcChecksum = CalcChecksumInternal, PS_ReadFile = PS_ReadPartialFile, PS_WriteFile, PS_FlushFile,
 *   PS_FlushPak, PS_CreateFile, PS_PakServer, InsertLLE, strncmp = game_strncmp,
 *   strcmp = game_strcmp, _NewGhost = NewGhost, SavePlayerOption = audio_bus_mix, gOptions,
 *   gTrackNum, _DecodeGhost = menu_back, gGhostFiles, gPlayerFiles,
 *   gPS_Files = gPS_Files (+8 = head).
 */
typedef signed char s8;typedef unsigned char u8;typedef short s16;typedef unsigned short u16;
typedef signed int s32;typedef unsigned int u32;typedef float f32;
#define NULL ((void *)0)
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
u32 CalcChecksumInternal(u8 *, u32);
void SetDefaultControls(void *);
void SavePlayerOption(Handle, u8, s8);
s8 GetDefaultOption(s32);
void SavePlayerCar(Handle, s32);
void SavePlayerCarView(Handle, s32);
s32 game_strcmp(void *, void *);
Handle PS_CreateFile(s32, char *, char *, u32, s32, s32);
void PS_WriteFile(Handle, void *, u32);
s32 PS_FlushFile(Handle, s32);
s32 PS_FlushPak(s32, s32);
void InsertLLE(void *, Handle, Handle);
void game_strcpy(void *, char *);
void PS_DeleteFile(Handle);
void RemovePlayerFile(void);
void MovePlayerFile(void);

/* ---- SetDefaultCarSettings: copied from src/blob/groups/save_default_car/dn.c (matched there) ---- */
extern s8 gCarHandling[];
extern s8 gCarEngines[];
extern s8 gCarTires[];
extern s8 gCarFrame[];
extern s8 gCarWings[];
extern s8 car_color_choice_a[];
extern s8 car_color_choice_b[];
extern s8 car_color_choice_c[];
extern s8 gTireRims[];
extern s8 gCarHorn[];
extern float gFrontTireScale[];
extern float gRearTireScale[];

extern void SavePlayerCarSetting(void *p, u8 idx, u8 slot, s8 val);

void SetDefaultCarSettings(void *p, s32 idx);

extern s8 gTrackNum;

void InitPlayer(Handle h) {
    u8 *data;
    s32 i;
    s8 saved;

    memset(*h, 0, SLOT_DATA);
    data = *F(*h, Handle, SLOT_DATA);
    memset(data, 0, 2064);
    data[12] = 16;
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

extern s8 gOptions[];
void PS_ReadPartialFile(Handle, s32, s32);
void SavePlayerControls(Handle, void *);
extern char cheat_profile_name[];
extern s8 cheat_menu_unlocked;
extern s16 gCheatState[];
extern s8 gSaveGhostFiles;

s32 CheckAndRepairPlayer(Handle h, s32 repair) {
    s32 dirty;
    u8 *save;
    u8 *p;
    s32 i;
    s32 j;
    s8 saved;

    dirty = 0;
    save = *F(*h, Handle, SLOT_DATA);
    if (save[12] != 16) {
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
        cheat_menu_unlocked = 1;
        for (i = 1; i < 21; i++) {
        }
        gCheatState[14] = 1;
        gCheatState[15] = 1;
        gCheatState[16] = 1;
        gSaveGhostFiles = 1;
    }
    return 1;
}

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
extern Handle player_profile_handles[][16];
/* the profile records (save.c's SaveSlot, 64 bytes; one-element arrays, so player_profile_records[pak][slot] is a pointer as
 * the N64's byte array is) and the four tDLLists (24 bytes) */
typedef struct { void *next, *prev; Handle fileh; u8 pad0C[0x2C - 0x0C]; Handle data; } ProfileRec;
extern ProfileRec player_profile_records[][16][1];
extern u8 gPlayerFiles[];
extern u8 gPS_Files[][24];
extern s32 gScheduler[];
extern char player_file_prefix[], player_file_ext_match[], player_file_ext[], player_file_digits_format[];

/* deleted static at 0x800CD0FC (caller-less stub): GC save.c _Pow(int num, int exp) */
s32 Pow(s32 exp) {
    s32 result = 1;
    while (exp--) {
        result *= 10;
    }
    return result;
}

Handle NewPlayer(s32 pak, s32 slot) {   /* deleted static at 0x800C7EC0: GC save.c _NewPlayer (N64: pak, slot) */
    Handle player;

    player = &player_profile_handles[pak][slot];
    *player = player_profile_records[pak][slot];
    return player;
}

/* GC save.c CreatePlayer(int pak, unsigned char *name) */
/* A profile keeps its handle's N64 address: the handle's own (pc_n64_identity, pc_prelude.h; LOCKSTEP.md 5). */
#define N64A(x) pc_n64_identity(x)
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
    fileh = PS_CreateFile(pak, file, player_file_ext, 2064, 0x3544, 0x4E525545);
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
    PS_WriteFile(F(p, Handle, PROFILE_OFS(8)), data, 2064);
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


/* ===== ReadPlayer = GC save.c _ReadPlayer(fileh); IPA: fileh in s0 ===== */
s8 GetPlayerOption(Handle player, u8 option);      /* GC GetPlayerOption */
void video_set_offset(s8 x, s8 y);               /* GC VideoSetOffset */
void LoadTextWave(void);                       /* GC LoadTextWave */
void AddPlayerScores(Handle player);                  /* GC AddPlayerScores */
void PurgePlayerData(Handle player);      /* GC PurgePlayerData */
extern s8 gGlobalsSet;                           /* GC gGlobalsSet */
extern s16 gLang;                          /* GC gLang */

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

/* ===== CheckAndRepairGhost = GC _CheckAndRepairGhost ===== */
typedef struct GhostHeader {      /* GC __anon_49776; N64 layout 0x58 (from the retail words) */
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


s32 CheckAndRepairGhost(Ghost **ghosth, s32 data, s32 repair) {
    s32 result;
    GhostHeader *header;

    result = 0;
    header = *(*ghosth)->header;
    if (header->revision != 17) {
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

/* ===== ReadGhostDataInternal = GC save.c _ReadGhostData(ghosth, decode) + an N64-only third flag;
 * IPA: decode s0, ghosth s1, select s3 ===== */
void PurgeGhostData(Ghost **ghosth);            /* frees the ghost data (GC _FreeGhostData-like) */
void DecodeGhost(Ghost **ghosth);                  /* GC _DecodeGhost */
/* The heap's resize takes the handle's 32-bit N64 address (its definition's u32): N64A0(p) is a window pointer's
 * (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
void ReallocateMemory(u32 headerh, s32 offset);
#define N64A0(x) PC_N64A(x)
void heap_move_block_low(void *h);

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

/* ===== func_800C813C = GC save.c SaveServer(forceUpdate, rumbleOnly) ===== */
extern char save_file_name_prefix[];      /* "RUSH2049" */
extern char player_file_ext_prefix[];      /* "P" */
/* PC: the controller/pak record's host layout (paksys' PakRecord, docs/port/POINTERS.md 9): its OSPfs at +0x0C
 * holds a pointer, so from there on the record is 4..12 bytes later and 0x310 bytes long. */
typedef struct { s32 status; void *queue; s32 channel; u8 id[32], label[32]; s32 version, dir_size, inode_table,
    minode_table, dir_table, inode_start_page; u8 banks, activebank; } PcPfs;   /* OSPfs */
typedef struct PakChannel {
    s8 present;                /* 0x00 */
    s8 state;                  /* 0x01 */
    u8 unk02[4];
    s8 busy;                   /* 0x06 */
    u8 unk07[5];
    PcPfs pfs;
    u8 pad74[0x304 - 0x74];
} PakChannel;
extern PakChannel gPaks[];
/* pak-TU channel getters (locked singles, re-typed on the gPaks record so the inlined copies share one
 * address CSE in SaveServer; each still compiles to its locked body) */
s8 PS_PakChanged(s32 pak) {
    return gPaks[pak].present;
}

s8 PS_PakInserted(s32 pak) {
    return gPaks[pak].state;
}

s8 PS_PakRumble(s32 pak) {
    return gPaks[pak].busy;
}
extern u8 gGhostFiles[];        /* GC gGhostFiles (list head at +8) */
void PS_PakServer(s32 forceUpdate, s32 rumbleOnly);
/* SaveServer calls it before touching a0/a1, so on the N64 it receives SaveServer's own forceUpdate and
 * rumbleOnly; the decomp's call passes nothing (docs/port/PROTOTYPES.md). */
#define PS_PakServer() PS_PakServer(forceUpdate, rumbleOnly)
                 /* GC PS_PakServer */
s32 game_strncmp(char *, char *, s32);          /* strncmp */
Handle NewGhost(void);                      /* GC _NewGhost */
void PS_PurgeFile(Handle fileh);

/* deleted static at 0x800C7300 (caller-less stub): GC save.c _ReadGhostHeader(fileh), with GC _AlreadyReadGhost
 * written inline (one `ghosth` variable for the search and the new ghost) */
void FreeGhost(Handle ghosth) {   /* GC _FreeGhost */
    *ghosth = NULL;
}

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

void SaveServer(s32 forceUpdate, s32 rumbleOnly) {
    s32 pak;
    Handle fileh;
    Handle next;
    static s8 sInitCountdown = 3;              /* 0x8011742C */
    static s8 sLastPak = -1;                   /* 0x80117430 */
    static s8 sReadingPak[4] = { 0, 0, 0, 0 }; /* 0x80117434 */

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
