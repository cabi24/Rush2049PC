/* include/game/save.h: the Controller Pak, profiles, records, options and passwords.
 *
 * The pak system (MAT3 paksysdc.c: PS_*) keeps each controller's pak (gPaks), its directory and the files the game
 * has open (gPS_Files, pak_file_records), reads them whole into the heap and writes back only the 32-byte runs that
 * changed. Above it, the save code (MAT3 save.c, gamerecords.c, ghost.c, password.c) keeps the players' profiles,
 * the records and ghosts, the unlocks and the options, and the pak error screens (MAT3 pakerr.c) ask the player what
 * to do when a pak fails (docs/code/ui.md). */
#ifndef GAME_SAVE_H
#define GAME_SAVE_H

#include "game/types.h"

/* The options (gOptions; a profile keeps them, SavePlayerOption, GetPlayerOption): 0..20 the game's, 21..40 the
 * settings of a race type (a profile keeps them per race type: tSaveData's race, battle, stunt and obstacle settings).
 * What each sets is InitOptions'. */
enum {
    kOptTrackMap = 0, kOptRadar = 1, kOptElapsed = 2, kOptRemaining = 3, kOptTach = 4, kOptSpeed = 5,
    kOptOdometer = 6, kOptPlace = 7, kOptShifter = 8, kOptMetric = 9, kOptPlayerArrows = 10, kOptCoins = 11,
    kOptMusicVolume = 12, kOptEffectsVolume = 13, kOptSong = 14, kOptMono = 15, kOptLanguage = 16,
    kOptWrongWay = 18, kOptScreenX = 19, kOptScreenY = 20,
    kNumGameOptions = 21,
    kOptLaps = 21, kOptDrones = 22, kOptGravity = 23, kOptDifficulty = 24, kOptGhostSelect = 25, kOptCatchup = 26,
    kOptBackwards = 27, kOptMirror = 28, kOptFog = 29, kOptWind = 30, kOptDeathMode = 31,
    kOptStuntMode = 32, kOptStuntFlow = 33, kOptStuntPoints = 34, kOptStuntMinutes = 35, kOptStuntDeaths = 36,
    kOptBattleMode = 37, kOptBattlePoints = 38, kOptBattleMinutes = 39, kOptBattleNeg = 40,
    kNumOptions = 41
};

/* The pak files' company and game codes (osPfsAllocateFile). */
#define GAME_COMPANY_CODE 0x3544
#define GAME_CODE 0x4E525545

/* The pak system's errors (tPak.error, the pak error screens' modes). 0 to 10 are MAT3's (enum kPS_ErrUnknown),
 * which pak_error_from_pfs makes from libultra's PFS_ERR_*; 11 to 15 are the N64's own. */
enum {
    kPS_ErrNone = 0,
    kPS_ErrNoCont = 1,         /* no controller */
    kPS_ErrNoPak = 2,          /* no pak in the controller (PFS_ERR_NOPACK) */
    kPS_ErrNewPak = 3,         /* another pak than before (PFS_ERR_NEW_PACK) */
    kPS_ErrInconsistent = 4,   /* the pak's file system needs repairing (PFS_ERR_INCONSISTENT) */
    kPS_ErrComm = 5,           /* the controller didn't answer (PFS_ERR_CONTRFAIL) */
    kPS_ErrFull = 6,           /* no room or no directory entry left (PFS_DATA_FULL, PFS_DIR_FULL) */
    kPS_ErrFail = 7,           /* PFS_ERR_INVALID */
    kPS_ErrExist = 8,          /* the file exists (PFS_ERR_EXIST) */
    kPS_ErrBadData = 9,        /* PFS_ERR_BAD_DATA */
    kPS_ErrRecognize = 10,     /* not a pak the system can read (PFS_ERR_ID_FATAL, PFS_ERR_DEVICE): a Rumble Pak, or
                                * the screen offering to repair it */
    kPS_ErrReread = 11,        /* the message before the pak is read again (neither repair nor give up) */
    kPS_ErrRepairFailed = 12,  /* osPfsRepairId failed */
    kPS_ErrRepaired = 13,      /* osPfsRepairId succeeded */
    kPS_ErrDone = 14,          /* closes the error screen at once */
    kPS_ErrUnknown = 15        /* any other libultra error (MAT3's kPS_ErrUnknown is 14) */
};

