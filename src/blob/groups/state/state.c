#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct PSFile { void *next, *prev; void *fileh; } PSFile;     /* the profile's host layout (pc_prelude.h) */
typedef struct PadState { u8 pad0[72]; PSFile **save; } PadState;    /* gPlayers[], 76 bytes, save at +72 */
typedef struct PlayerData { struct PlayerData **next; } PlayerData;  /* handle list, next handle at +0 */
typedef struct { u32 opaque[6]; } OSMesgQueue;
s32 osRecvMesg(OSMesgQueue *mq, void **msg, s32 flag);
s32 osSendMesg(OSMesgQueue *mq, void *msg, s32 flag);

VIEW(u32, gNewGameState);
#define gNewGameState gNewGameState_view

VIEW(s8[], gOptions);
#define gOptions gOptions_view

extern PadState gPlayers[];           /* gPlayers */

struct PlayerFiles { u32 unknown0,  unknown4 ; PlayerData ** head,  ** tail ; };
VIEW(struct PlayerFiles, gPlayerFiles);
#define gPlayerFiles gPlayerFiles_view
VIEW(u8[][3], gFogColors);
#define gFogColors gFogColors_view

extern u8 overlay_base[];                 /* runtime image base (A and B) */
VIEW(OSMesgQueue, heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view

void *ZOID_SetFog(s16 a0, s16 a1, u8 r, u8 g, u8 b, u8 a, u32 win);

/* The heap's API takes N64 addresses: N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
void LoadFrontEndOverlay(s32 load)           /* LoadFrontEndOverlay */
{
    static s32 loaded = 0;
    if (load) {
        if (!loaded) {
            if (extra_overlay_loaded) {
                osRecvMesg(&heap_lock_queue, (void **)0, 1);
                DeallocateMemoryInternal(N64A(overlay_base), 0);
                osSendMesg(&heap_lock_queue, (void *)0, 0);
                extra_overlay_loaded = 0;
            }
            load_start_overlay();
            loaded = 1;
        }
    } else if (loaded) {
        if (start_overlay_loaded) {
            osRecvMesg(&heap_lock_queue, (void **)0, 1);
            DeallocateMemoryInternal(N64A(overlay_base), 0);
            osSendMesg(&heap_lock_queue, (void *)0, 0);
            start_overlay_loaded = 0;
        }
        loaded = 0;
    }
}

void initGameState(void)                /* initGameState@state.c (static in MAT3; deleted after inlining, stub 0x800CA2F8) */
{
    s32 i;

    if (gGameState & 0x1) {
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
        setGameState(0x2);
    } else if (gGameState & 0x2) {
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
    } else if (gGameState & 0x40000000) {
    } else if (gGameState & 0x4) {
        HAL_PurgeFonts();
        InitWaves();
        if (!expansion_pak_present) {
            AudioInit(32, 16, 16, 5, 64, 1.0f, 200.0f, 0);
            AudFree3D();
        }
        AudSetFxVol(gOptions[13] / 10.0f, 0.1f);
        LoadFrontEndOverlay(1);
        LoadFrontEndWaves();
        if (!(gSkipToGameState & 0x7cdfc0)) {
            for (i = 0; i < gNumPlayers; i++) {
                if (gPlayers[i].save) {
                    PS_LockFile((*gPlayers[i].save)->fileh, 0);
                }
            }
        }
        SetControllerActive(-1, 0);
        SetControllerActive(0, 1);
        SetControllerStickEdges(-1, 1);
        if (expansion_pak_present && !(gSkipToGameState & 0x7c0000) && gOptions[14] < 13) {
            AudSetMusicVol(gOptions[12] / 10.0f, 0.05f);
            AudPlaySong(6, 1);
        }
        gUpsideDownTrack = 0;
    } else if (gGameState & 0x8) {
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
    } else if (gGameState & 0x10) {
        SetControllerActive(-1, 0);
        SetControllerActive(0, 1);
        SetControllerStickEdges(-1, 1);
    } else if (gGameState & 0x20) {
        s32 p;

        for (p = 0; p < gNumPlayers; p++) {
            SetControllerActive(p, 1);
            SetControllerStickEdges(p, 1);
        }
        for (; p < 4; p++) {
            SetControllerActive(p, 0);
            if (p > 1) {
            }
        }
    } else if (gGameState & 0x40) {
        SetControllerActive(-1, 0);
        SetControllerActive(0, 1);
    } else if (gGameState & 0x200) {
    } else if (gGameState & 0x400) {
    } else if (gGameState & 0x800) {
    } else if (gGameState & 0x1000) {
    } else if (gGameState & 0x2000) {
    } else if (gGameState & 0x8000000) {
    } else if (gGameState & 0x4000) {
        loading_effect_none();
        LoadWave(57, 0, 0, 0, 0);
        loading_effect_none();
    } else if (gGameState & 0x100) {
        s32 p;

        LoadWave(56, 0, 0, 0, 0);
        for (p = 0; p < gNumPlayers; p++) {
            SetControllerActive(p, 1);
            SetControllerStickEdges(p, 1);
        }
        for (; p < 4; p++) {
            SetControllerActive(p, 0);
            if (p > 1) {
            }
        }
    } else if (gGameState & 0x80) {
        loading_effect_none();
        LoadWave(60, 0, 0, 0, 0);
        SetControllerActive(-1, 0);
        SetControllerActive(0, 1);
        loading_effect_none();
    } else if (gGameState & 0x40000) {
        if (!gRestartGame) {
            UnloadFrontEndWaves();
            HAL_PurgeFonts();
            InitWaves();
        }
        LoadFrontEndOverlay(0);
        if (!gRestartGame) {
            if (!expansion_pak_present && gRace != 6 && gRace != 4 && gRace != 5) {
                AudioInit(8, 8, 0, 0, 0, 1.0f, 200.0f, 0);
            }
            if (!expansion_pak_present && gRace == 6) {
                AudInit3D(5, 64, 1.0f, 200.0f, 0);
            }
            SaveFlush(1);
        }
        if (gRace == 6 || gRace == 4 || gRace == 5 || expansion_pak_present) {
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
    } else if (gGameState & 0x1000000) {
        SetControllerStickEdges(-1, 1);
    } else if (gGameState & 0x800000) {
        SaveServer(1, 0);
        SaveFlush(1);
    } else if (gGameState & 0x20000000) {
        UnloadFrontEndWaves();
        InitWaves();
        LoadWave(55, 0, 0, 0, 0);
    }
    if (gGameState & 0x7c03fffe) {
        ZOID_SetFog(1000, 999, gFogColors[19][0], gFogColors[19][1], gFogColors[19][2], 0xff, 0);
        ZOID_SetFog(1000, 999, gFogColors[19][0], gFogColors[19][1], gFogColors[19][2], 0xff, 1);
    }
}

void updateGameState(void)        /* updateGameState */
{
    if (gNewGameState != gGameState) {
        uninitGameState();
        gGameState = gNewGameState;
        initGameState();
    }
}

void loading_effect_none(void)
{
}
