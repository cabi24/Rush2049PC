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

/* A profile's save data, in bytes (PS_GetBlocks: the pages a profile takes). */
#define PROFILE_BYTES 0x810

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

/* ---- The records. types.h names them (typedef struct tPak tPak ...) for the headers before this one.
 *
 * In memory: the pak system's paks (tPak), their files (tPakFile) and the open files (PakFileRecord, reached through
 * a tPakHandle); a profile (tProfile) and a ghost (tGhostFile) for each profile and ghost file found on a pak, each
 * reached through a handle (tProfile **, tGhostFile **). On the pak: a profile's file (tSaveData) and a ghost file's
 * header (tGhostHeader), whose layouts are the files', byte for byte (docs/port/SAVES.md). */

/* One of the 16 files of a pak, as the pak system keeps it (tPak.files, by the file's number on the pak). */
struct tPakFile {
    s8 locked;           /* in use (PS_LockFile): the pak's files stay while it is out */
    s8 dirty;            /* holds data PS_FlushFile is yet to write */
    s8 flag2;            /* cleared with the others; PS_WriteFile tests it and does nothing */
    u8 unused;
    u16 mapBytes;        /* the size of its dirty map: one bit for each 32 bytes of its data, after the data */
    u16 unused2;
    PakFileState state;  /* its directory entry (osPfsFileState) */
};

/* One controller's pak, as the pak system keeps it (gPaks; MAT3 paksysdc.c's anon_changed_inserted_25c, whose flags
 * are these but formatted and in another order). The rumble thread keeps the motor's state in it too. */
struct tPak {
    s8 changed;          /* PS_PakServer found it put in, taken out or another pak (PS_PakChanged) */
    s8 inserted;         /* a pak the pak system has read is in (PS_PakInserted) */
    s8 plugged;          /* something is plugged into the controller (pak_poll_inserted) */
    s8 wasRemoved;       /* the pak was taken out while its files were kept (another port has it now) */
    s8 inited;           /* pfs holds a pak that was read (its id tells whether the next pak is the same) */
    s8 damaged;          /* the pak can't be read (PS_PakDamaged) */
    s8 rumble;           /* the pak is a Rumble Pak (PS_PakRumble) */
    s8 flushing;         /* PS_FlushPak is writing it */
    s8 flushed;          /* PS_FlushPak wrote something */
    s8 original;         /* the pak in is the one the files came from */
    s8 locked;           /* one of its files is locked (tPakFile.locked) */
    s8 flush;            /* one of its files is dirty */
    PcPfs pfs;           /* the pak's libultra handle */
    s32 error;           /* the last error (kPS_Err*) */
    s32 FreeSpace;       /* free bytes (osPfsFreeBlocks) */
    u8 motorOn;          /* the rumble thread's: the motor is running */
    u8 request;          /* the rumble wanted, 0..40 (PS_SetRumblePercent) */
    u8 level;            /* the rumble now: falls one a retrace towards request */
    u8 unused;
    s16 phase;           /* the retrace in the rumble's 10-retrace duty cycle (rumble_duty_masks) */
    u8 unused2[2];
    tPakFile files[16];
};

/* The pak system's error callback (gErrorFunc; PS_InitPakSys sets it to PS_ErrorFunc, the pak error dialog): it shows
 * error `error` (kPS_Err*) on pak `pak`, with the pages a file needs (`blocks`), and sets *retry to try again; poll,
 * when given, says whether the pak was taken out meanwhile. */
typedef void (*PakErrorFunc)(s32 pak, s32 error, u32 blocks, s32 unused, s8 *retry, s8 *status, s32 (*poll)(s32));

#define NUM_PAKS 4          /* one a controller */
#define PAK_MAX_FILES 16    /* the files a pak holds (tPak.files) */
#define PAK_BLOCK_SIZE 256  /* a pak's page: files are sized in them */

/* A pak file's handle: what the pak system's callers hold (MAT3 tPS_File **). pak_file_handles holds 64, each the
 * handle of one of pak_file_records; the free ones are pak_free_handles. */
