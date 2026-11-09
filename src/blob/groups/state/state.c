/* The game state machine (MAT3 state.c): GameLoop calls updateGameState each frame; when setGameState has asked for
 * another state (gNewGameState), the old state is left (uninitGameState: its waves go) and the new one entered
 * (initGameState): which runtime image, which waves and sounds are loaded, which controllers count. The states are
 * system.h's GAME_STATE_* bits. */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct PSFile { void *next, *prev; void *fileh; } PSFile;     /* the profile's host layout (pc_prelude.h) */
typedef struct PadState { u8 pad0[72]; PSFile **save; } PadState;    /* gPlayers[] (tPadState): the profile at +72 */
typedef struct PlayerData { struct PlayerData **next; } PlayerData;  /* handle list, next handle at +0 */
typedef struct { u32 opaque[6]; } OSMesgQueue;
s32 osRecvMesg(OSMesgQueue *mq, void **msg, s32 flag);
s32 osSendMesg(OSMesgQueue *mq, void *msg, s32 flag);

VIEW(s8[], gOptions);
#define gOptions gOptions_view

extern PadState gPlayers[];           /* gPlayers */

struct PlayerFiles { u32 unknown0,  unknown4 ; PlayerData ** head,  ** tail ; };
VIEW(struct PlayerFiles, gPlayerFiles);
#define gPlayerFiles gPlayerFiles_view