/* Data */
extern u8 rumble_thread_stack_top[EXTENT(4096)];
extern u8 rumble_thread[EXTENT(432)];
extern OSMesgQueue si_event_queue;
extern s8 gSaveGhostFiles;
extern s8 gCheatActive[EXTENT(21)];
extern Rec164A4 gCheatState;
extern s16 cheat_all_cars;
extern s16 cheat_all_tracks;
extern s16 cheat_all_parts;
extern struct pc_multiblit PS_ErrorFunc_dimMultiBlit[EXTENT(1)];
extern char *gPasswordAlphabet;
extern u8 gInversePasswordAlphabet[EXTENT(256)];
extern u16 gNumBytes;
extern u16 gDataBits;
extern u16 gChecksumBits;
extern u16 gNumBits;
extern u8 *gPlayerNameChars;
extern s8 gGlobalsSet;
extern char *video_buttonNames[EXTENT(9)];
extern s32 video_buttonsJustInited;
extern f32 video_buttonSlideScale;
extern const struct pc_multiblit video_frontEndBlit[EXTENT(2)];
extern const struct pc_multiblit video_pauseBlit[EXTENT(2)];
extern s8 video_inited;
extern struct Blit *video_blit;
extern s32 video_resetChoice;
extern char *gPS_PakName[EXTENT(4)];
extern s8 gPakSysInited;
extern s8 gRumble;
extern s8 gErrPak;
extern u16 pak_charset[EXTENT(256)];
extern s8 start_overlay_loaded;
extern s8 extra_overlay_loaded;
extern char save_file_name_prefix[EXTENT(12)];
extern char player_file_ext_prefix[EXTENT(4)];
extern char cheat_profile_name[EXTENT(16)];
extern char player_file_ext_match[EXTENT(4)];
extern char player_file_ext[EXTENT(4)];
extern char ghost_file_name_format[EXTENT(8)];
extern char ghost_backward_suffix[EXTENT(4)];
extern char ghost_forward_suffix[EXTENT(4)];
extern char ghost_file_ext[EXTENT(4)];
extern char player_file_prefix[EXTENT(12)];
extern char player_file_digits_format[EXTENT(8)];
extern u8 password_gData[EXTENT(32)];
extern PlayerFileList gPlayerFiles;
extern struct Record **gLimboGhosts;
extern tProfile gRealJustPlayers[EXTENT(4)];
extern struct SaveData *gJustPlayerData[EXTENT(4)];
extern tSaveData gRealJustPlayerData[EXTENT(4)];
extern void **player_profile_handles[EXTENT(4)][16];
extern tProfile player_profile_records[EXTENT(4)][16][1];
extern struct Record *gRealLimboGhosts;
extern u8 gNumLimbo;
extern s8 gRankedPlayer[EXTENT(4)];
extern void (*gErrorFunc)(s32, s32, u32, s32, s8 *, s8 *, s32 (*)(s32));
extern u8 gLap[EXTENT(4)];
extern tPak gPaks[EXTENT(4)];
extern tBlastMat pak_file_handles[EXTENT(64)];
extern AudioList gPS_Files[EXTENT(4)];
extern f32 gBestLapTime[EXTENT(4)];
extern PakFileRecord pak_file_records[EXTENT(64)];
extern Settings gOptions;
extern tBlastMat gJustPlayers[EXTENT(4)];
extern s32 gBattlePoints;
extern f32 gLapTimes[EXTENT(4)][8];
extern s32 ties;
extern s8 tied[EXTENT(8)];
extern Ent19 gUnlockedTracks[EXTENT(1)];
extern u8 gUnlockedCars[EXTENT(16)];
extern s8 gUnlockedCircuits[EXTENT(4)][4];
extern s8 gUnlockedTrans[EXTENT(1)][8];
extern s8 gUnlockedEngines[EXTENT(4)][9];
extern s8 gUnlockedTires[EXTENT(4)][5];
extern u8 gUnlockedFrames[EXTENT(56)];
extern u8 gForceUnlockedCircuits[EXTENT(4)];
extern tRaceStats gTotalRaceStats[EXTENT(12)];
extern tStuntStats gTotalStuntStats[EXTENT(4)];
extern tBattleStats gTotalBattleStats[EXTENT(8)];
extern tObstacleStats gTotalObstacleStats[EXTENT(1)];
extern tCircuitStats gTotalCircuitStats[EXTENT(4)];
extern RecordOwner gTotalNames[EXTENT(12)];
extern char gTempNames[EXTENT(20)][13];
extern u16 gTempNameAges[EXTENT(20)];
extern s8 gNewTotalScore[EXTENT(16)];
extern AudioList gGhostFiles;
extern s8 gTeamPoints[EXTENT(4)];
extern f32 video_buttonSlideSpeed;
extern struct ReplaySlot *gGhosts[EXTENT(4)];
extern PakErrorState gPakError[EXTENT(4)][2];
extern tStatList extra_overlay_bss_start;
extern f32 start_overlay_bss_start[EXTENT(12)];