struct tPakHandle {
    PakFileRecord *file;
};

/* A pak error dialog (gPakError[4][2], two a pak; MAT3 pakerr.c's anon_done_choice1_34, whose names these are, in
 * the N64's order). SetPakError opens one, ShowPakError runs and draws it, CancelPakError closes it. */
struct PakErrorState {
    s8 done;          /* answered or timed out */
    s8 choice1;       /* the first answer chosen (of two or three) */
    s8 choice2;       /* the second answer chosen (of three) */
    s8 cont;          /* the controller that answers (any, for kPS_ErrNoCont and kPS_ErrNoPak) */
    s8 contActive[4]; /* the controllers' active flags before the dialog, put back after it */
    s8 contStick[4];  /* their stick edges before it */
    s8 contDebug[4];
    s8 rumble;        /* a Rumble Pak is in (the new-pak dialog says so) */
    s32 err;          /* the dialog (kPS_Err*), 0 when closed */
    s16 left;         /* its box on the screen */
    s16 top;
    s16 width;
    s16 height;
    u32 edges;        /* the buttons pressed this frame */
    u32 levels;       /* held */
    u32 timeout;      /* the retrace count at which a waiting dialog closes itself */
};

/* An open pak file (MAT3 tPS_File): pak_file_records holds 64, reached through their handles (pak_file_handles). Each
 * pak's open files are a list of handles (gPS_Files, linked through next and prev). Profiles and ghosts set the two
 * functions (RemovePlayerFile, MovePlayerFile); the pak system calls RemoveFile when the file goes. */
struct PakFileRecord {
    tPakHandle *next;                              /* the next file's handle on its pak's list (gPS_Files) */
    tPakHandle *prev;
    void (*RemoveFile)(tPakHandle *);              /* called when the file goes (the pak removed, the file deleted) */
    void (*MoveFile)(tPakHandle *, tPakHandle *);  /* called when its record moves to another handle */
    u8 pak;                                        /* the controller it is on */
    u8 fileNo;                                     /* its number on the pak (tPak.files) */
    u8 name[35];                                   /* its name and extension, in the game's characters */
    u8 ext[11];                                    /*   (pak_decode_name) */
    u32 size;                                      /* bytes */
    u32 blocks;                                    /* pages of PAK_BLOCK_SIZE */
    u8 **data;                                     /* its data in the heap (a handle), then its dirty map; NULL until
                                                    * read */
};


/* The parts of a profile file (tSaveData), each at its offset in the file. Each starts with the checksum of the rest
 * (CalcChecksumInternal), which CheckAndRepairPlayer checks. MAT3's layouts and names (save.c's anon_checksum_*). */
typedef struct tRaceSettings { /* 0x14 bytes */
    u32 checksum;   /* 0x0 */
    u8 track;       /* 0x4 */
    u8 car;         /* 0x5 */
    u8 trans;       /* 0x6 */
    u8 carView;     /* 0x7 */
    s8 laps;        /* 0x8 */
    s8 drones;      /* 0x9 */
    s8 gravity;     /* 0xA */
    s8 difficulty;  /* 0xB */
    s8 catchup;     /* 0xC */
    s8 backwards;   /* 0xD */
    s8 mirror;      /* 0xE */
    s8 fog;         /* 0xF */
    s8 wind;        /* 0x10 */
    s8 deaths;      /* 0x11 */
    u8 pad12[2];    /* 0x12 */
} tRaceSettings;

typedef struct tBattleSettings { /* 0x10 bytes */
    u32 checksum;  /* 0x0 */
    u8 track;      /* 0x4 */
    u8 car;        /* 0x5 */
    u8 trans;      /* 0x6 */
    u8 carView;    /* 0x7 */
    s8 gravity;    /* 0x8 */
    s8 mirror;     /* 0x9 */
    s8 fog;        /* 0xA */
    s8 wind;       /* 0xB */
    s8 mode;       /* 0xC */
    s8 points;     /* 0xD */
    s8 minutes;    /* 0xE */
    s8 neg;        /* 0xF */
} tBattleSettings;

