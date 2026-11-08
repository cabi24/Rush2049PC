/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * state.c (N64): MAT3 state.c / Dreamcast state.c with the N64 additions read from retail.
 *
 * updateGameState (0x800CA3B4, 2,544 B) = updateGameState@state.c with initGameState (initGameState, umerge-
 *   inlined) and setGameState (setGameState) inlined. Base text: the Dreamcast initGameState (it keeps the N64 13-font
 *   preload in state 0x40000 and SaveFlush(1)); N64 additions: the AudioInit block (AudioInit + AudFree3D) after
 *   InitWaves, AudSetFxVol(gOptions[13] / 10, 0.1) before LoadFrontEndOverlay(1) (AudSetFxVol, inlined),
 * N64 /60/55, the gPlayers PurgePlayerData loop (PurgePlayerData) in state 0x8, the front-end music
 *   block (AudSetMusicVol + AudPlaySong(6, 1)) at the end of state 0x4, the N64 audio re-init in state 0x40000
 *   (AudioInit(8, 8, ...) / AudInit3D for gRace 6), load_extra_overlay (overlay "Extra") and ZOID_SetFog(1000, 999).
 * LoadFrontEndOverlay (0x800C8FA4, 316 B) = LoadFrontEndOverlay@state.c (GC: empty stub "called with an argument"):
 *   load: unload overlay "Extra" (flag extra_overlay_loaded), load overlay "Start" (load_start_overlay, inlined:
 *   load_overlay with its IPA s0-s4 register parameters); unload: unload "Start" (flag start_overlay_loaded). Its flag `loaded`
 *   is MAT3's function-local static (retail 0x801174C0 = state.c .data after gGameState/gNewGameState/
 *   gSkipToGameState, value 0); as a file-level or extern variable uopt colours its address web into s5 and the
 *   caller then saves s5 (retail saves s0-s4 only).
 *   CONTEXT ONLY (not code-identical): retail has no address webs for extra_overlay_loaded / start_overlay_loaded either; ours colours
 * them into s3 (inside the s0-s4 clobber set, so the caller is unaffected). finding: the webs vanish when the
 * two overlay flags are procedure-local statics (micro tests), i.e. retail's overlay flags are
 *   most likely function-local statics of a deleted overlay helper (stub overlay_stub before unload_extra_overlay) that
 * is inlined into the overlay_loader.c wrappers and here. See.
 *
 * Shaping and disclosed choices (updateGameState):
 * - umerge's knapsack (-instrumented umerge) inlines a multi-caller callee at a
 *    site when size_incr * max(1, C >> 7) / benefit <= 3.0, C = the caller's size measure (+28). HAL_SetFont in the
 *    MAT3 two-level form (halfont.c) has size_incr 9, benefit 5: the font sites are visited from the last one, the
 *    first three (fonts 7, 6, 5) are inlined while C < 256, the fourth pushes C to 257 (factor 2) and every later
 *    candidate with cost > 1.5 (fonts 8..13, load_extra_overlay, LoadFrontEndOverlay) stays a jal, as in retail. This needs
 *    C = 232..239 before the font sites: the four calls of the empty N64 helper loading_effect_none (umerge inlines them
 *    in its first pass, each adding 2 to C: 225 -> 233) do that, and their four 8-byte umerge inline reserves are the
 *    retail frame's 32 bytes between the font homes (132/108/84) and `i` (220): frame 232 (without them: 200 and four
 * fonts inlined). loading_effect_none's retail stub sits right before SetControllerActive (post-order layout), so its
 *    last call site is at the end of state 0x80; the four positions used here (before and after LoadWave in 0x4000 and
 *    0x80, MAT3's loading-effect slots) are not otherwise determined by the bytes: any four calls before the font
 *    block give the same code and frame.
 *  - initGameState is initGameState: written non-static (not kept) so umerge's deleted-procedure stub keeps the symbol;
 *    its retail stub lies where the post-order layout puts initGameState itself (after its last new callee
 *    SetOptions, before uninitGameState = uninitGameState).
 *  - the two compiled-out `if (p > 1) {}` are the Dreamcast text's ("target retains empty per-controller check").
 */
typedef unsigned char u8;
typedef signed char s8;
typedef short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct PSFile { void *next, *prev; void *fileh; } PSFile;     /* the profile's host layout (pc_prelude.h) */
typedef struct PadState { u8 pad0[72]; PSFile **save; } PadState;    /* gPlayers[], 76 bytes, save at +72 */
typedef struct PlayerData { struct PlayerData **next; } PlayerData;  /* handle list, next handle at +0 */
typedef struct { u32 opaque[6]; } OSMesgQueue;
s32 osRecvMesg(OSMesgQueue *mq, void **msg, s32 flag);
s32 osSendMesg(OSMesgQueue *mq, void *msg, s32 flag);

extern u32 gGameState;                /* gGameState */
extern u32 gNewGameState;                /* gNewGameState */
extern u32 gSkipToGameState;                  /* gSkipToGameState */
extern s8 gGameStarted, gSofdecShown, gLicenseShown, gIntroMovieShown, pfsInited, lcdInited; /* state-1 flags */
extern s8 expansion_pak_present;
extern s8 gOptions[];                 /* gOptions: [12] music volume, [13] sfx volume, [14] music track */
extern s16 gNumPlayers;         /* gNumPlayers */
extern s16 gNumWindows;
extern PadState gPlayers[];           /* gPlayers */
extern s8 gUpsideDownTrack;                   /* gUpsideDownTrack */
extern s8 gRestartGame;                   /* gRestartGame */
extern s32 gRace;               /* gRace */
extern PlayerData **gPlayerFiles_head;
extern u8 gFogColors[][3];       /* gFogColors */
extern s8 start_overlay_loaded;                   /* overlay "Start" (image A) loaded */
extern s8 extra_overlay_loaded;                   /* overlay "Extra" (image B) loaded */
extern u8 overlay_base[];                 /* runtime image base (A and B) */
extern OSMesgQueue heap_lock_queue;          /* heap semaphore */

void uninitGameState(void);     /* uninitGameState */
void InitWaves(void);           /* InitWaves */
void AudioInit(u8 a0, u8 a1, u8 a2, s32 numA, s32 numB, f32 a5, f32 a6, s32 a7); /* AudioInit */
void AudFree3D(void);
void SetOptions(void);            /* SetOptions */
void SetCountdownTimer(f32 t);             /* SetCountdownTimer */
void UpdateFrame(void);
void InitRecords(void);               /* InitRecords */
void setGameState(s32 state);          /* setGameState */
void HAL_PurgeFonts(void);        /* _UnloadFontWaves */
void UnloadFrontEndWaves(void);         /* _UnloadFrontEndWaves */
void LoadFrontEndWaves(void); /* _LoadFrontEndWaves */
void AudSetMusicVol(f32 vol, f32 time); /* AudSetMusicVol */
void AudSetFxVol(f32 vol, f32 time); /* AudSetFxVol */
void SetControllerActive(s32 p, s32 on);   /* SetControllerActive */
void SetControllerStickEdges(s32 p, s32 on);    /* SetControllerStickEdges */
void PS_LockFile(void *fileh, s32 lock); /* PS_LockFile */
void AudPlaySong(s32 song, s32 a1); /* AudPlaySong */
void PurgePlayerData(PlayerData **h); /* PurgePlayerData */
void load_extra_overlay(void);                 /* load overlay "Extra" */
void load_start_overlay(void);  /* load overlay "Start" */
s32 LoadWave(s32 wave, s32 a1, s32 a2, s32 a3, void *a4); /* LoadWave */
void SaveFlush(s32 a);      /* SaveFlush */
void AudInit3D(s32 numA, s32 numB, f32 a2, f32 a3, s32 a4);
s32 HAL_SetFont(s32 fontID);          /* HAL_SetFont (halfont.c) */
void SaveServer(s32 forceUpdate, s32 rumbleOnly);
void *ZOID_SetFog(s16 a0, s16 a1, u8 r, u8 g, u8 b, u8 a, u32 win); /* ZOID_SetFog */
void DeallocateMemoryInternal(u32 address, s32 tag); /* _DeallocateMemory (internal, IPA a1/a2) */
void loading_effect_none(void);               /* empty N64 helper, deleted after inlining (stub 0x800C9150) */
void initGameState(void);               /* initGameState */

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
        for (h = gPlayerFiles_head; h; h = (*h)->next) {
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