/* Functions */
void **_ReadGhostHeader(void **fileh);
void AddPlayerScores(tProfile **profile);
void AddPlayerStats(tProfile **profile);
void *allocate_memory_at(u32 addr, u32 size);
s32 animControllerDimInternal(tBlit *spr);
s32 CalcChecksum(void);
void CalcUnlockedStuff(void);
void CancelPakError(s32 pak);
s32 CheckAndRepairGhost(struct Ghost **ghosth, s32 data, s32 repair);
void ClearFlush(s32 index);
s32 ClearPlayerRecords(tProfile **profile);
void ClearTotalStats(void);
s32 CountBits(u32 x, u8 n);
void **CreatePlayer(s32 pak, u8 *name);
void DecodeGhost(struct Obj **h);
void DeleteGhost(struct Obj **ghosth);
void DeletePlayer(void **h);
void DrawPakErrNoPak(s32 row, s32 col);
s16 FindTempNameIdx(u8 *name);
void FindWinner(void);
void FoundCoin(s32 player, s32 bit);
void FreeGhost(void **ghosth);
s32 game_memcmp(void *memory, u8 *right, s32 count);
char *game_strncat(char *s1, const char *s2, u32 n);
u8 *game_strstr(u8 *haystack, u8 *needle);
s8 GetBattleSetting(tProfile **profile, u8 selector);
s8 GetDefaultOption(s32 selector);
s8 GetObstacleSetting(tProfile **profile, u8 selector);
s32 GetPasswordElement(s32 *value, u32 nbits);
u8 GetPlayerCar(tProfile **profile);
s8 GetPlayerCarSetting(tProfile **profile, u8 car, u8 setting);
u8 GetPlayerCarTrans(tProfile **profile);
u8 GetPlayerCarView(tProfile **profile);
u8 GetPlayerRaceType(tProfile **profile);
u8 GetPlayerTrack(tProfile **profile, s32 raceType);
s8 GetRaceSetting(tProfile **profile, u8 selector, s32 mode);
s8 GetStuntSetting(tProfile **profile, u8 selector);
void heap_move_block_low(void *addr);
void highscore_InitScreen(void);
void InitBattlePoints(void);
void InitOptions(void);
void InitPlayer(void **h);
void InitRecords(void);
s8 IsCarUnlocked(s32 row, s32 col);
s32 IsCircuitUnlocked(s32 field);
u32 IsCoinFound(s32 player, s32 bit);
s8 IsEngineUnlocked(s32 row, s32 column);
s8 IsFrameUnlocked(s32 row, s32 col);
s8 IsTiresUnlocked(s32 row, s32 column);
s32 IsTrackUnlocked(s32 idx);
s8 IsTransUnlocked(s32 row, s32 column);
void load_extra_overlay(void);
void load_overlay(u8 *dst, char *name, u8 *end, u8 *bssEnd, u8 *romStart, u8 *bssStart, s8 *loaded, u8 *rom);
void load_start_overlay(void);
void MovePlayerFile(tPakHandle *from, tPakHandle *to);
void **NewGhost(void);
s32 NewPassword(u32 dataBits, u32 checksumBits);
void **NewPlayer(s32 pak, s32 slot);
u8 NumCoinsFound(s32 index, s32 lane);
void pak_decode_name(u8 *out, u8 *in, u8 len);
void pak_encode_name(u8 *output, u8 *input, u8 limit);
s32 pak_error_from_pfs(s32 error);
s32 pak_file_unchanged(tPakHandle *handle);
s32 pak_find_dirty_run(tPakHandle *handle, u8 **address, s32 *bytes, s32 *offset);
s32 pak_get_free_bytes(s32 pak);
void pak_insert_file(tPakHandle *fileh);
void pak_mark_dirty(tPakHandle *handle, u8 *address, u32 bytes, s32 mark);
s32 Pow(s32 exp);
void PS_DeleteFile(tPakHandle *fileh);
void PS_ErrorFunc(s32 pak, s32 mode, s32 blocks, s32 unused, intptr_t retry, intptr_t status, s32 (*poll)(void));
s32 PS_FlushFile(tPakHandle *handle, s32 mode);
s32 PS_FlushFileInternal(tPakHandle *handle, s32 mode);
s32 PS_FlushPak(s32 index, s32 mode);
s32 PS_GetBlocks(s32 bytes);
u32 PS_GetFreeBlocks(s32 pak);
s32 PS_GetFreeFiles(s32 pak);
void PS_InitPakSys(void (*errorFunc)(s32, s32, u32, s32, s8 *, s8 *, s32 (*)(s32)));
void PS_LockFile(tPakHandle *handle, s32 active);
s8 PS_PakChanged(s32 pak);
s8 PS_PakDamaged(s32 pak);
s32 PS_PakFormatted(s32 pak);
s8 PS_PakInserted(s32 pak);
s8 PS_PakRumble(s32 pak);
void PS_PakServer(s32 forceUpdate, s32 rumbleOnly);
void PS_PurgeFile(struct Obj **h);
void PS_ReadFile(void **fileh);
void PS_ReadPartialFile(struct PakHandle *fileh, s32 offset, u32 numBytes);
void PS_RumbleGo(void);
void PS_RumbleStop(void);
void PS_SetRumblePercent(s32 pak, u32 value);
void PurgeGhostData(struct Obj **h);
void PurgePlayerData(tProfile **profile);
s32 ReadGhostData(struct Obj **h);
s32 ReadGhostDataInternal(struct Ghost **ghosth, s32 decode, s32 select);
void **ReadPlayer(void **fileh);
s32 ReadPlayerData(void **h);
void RecordBattle(void);
void RecordBattlePoints(s8 row, s8 column, s8 delta);
void RecordCircuit(void);
void RecordGameOver(void);
void RecordGhost(void);
void RecordLap(s32 player, f32 time);
void RecordObstacle(void);
void RecordPractice(void);
void RecordRace(void);
void RecordStunt(void);
void RemovePak(s32 pak);
void RemovePlayerScores(intptr_t handle);
void ResetHighScores(void);
s32 save_file_pak_ready(struct Profile **profile, s32 action);
void SaveFlush(s32 action);
s32 SaveGhost(s32 pak, struct Obj **player, struct Obj **saveGhosth, s32 limbo);
void SaveGhostInLimbo(tGhostHeader *entry, void *payload);
void SaveGlobals(void);
void SaveInit(void);
void SavePlayerCar(tProfile **profile, u8 value);
void SavePlayerCarSetting(tProfile **profile, u8 index, u8 field, s8 value);
void SavePlayerCarTrans(tProfile **profile, u8 value);
void SavePlayerCarView(tProfile **profile, u8 value);
void SavePlayerCircuit(tProfile **profile);
void SavePlayerCircuitStats(tProfile **profile, u8 kind);
void SavePlayerCoins(tProfile **profile, u8 track);
void SavePlayerControls(tProfile **profile, u8 *controls);
void SavePlayerRaceType(tProfile **profile, s32 raceType);
void SavePlayerStats(tProfile **profile, u8 track);
void SavePlayerTrack(tProfile **profile, s32 raceType, u8 value);
void SaveSelectPlayer(intptr_t profile);
void SaveServer(s32 forceUpdate, s32 rumbleOnly);
void SetDefaultCarSettings(void *profile, s32 car);
void SetOptions(void);
void SetPakError(s32 player, s32 mode, s32 x, s32 y, s32 w, s32 h);
s32 SetPasswordString(u8 *str);
s32 StorePasswordElement(u32 value, u32 nbits);
void unload_extra_overlay(void);
void unload_start_overlay(void);
void UnloadFrontEndWaves(void);
void video_calcMenuRect(void);
void video_initScreen(void);
void video_uninitButtons(void);
void video_uninitScreen(void);
void ZOID_EndFrame(s32 arg0);

#endif /* GAME_SAVE_H */