typedef struct tStuntSettings { /* 0x14 bytes */
    u32 checksum;  /* 0x0 */
    u8 track;      /* 0x4 */
    u8 car;        /* 0x5 */
    u8 trans;      /* 0x6 */
    u8 carView;    /* 0x7 */
    s8 gravity;    /* 0x8 */
    s8 mirror;     /* 0x9 */
    s8 fog;        /* 0xA */
    s8 wind;       /* 0xB */
    s8 mode;       /* 0xC */
    s8 flow;       /* 0xD */
    s8 points;     /* 0xE */
    s8 minutes;    /* 0xF */
    s8 deaths;     /* 0x10 */
    u8 pad11[3];   /* 0x11 */
} tStuntSettings;

typedef struct tObstacleSettings { /* 0xC bytes */
    u32 checksum;  /* 0x0 */
    u8 track;      /* 0x4 */
    u8 car;        /* 0x5 */
    u8 trans;      /* 0x6 */
    u8 carView;    /* 0x7 */
    s8 gravity;    /* 0x8 */
    s8 mirror;     /* 0x9 */
    s8 fog;        /* 0xA */
    s8 wind;       /* 0xB */
} tObstacleSettings;

/* A track's race records and totals (one per track; gTotalRaceStats sums them over the profiles). */
typedef struct tRaceStats { /* 0x60 bytes */
    u32 checksum;              /* 0x0 */
    f32 scores[3][5];          /* 0x4 the five best laps, average laps and ghost race times (RecordRace,
                                *      RecordGhost; gTotalNames: their owners, gNewTotalScore: this race's) */
    f32 totalTime;             /* 0x40 */
    u16 totalLaps;             /* 0x44 */
    u16 totalRaces;            /* 0x46 */
    u16 total1st;              /* 0x48 */
    u16 total2nd;              /* 0x4A */
    u16 total3rd;              /* 0x4C */
    u16 totalCrashes;          /* 0x4E */
    u16 totalDeathAttempted;   /* 0x50 */
    u16 totalDeathCompleted;   /* 0x52 */
    u16 totalGhosts;           /* 0x54 */
    u8 pad56[2];               /* 0x56 */
    u32 totalMiles;            /* 0x58 */
    u16 coinsFound;            /* 0x5C a bit for each coin found on the track */
    u8 pad5E[2];               /* 0x5E */
} tRaceStats;

/* A stunt arena's records and totals. */
typedef struct tStuntStats { /* 0x40 bytes */
    u32 checksum;          /* 0x0 */
    u32 bestStunt;         /* 0x4 */
    u32 bestAvg;           /* 0x8 */
    u32 totalPoints;       /* 0xC */
    u16 totalTime;         /* 0x10 */
    u16 totalCrashes;      /* 0x12 */
    u32 totalStunts[10];   /* 0x14 each kind of stunt */
    u16 coinsFound;        /* 0x3C */
    u8 pad3E[2];           /* 0x3E */
} tStuntStats;

/* A battle arena's totals. */
typedef struct tBattleStats { /* 0xC bytes */
    u32 checksum;       /* 0x0 */
    u16 totalMatches;   /* 0x4 */
    u16 totalWins;      /* 0x6 */
    u16 totalKills;     /* 0x8 */
    u16 totalDeaths;    /* 0xA */
} tBattleStats;

/* The obstacle course's records and totals. */
typedef struct tObstacleStats { /* 0x18 bytes */
    u32 checksum;            /* 0x0 */
    f32 bestTime;            /* 0x4 */
    f32 totalTime;           /* 0x8 */
    u16 totalRaces;          /* 0xC */
    u16 totalFinished;       /* 0xE */
    u16 totalCrashes;        /* 0x10 */
    u16 numDeathAttempted;   /* 0x12 */
    u16 numDeathCompleted;   /* 0x14 */
    u8 pad16[2];             /* 0x16 */
} tObstacleStats;