extern u8 overlay_base[];                 /* runtime image base (A and B) */
VIEW(OSMesgQueue, heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view


/* Loads runtime image A, the front end (load set), freeing image B first if it is in; or frees image A (load
 * clear). Both share one heap block (overlay_base). Does nothing when image A already is, or isn't, loaded. */
void LoadFrontEndOverlay(s32 load)
{
    static s32 loaded = 0;
    if (load) {
        if (!loaded) {
            if (extra_overlay_loaded) {
                osRecvMesg(&heap_lock_queue, (void **)0, 1);
                DeallocateMemoryInternal(PC_N64A(overlay_base), 0);
                osSendMesg(&heap_lock_queue, (void *)0, 0);
                extra_overlay_loaded = 0;
            }
            load_start_overlay();
            loaded = 1;
        }
    } else if (loaded) {
        if (start_overlay_loaded) {
            osRecvMesg(&heap_lock_queue, (void **)0, 1);
            DeallocateMemoryInternal(PC_N64A(overlay_base), 0);
            osSendMesg(&heap_lock_queue, (void *)0, 0);
            start_overlay_loaded = 0;
        }
        loaded = 0;
    }
}

/* Enters gGameState (MAT3 initGameState):
 *   boot: nothing shown yet; options and records, three frames, then the licence screen;
 *   licence, title (and the screens that set fonts again): the front end's waves and image A, the effects volume
 *     (gOptions[13]), controller 0 active, the sticks driving the menus; the title also locks the players' pak
 *     files unless a skip to a later screen is set, and starts the title music (song 6, gOptions[12] the music
 *     volume, gOptions[14] the song: 13 and up none) with an Expansion Pak;
 *   demo: the players' data and the front end go, image B with an Expansion Pak, a smaller sound set without;
 *   start, players, mode: which controllers count (the players' in the player screen);
 *   circuit, car, track: their screens' waves (57, 56, 60);
 *   race load: unless restarting, the front end's waves and fonts go and the race's fonts are loaded (13, 10 ...),
 *     the sound set for the race kind, the pak flushed; image B for battle, stunt and obstacle (gRace 6, 4, 5) or
 *     with an Expansion Pak; the sticks no longer drive the menus;
 *   race end: the sticks drive the menus again; game over: the records saved to the paks;
 *   0x20000000: the front end's waves go, wave 55 comes.
 * Without an Expansion Pak the sound system is made again at each front-end state (32 voices ...) and the 3D sound
 * tables freed, as there is no room for both. Every front-end state sets both windows' fog to the colour of fog
 * entry 19, far away. */
void initGameState(void)
{
    s32 i;

    if (gGameState & GS_BOOT) {
        gGameStarted = 0;
        gSofdecShown = 0;
        gLicenseShown = 0;
        gIntroMovieShown = 0;
        pfsInited = 0;
        lcdInited = 0;
        InitWaves();
        if (!expansion_pak_present) {
            AudioInit(32, 16, 16, 5, 64, 1.0f, 200.0f, 0);
            AudFree3D();
        }
        LoadFrontEndOverlay(1);
        gNumPlayers = 1;
        gNumWindows = 1;
        SetOptions();
        SetCountdownTimer(30.0f);
        UpdateFrame();
        UpdateFrame();
        UpdateFrame();
        InitRecords();
        setGameState(GS_LICENSE);
    } else if (gGameState & GS_LICENSE) {
        HAL_PurgeFonts();
        InitWaves();
        if (!expansion_pak_present) {
            AudioInit(32, 16, 16, 5, 64, 1.0f, 200.0f, 0);
            AudFree3D();
        }
        AudSetFxVol(gOptions[13] / 10.0f, 0.1f);
        LoadFrontEndOverlay(1);
        LoadFrontEndWaves();
        SetControllerActive(-1, 0);
        SetControllerActive(0, 1);
        SetControllerStickEdges(-1, 1);
    } else if (gGameState & GS_WARNING) {
    } else if (gGameState & GS_TITLE) {
        HAL_PurgeFonts();
        InitWaves();
        if (!expansion_pak_present) {
            AudioInit(32, 16, 16, 5, 64, 1.0f, 200.0f, 0);
            AudFree3D();
        }
        AudSetFxVol(gOptions[13] / 10.0f, 0.1f);
        LoadFrontEndOverlay(1);
        LoadFrontEndWaves();
        if (!(gSkipToGameState & 0x7cdfc0)) {   /* not skipping to a race's start or to a screen from mode on */
            for (i = 0; i < gNumPlayers; i++) {
                if (gPlayers[i].save) {
                    PS_LockFile((*gPlayers[i].save)->fileh, 0);
                }
            }
        }
        SetControllerActive(-1, 0);
        SetControllerActive(0, 1);
        SetControllerStickEdges(-1, 1);
        if (expansion_pak_present && !(gSkipToGameState & GS_RACE_MASK) && gOptions[14] < 13) {
            AudSetMusicVol(gOptions[12] / 10.0f, 0.05f);
            AudPlaySong(6, 1);
        }
        gUpsideDownTrack = 0;
    } else if (gGameState & GS_DEMO) {
        PlayerData **h;
        for (h = gPlayerFiles.head; h; h = (*h)->next) {
            PurgePlayerData(h);
        }
        UnloadFrontEndWaves();
        HAL_PurgeFonts();
        InitWaves();
        if (!expansion_pak_present) {
            AudioInit(8, 8, 0, 0, 0, 1.0f, 200.0f, 0);
        }
        LoadFrontEndOverlay(0);
        if (expansion_pak_present) {
            load_extra_overlay();
        }
    } else if (gGameState & GS_START) {
        SetControllerActive(-1, 0);
        SetControllerActive(0, 1);
        SetControllerStickEdges(-1, 1);
    } else if (gGameState & GS_PLAYER) {
        s32 p;

        for (p = 0; p < gNumPlayers; p++) {
            SetControllerActive(p, 1);
            SetControllerStickEdges(p, 1);
        }
        for (; p < 4; p++) {
            SetControllerActive(p, 0);
        }
    } else if (gGameState & GS_MODE) {
        SetControllerActive(-1, 0);
        SetControllerActive(0, 1);
    } else if (gGameState & GS_OPTIONS) {
    } else if (gGameState & GS_CONTROLS) {
    } else if (gGameState & GS_AUDIO) {
    } else if (gGameState & GS_VIDEO) {
    } else if (gGameState & GS_RECORDS) {
    } else if (gGameState & GS_CHEAT) {
    } else if (gGameState & GS_CIRCUIT) {
        loading_effect_none();
        LoadWave(57, 0, 0, 0, 0);
        loading_effect_none();
    } else if (gGameState & GS_CAR) {
        s32 p;

        LoadWave(56, 0, 0, 0, 0);
        for (p = 0; p < gNumPlayers; p++) {
            SetControllerActive(p, 1);
            SetControllerStickEdges(p, 1);
        }
        for (; p < 4; p++) {
            SetControllerActive(p, 0);
        }
    } else if (gGameState & GS_TRACK) {
        loading_effect_none();
        LoadWave(60, 0, 0, 0, 0);
        SetControllerActive(-1, 0);
        SetControllerActive(0, 1);
        loading_effect_none();
    } else if (gGameState & GS_GAME_INIT) {
        if (!gRestartGame) {
            UnloadFrontEndWaves();
            HAL_PurgeFonts();
            InitWaves();
        }
        LoadFrontEndOverlay(0);
        if (!gRestartGame) {
            if (!expansion_pak_present && gRace != kBattle && gRace != kStunt && gRace != kObstacle) {
                AudioInit(8, 8, 0, 0, 0, 1.0f, 200.0f, 0);
            }
            if (!expansion_pak_present && gRace == kBattle) {
                AudInit3D(5, 64, 1.0f, 200.0f, 0);
            }
            SaveFlush(1);
        }
        if (gRace == kBattle || gRace == kStunt || gRace == kObstacle || expansion_pak_present) {
            load_extra_overlay();
        }
        if (!gRestartGame) {
            HAL_SetFont(13);
            HAL_SetFont(10);
            HAL_SetFont(11);
            HAL_SetFont(12);
            HAL_SetFont(4);
            HAL_SetFont(1);
            HAL_SetFont(2);
            HAL_SetFont(3);
            HAL_SetFont(9);
            HAL_SetFont(8);
            HAL_SetFont(5);
            HAL_SetFont(6);
            HAL_SetFont(7);
        }
        SetControllerStickEdges(-1, 0);
    } else if (gGameState & GS_END_GAME) {
        SetControllerStickEdges(-1, 1);
    } else if (gGameState & GS_GAME_OVER) {
        SaveServer(1, 0);
        SaveFlush(1);
    } else if (gGameState & 0x20000000) {
        UnloadFrontEndWaves();
        InitWaves();
        LoadWave(55, 0, 0, 0, 0);
    }
    if (gGameState & GS_FRONTEND_MASK) {
        ZOID_SetFog(1000, 999, gFogColors[19][0], gFogColors[19][1], gFogColors[19][2], 0xff, 0);
        ZOID_SetFog(1000, 999, gFogColors[19][0], gFogColors[19][1], gFogColors[19][2], 0xff, 1);
    }
}

/* Changes state if setGameState asked for one: leaves the old (uninitGameState), enters the new (initGameState). */
void updateGameState(void)
{
    if (gNewGameState != gGameState) {
        uninitGameState();
        gGameState = gNewGameState;
        initGameState();
    }
}

/* Where MAT3 starts and ends its loading effect (leStartLoadingEffect): nothing on the N64. */
void loading_effect_none(void)
{
}