/* A circuit's records and totals (one per circuit). */
typedef struct tCircuitStats { /* 0x1C bytes */
    u32 checksum;            /* 0x0 */
    u16 numCircuits;         /* 0x4 */
    u16 num1st;              /* 0x6 */
    u16 num2nd;              /* 0x8 */
    u16 num3rd;              /* 0xA */
    u16 numDeathAttempted;   /* 0xC */
    u16 numDeathCompleted;   /* 0xE */
    u16 bestPts;             /* 0x10 */
    u8 pad12[2];             /* 0x12 */
    f32 bestTime;            /* 0x14 */
    u32 totalPts;            /* 0x18 */
} tCircuitStats;

/* A car's settings (the car setup screen's sliders). */
typedef struct tCarSettings { /* 0x10 bytes */
    u32 checksum;      /* 0x0 */
    s8 settings[12];   /* 0x4 the checksum covers all 12 (SavePlayerCarSetting) */
} tCarSettings;

/* A profile's file on the pak (MAT3 save.c's tSaveData, the N64's revision 16 of it: controls 10 bytes, then rumble
 * and the race type), 2064 bytes. A file format: its layout is the pak file's, byte for byte, and the offsets are
 * the file's. */
struct tSaveData { /* 0x810 bytes */
    u32 nameChecksum;                     /* 0x0 over id, revision and name */
    u32 id[2];                            /* 0x4 the profile's id: the creation time, the record's address and pak */
    u8 revision;                          /* 0xC PROFILE_REVISION */
    u8 name[13];                          /* 0xD */
    u8 pad1A[2];                          /* 0x1A */
    u32 optChecksum;                      /* 0x1C */
    s8 options[21];                       /* 0x20 the game options (gOptions' first 21) */
    u8 pad35[3];                          /* 0x35 */
    u32 contChecksum;                     /* 0x38 over the controls and rumble */
    s8 controls[10];                      /* 0x3C */
    s8 rumble;                            /* 0x46 */
    u8 raceType;                          /* 0x47 the race type last played (gRace) */
    tRaceSettings raceSettings;           /* 0x48 */
    tBattleSettings battleSettings;       /* 0x5C */
    tStuntSettings stuntSettings;         /* 0x6C */
    tObstacleSettings obstacleSettings;   /* 0x80 */
    tRaceStats raceStats[12];             /* 0x8C */
    tStuntStats stuntStats[4];            /* 0x50C */
    tBattleStats battleStats[8];          /* 0x60C */
    tObstacleStats obstacleStats[1];      /* 0x66C */
    tCircuitStats circuitStats[4];        /* 0x684 */
    tCircuitRecord circuit;               /* 0x6F4 the circuit in progress (wave 4: tCircuit) */
    tCarSettings carSettings[13];         /* 0x740 */
};

#define PROFILE_FILE_SIZE 2064
#define PROFILE_REVISION 16   /* tSaveData.revision (MAT3's is 19) */

/* A ghost file's header (MAT3 save.c's tSaveHeader, the N64's 0x58 bytes): file data, so in the file's layout, with
 * the file's offsets; the compressed recording follows it. The race's recording headers (gRealNewGhostHeaders) have
 * the same layout.
 *
 * Its last three words are the replay's, not the file's (0 in every file): MAT3's start, the samples' address, then
 * the step and the number of steps. A host pointer doesn't fit the file's 4 bytes, so on the PC the header says only
 * whether it has its samples (hasSamples), and the samples are found by what owns them: a ghost's decoded recording
 * is its data block (tGhostFile.datah), a race being recorded is its player's block (ghost_recording, NewGhost.c). */
struct tGhostHeader { /* 0x58 bytes */
    u32 headerChecksum;     /* 0x0 over revision .. unzippedSize */
    u8 revision;            /* 0x4 GHOST_REVISION */
    s8 flag;                /* 0x5 the replay: -1 recording, 1 playing back (DecodeGhost), 0 neither */
    s8 track;               /* 0x6 */
    u8 backward;            /* 0x7 */
    u8 car;                 /* 0x8 the car raced (MAT3 car) */
    u8 carTrans;            /* 0x9 its transmission (MAT3 trans) */
    s8 settings[12];        /* 0xA handling, engine, tires, frame, wings, the three colours, rims, horn, front and
                             * rear tire size */
    u8 name[13];            /* 0x16 the driver's */
    u8 pad23;               /* 0x23 */
    u16 tournamentKey;      /* 0x24 */
    u8 pad26[2];            /* 0x26 */
    u32 playerId[2];        /* 0x28 the driver's profile id */
    u32 id;                 /* 0x30 the ghost's number: the driver's ghost races on the track before it (its profile's
                             * raceStats totalGhosts; MAT3 id) */
    f32 frameTime;          /* 0x34 the model step it was recorded at (MAT3 retraceTime) */
    f32 finish;             /* 0x38 the race time */
    u32 zippedSize;         /* 0x3C */
    u32 unzippedSize;       /* 0x40 */
    u32 zippedChecksum;     /* 0x44 over the compressed recording */
    u32 unzippedChecksum;   /* 0x48 */
    u32 hasSamples;         /* 0x4C 1 while the replay's samples are in memory, else 0 (MAT3 start: above) */
    u32 offset;             /* 0x50 the replay's step */
    u32 size;               /* 0x54 its steps: each of the samples' three channels is size bytes long */
};

#define GHOST_REVISION 17

/* A ghost on a pak, as the ghost list keeps it (MAT3 tGhostFile): gGhostFiles lists them, fastest first, linked
 * through their handles (next). */
struct tGhostFile {
    tGhostFile **next;        /* the next ghost's handle on gGhostFiles (MAT3 ll.next; the list is singly linked) */
    tPakHandle *fileh;        /* its pak file */
    s8 track;
    u8 backward;
    u8 name[13];              /* the driver's */
    u8 unused;
    u32 playerId[2];          /* the driver's profile id */
    f32 finish;               /* the race time */
    u8 **datah;               /* the recording once read */
    tGhostHeader **header;    /* the file's data (its header first) once read */
};

/* A profile in memory (MAT3 save.c's tPlayerProfile): the player's name, controls and the handle of the save data.
 * Each pak's profiles are player_profile_records, reached through handles (player_profile_handles); gPlayerFiles
 * lists the profiles read, linked through their handles (next, prev). gRealJustPlayers are the four without a pak
 * ("Player n"), gRealGhostPlayers the drivers of a ghost race's ghosts. */
struct tPlayerProfile {
    tProfile **next;          /* the next profile's handle on gPlayerFiles (MAT3 ll.next) */
    tProfile **prev;          /* the one before (MAT3 ll.prev) */
    tPakHandle *fileh;        /* its pak file, NULL for a profile without one */
    u32 id[2];                /* tSaveData.id */
    u8 name[13];
    s8 controls[10];          /* the buttons' functions (SavePlayerControls) */
    s8 rumble;                /* the Rumble Pak on (the pause screen's rumble page) */
    tSaveData **datah;        /* its file's data once read (the pak file's data handle), else NULL */
};

/* Who set a time in the high score tables (gTotalNames; the high score screen's gThisPlayer): a profile's handle, or
 * for a player without a profile the name typed on the high score screen, by its index in gTempNames (0 to 19); -1
 * for none. MAT3 keeps both in the handle (tPlayerProfile **); here they are a union, pointer-sized either way. */
typedef union tScoreOwner {
    intptr_t name;            /* the typed name's index in gTempNames, or -1 */
    tProfile **profile;       /* the profile's handle */
} tScoreOwner;

/* Data */
extern u8 rumble_thread_stack_top[EXTENT(4096)];
extern u8 rumble_thread[EXTENT(432)];
extern OSMesgQueue si_event_queue;
extern s8 gSaveGhostFiles;
extern s8 gCheatActive[EXTENT(21)];
extern s16 gCheatState[EXTENT(21)];   /* the cheats' states, by number (cheatscr.c; MAT3 signed short gCheatState[23]) */
/* The unlock-everything cheats' states, gCheatState's entries 14 to 16 (cheatscr.c sets them by number). */
#define cheat_all_cars (gCheatState[14])
#define cheat_all_tracks (gCheatState[15])
#define cheat_all_parts (gCheatState[16])
extern tBlitDef PS_ErrorFunc_dimMultiBlit[EXTENT(1)];
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
extern const tBlitDef video_frontEndBlit[EXTENT(2)];
extern const tBlitDef video_pauseBlit[EXTENT(2)];
extern s8 video_inited;
extern tBlit *video_blit;
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
extern AudioList gPlayerFiles;   /* the profiles read from the paks, in name order: a list of their handles */
extern tGhostFile **gLimboGhosts;   /* the ghosts kept in memory ("limbo"), three a track: gRealLimboGhosts' handles */
extern tProfile gRealJustPlayers[EXTENT(4)];
extern tSaveData *gJustPlayerData[EXTENT(4)];   /* their data (gRealJustPlayerData), through these handles */
extern tSaveData gRealJustPlayerData[EXTENT(4)];
extern tProfile *player_profile_handles[EXTENT(4)][16];   /* by pak and file number: a profile's handle */
extern tProfile player_profile_records[EXTENT(4)][16];    /* the profiles they hold (NewPlayer) */
extern tGhostFile *gRealLimboGhosts;   /* the records gLimboGhosts' handles hold, in the heap (SaveInit) */
extern u8 gNumLimbo;
extern s8 gRankedPlayer[EXTENT(4)];
extern PakErrorFunc gErrorFunc;
extern u8 gLap[EXTENT(4)];
extern tPak gPaks[EXTENT(4)];
extern tPakHandle pak_file_handles[EXTENT(64)];   /* the open files' handles, one a pak_file_records entry */
extern AudioList gPS_Files[EXTENT(4)];   /* each pak's open files: a list of their handles */
extern f32 gBestLapTime[EXTENT(4)];
extern PakFileRecord pak_file_records[EXTENT(64)];
extern s8 gOptions[EXTENT(kNumOptions)];   /* the options, a byte each by kOpt* (MAT3: signed char gOptions[41]) */
extern tProfile *gJustPlayers[EXTENT(4)];   /* the controllers' own profiles, "Player n" (gRealJustPlayers) */
extern s8 gBattlePoints[EXTENT(4)][4];   /* this battle's points, by player: [scorer][victim], the points one scored
                                         * against the other (RecordBattlePoints; negative for its own deaths) */
extern f32 gLapTimes[EXTENT(4)][8];
extern s32 ties;
extern s8 tied[EXTENT(8)];
/* What each player has unlocked (CalcUnlockedStuff, from the players' records), a row per player: MAT3 signed char
 * [4][n]. IsCarUnlocked and the others read them; the high score screen shows what a race unlocked. */
extern s8 gUnlockedTracks[EXTENT(4)][19];
extern s8 gUnlockedCars[EXTENT(4)][13];
extern s8 gUnlockedCircuits[EXTENT(4)][4];
extern s8 gUnlockedTrans[EXTENT(4)][8];
extern s8 gUnlockedEngines[EXTENT(4)][9];
extern s8 gUnlockedTires[EXTENT(4)][5];
extern s8 gUnlockedFrames[EXTENT(4)][6];
extern u8 gForceUnlockedCircuits[EXTENT(4)];
extern tRaceStats gTotalRaceStats[EXTENT(12)];
extern tStuntStats gTotalStuntStats[EXTENT(4)];
extern tBattleStats gTotalBattleStats[EXTENT(8)];
extern tObstacleStats gTotalObstacleStats[EXTENT(1)];
extern tCircuitStats gTotalCircuitStats[EXTENT(4)];
extern tScoreOwner gTotalNames[EXTENT(12)][3][5];   /* by track, the owners of gTotalRaceStats' scores: the five
                                                     * best laps, average laps and ghost race times */
extern char gTempNames[EXTENT(20)][13];
extern u16 gTempNameAges[EXTENT(20)];
extern s8 gNewTotalScore[EXTENT(3)][5];   /* this race's places in the three high score tables of gTotalNames: the
                                            * player holding each, -1 for none */
extern AudioList gGhostFiles;   /* the ghosts on the paks, fastest first: a list of their handles */
extern s8 gTeamPoints[EXTENT(4)];
extern f32 video_buttonSlideSpeed;
/* By car, in a ghost race: the ghost it records or plays back (a handle), NULL for none. A ghost race's cars are its
 * players and ghosts, four at most (GhostStart), so the bound is the game's too: the sanitizer builds check it. */
extern tGhostFile **gGhosts[4];
extern PakErrorState gPakError[EXTENT(4)][2];

/* Functions */
tGhostFile **_ReadGhostHeader(tPakHandle *fileh);
void AddPlayerScores(tProfile **profile);
void AddPlayerStats(tProfile **profile);
void *allocate_memory_at(void *ptr, u32 size);
s32 animControllerDimInternal(tBlit *spr);
s32 CalcChecksum(void);
void CalcUnlockedStuff(void);
void CancelPakError(s32 pak);
s32 CheckAndRepairGhost(tGhostFile **ghosth, s32 data, s32 repair);
void ClearFlush(s32 index);
s32 ClearPlayerRecords(tProfile **profile);
void ClearTotalStats(void);
tGhostFile **CompressGhost(tGhostHeader *header, u8 *blocks);
u8 **ghost_recording(tGhostHeader *header);
s32 CountBits(u32 x, u8 n);
tProfile **CreatePlayer(s32 pak, u8 *name);
void DecodeGhost(tGhostFile **h);
void DeleteGhost(tGhostFile **ghosth);
void DeletePlayer(tProfile **h);
void DrawPakErrNoPak(s32 row, s32 col);
s16 FindTempNameIdx(u8 *name);
void FindWinner(void);
void FoundCoin(s32 player, s32 bit);
void FreeGhost(tGhostFile **ghosth);
s32 game_memcmp(void *memory, u8 *right, s32 count);
char *game_strncat(char *s1, const char *s2, u32 n);
u8 *game_strstr(u8 *haystack, u8 *needle);
s8 GetBattleSetting(tProfile **profile, u8 selector);
s8 GetDefaultOption(s32 selector);
s8 GetObstacleSetting(tProfile **profile, u8 selector);
s32 GetPasswordElement(s32 *value, u32 nbits);
void GetPasswordString(char *out);
u8 GetPlayerCar(tProfile **profile);
s8 GetPlayerCarSetting(tProfile **profile, u8 car, u8 setting);
u8 GetPlayerCarTrans(tProfile **profile);
u8 GetPlayerCarView(tProfile **profile);
s32 GetPlayerOption(tProfile **profile, u8 selector);
u8 GetPlayerRaceType(tProfile **profile);
u8 GetPlayerTrack(tProfile **profile, s32 raceType);
s8 GetRaceSetting(tProfile **profile, u8 selector, s32 mode);
s8 GetStuntSetting(tProfile **profile, u8 selector);
void heap_move_block_low(void *addr);
void highscore_InitScreen(void);
void InitBattlePoints(void);
void InitOptions(void);
void InitPlayer(tProfile **h);
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
void load_overlay(u8 *dst, u8 *end, s8 *loaded);
void load_start_overlay(void);
u8 *runtime_image_block(void);
void MovePlayerFile(tPakHandle *from, tPakHandle *to);
tGhostFile **NewGhost(void);
s32 NewPassword(u32 dataBits, u32 checksumBits);
tProfile **NewPlayer(s32 pak, s32 slot);
u8 NumCoinsFound(s32 index, s32 lane);
void pak_decode_name(u8 *out, u8 *in, u8 len);
void pak_encode_name(u8 *output, u8 *input, u8 limit);
s32 pak_error_from_pfs(s32 error);
s32 pak_file_unchanged(tPakHandle *handle);
s32 pak_find_dirty_run(tPakHandle *handle, u8 **address, s32 *bytes, s32 *offset);
s32 pak_get_free_bytes(s32 pak);
void pak_insert_file(tPakHandle *fileh);
void pak_mark_dirty(tPakHandle *handle, u8 *address, u32 bytes, s32 mark);
tPakHandle *pfs_get_node(void);
s32 Pow(s32 exp);
tPakHandle *PS_CreateFile(s32 pak, u8 *name, u8 *ext, u32 numBytes, s32 company, s32 gameCode);
void PS_DeleteFile(tPakHandle *fileh);
void PS_ErrorFunc(s32 pak, s32 mode, s32 blocks, s32 unused, s8 *retry, s8 *status, s32 (*poll)(s32));
s32 PS_FlushFile(tPakHandle *handle, s32 mode);
s32 PS_FlushFileInternal(tPakHandle *handle, s32 mode);
s32 PS_FlushPak(s32 index, s32 mode);
s32 PS_GetBlocks(s32 bytes);
u32 PS_GetFreeBlocks(s32 pak);
s32 PS_GetFreeFiles(s32 pak);
void PS_InitPakSys(PakErrorFunc errorFunc);
void PS_LockFile(tPakHandle *handle, s32 active);
s8 PS_PakChanged(s32 pak);
s8 PS_PakDamaged(s32 pak);
s32 PS_PakFormatted(s32 pak);
s8 PS_PakInserted(s32 pak);
s8 PS_PakRumble(s32 pak);
void PS_PakServer(s32 forceUpdate, s32 rumbleOnly);
void PS_PurgeFile(tPakHandle *fileh);
void PS_ReadFile(tPakHandle *fileh);
void PS_ReadPartialFile(tPakHandle *fileh, s32 offset, u32 numBytes);
void PS_RumbleGo(void);
void PS_RumbleStop(void);
void PS_SetRumblePercent(s32 pak, u32 value);
void PS_WriteFile(tPakHandle *handle, void *address, u32 bytes);
void PurgeGhostData(tGhostFile **h);
void PurgePlayerData(tProfile **profile);
s32 ReadGhostData(tGhostFile **h);
s32 ReadGhostDataInternal(tGhostFile **ghosth, s32 decode, s32 select);
tProfile **ReadPlayer(tPakHandle *fileh);
s32 ReadPlayerData(tProfile **h);
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
void RemovePlayerFile(tPakHandle *fileh);
void RemovePlayerScores(tProfile **profile);
void ResetHighScores(void);
s32 save_file_pak_ready(tProfile **profile, s32 action);
void SaveFlush(s32 action);
s32 SaveGhost(s32 pak, tProfile **player, tGhostFile **saveGhosth, s32 limbo);
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
void SavePlayerOption(tProfile **player, u8 option, s8 value);
void SavePlayerControls(tProfile **profile, u8 *controls);
void SavePlayerRaceType(tProfile **profile, s32 raceType);
void SavePlayerStats(tProfile **profile, u8 track);
void SavePlayerTrack(tProfile **profile, s32 raceType, u8 value);
void SaveSelectPlayer(tProfile **profile);
void SaveServer(s32 forceUpdate, s32 rumbleOnly);
void SetDefaultCarSettings(tProfile **profile, s32 car);
void SetOptions(void);
void SetPakError(s32 player, s32 mode, s32 x, s32 y, s32 w, s32 h);
s32 SetPasswordString(u8 *str);
void ShowPakError(s32 row, s8 *choice1, s8 *choice2, s8 *done, s32 (*poll)(s32));
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
