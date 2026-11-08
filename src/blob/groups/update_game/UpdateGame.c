/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* UpdateGame (0x800FBF88, 668 words) = MAT3 game.c _UpdateGame (GC 0x80014340, size 0x9E0), the per-frame
 * race state machine called by GameLoop (= GameLoop) when gGameState & 0x3FC0000. The N64 label is WRONG
 * (proposed name: _UpdateGame). Direct typed port of the MAT3 body (statement order, gotos preplay/UpdateGame,
 * locals i/allSolid/p/numDeaths, static startCountdown, static _EndGame defined after it).
 * N64 differences from GC: no gPakInserted loop; no SwitchFrameMode/DisableDrawFace/EnableDrawFace/
 * UpdateStationaryEmitters; SaveServer(0, 1) after the countdown frame and a 3-second SaveServer tick
 * (save_server_tick_time vs game time gModelTime) at the start of the race state; gRetireGame heads the race-state
 * else-if chain; kCircuit (3) joins the one-race/practice branch; N64 sound ids 78/2/22; the second battle
 * loop indexes with i (GC: p).
 * Callee map (N64 -> GC): .
 *
 * Matching levers (all natural; no unused locals, no compiled-out code):
 *  - setGameState is the kept 1-statement setGameState (as in GC). umerge inlines its 7 calls; they use up
 *    UpdateGame's inline budget, so PlayGameSound (PlayGameSound) and _CheckButtons (CheckButtons) stay
 *    `jal` calls as in retail with no inline blockers (writing `gNewGameState = X` inlines one
 *    PlayGameSound and _CheckButtons).
 *  - startCountdown is a function-local `static s8 = 0` (.data 0x801146F0); as an extern its address web is
 *    coloured into s0.
 *  - _EndGame is a static helper defined after UpdateGame (umerge inlines it; its later line numbers give as1
 *    retail's store-before-reload order at the end of the 0x1000000 state).
 *  - SetCountdownTimer(3.5) and `3.5f * gRetraceRate`: two spellings, two constants (retail rematerialises
 *    3.5 after the call; one spelling keeps it in $f20).
 *  - coast loop `if (gDeathMode) continue;` (retail reloads gNumPlayers only on the AreWeSolid path; the
 *    `&&` form reloads at the join and colours &gNumPlayers into s3).
 *  - the two-store blocks (gCoastFlag = 1; gOutOfTime = 1;) on separate lines in stunt case 2 and battle
 *    case 1 (as1 store order).
 * Needs to be INTERNAL (not kept): retail uses s0-s6/$f20/$f22 unsaved (IPA; GameLoop saves them).
 */
typedef unsigned char u8;
typedef signed char s8;
typedef short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct Player76 { u8 slot; u8 pad01[63]; u16 numCrashes; u8 pad42[0x48 - 0x42]; void *ref; } Player76;
typedef struct Car952 {
    u8 pad000[232];
    s32 appearance;         /* 0x0E8 */
    u8 pad0EC[3];
    s8 place_locked;        /* 0x0EF */
    u8 pad0F0[0x110 - 0xF0]; struct pc_visual visuals[21]; u8 pad0F0_308[0x359 - 0x308];
    s8 dead;                /* 0x359 */
    u8 pad35A;
    s8 slot;                /* 0x35B */
    u8 pad35C[896 - 860];
    Player76 *player;       /* 0x380 */
    u8 pad384[931 - 900];
    s8 battlePoints;        /* 0x3A3 */
    u8 pad3A4[952 - 932];
} Car952;
typedef struct Model2056 {
    u8 pad000[1990];
    s16 slot;               /* 0x7C6 */
    u8 pad7C8[2024 - 1992];
    s8 laps;                /* 0x7E8 */
    u8 pad7E9[2056 - 2025];
} Model2056;
typedef struct Stunt120 { u8 pad00[20]; s32 totalPoints; u8 pad18[96]; } Stunt120;
typedef struct Cam152 { u8 pad00[36]; f32 pos[3]; u8 pad30[104]; } Cam152;
typedef struct Inner5 { u8 pad0[5]; s8 flag; } Inner5;
/* A ghost (SaveInit' Ghost): link, file and data handles before its header, pointer-sized on the PC
 * (docs/port/CASTS.md). */
typedef struct Ghost40 { void *ll; void **fileh; u8 pad8[0x24 - 8]; void *datah; Inner5 **header; } Ghost40;

extern u32 gGameState;        /* gGameState */
extern u32 gNewGameState;        /* gNewGameState */
extern u32 gSkipToGameState;          /* gSkipToGameState */

extern s8 gRestartGame;           /* gRestartGame */
extern s8 gGamePointPlayed;           /* gGamePointPlayed */
extern s32 gRace;       /* gRace */
extern s16 gNumPlayers; /* gNumPlayers */
extern Player76 gPlayers[];   /* gPlayers */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s8 gUseCatchup;           /* gUseCatchup */
extern s8 gCatchup;           /* gCatchup */
extern s32 gStuntMode;          /* gStuntMode */
extern s32 gStuntModeMins;          /* gStuntModeMins */
extern s32 gBattleMode;          /* gBattleMode */
extern s32 gBattleModeMins;          /* gBattleModeMins */
extern s32 gStuntModePoints;          /* gStuntModePoints */
extern s32 gStuntModeDeaths;          /* gStuntModeDeaths */
extern s32 gBattleModePoints;          /* gBattleModePoints */
extern f32 save_server_tick_time;          /* last SaveServer tick time */
extern f32 gModelTime;          /* game time */
extern s8 gCoastFlag;           /* gCoastFlag */
extern s8 gRetireGame;           /* gRetireGame */
extern s8 gOutOfTime;           /* gOutOfTime */
extern s8 gDeathMode;           /* gDeathMode */
extern Stunt120 gStuntPlayer[];   /* gStuntPlayer */
extern s8 gTeamPoints[4];        /* gTeamPoints */
extern s8 gCarTeam[];         /* gCarTeam */
extern s32 gAPlayerReallyFinished;          /* gAPlayerReallyFinished */
extern f32 gDeathWaitEnd;          /* gDeathWaitEnd */
extern s16 gNumLaps;          /* gNumLaps */
extern Ghost40 **gGhosts[];  /* gGhosts */
extern s8 gNumActiveCars;           /* gNumActiveCars */
extern s16 gNumWindows;
extern Cam152 gCamera[];
extern f32 gRetraceRate;          /* gRetraceRate */

void SetOptions(void);            /* SetOptions */
void CalcPolePositions(void);         /* _CalcPolePositions */
s32 PrePlay(s32 useHud, s32 cancel); /* PrePlay */
void ClearRushControls(void);         /* ClearRushControls */
void SaveServer(s32 forceUpdate, s32 rumbleOnly);
void CarSetup(void);           /* CarSetup */
void Update_MDrive(void);            /* Update_MDrive */
void TextureAnimation(void);                /* TextureAnimation */
void CheckTargetCollision(s16 slot);            /* CheckTargetCollision */
void SetCountdownTimer(f32 time);          /* SetCountdownTimer */
s32 CountdownTimeOut(void);             /* CountdownTimeOut */
void init_model_task(s32 ticks);          /* init_model_task */
void CountDown(void);               /* CountDown */
void CheckButtons(void);               /* _CheckButtons */
f32 CalcRaceTime(void);                /* CalcRaceTime */
s32 PlayGameSound(s32 sound, s32 slot, s32 a2, u8 a3); /* PlayGameSound */
void StartGameMusic(void);          /* StartGameMusic */
void InitCPTime(void);               /* InitCPTime */
void PlayGame(void);        /* PlayGame */
void FindWinner(void);               /* FindWinner */
void GhostEnd(void);            /* GhostEnd */
void RemoveHUD(void);              /* RemoveHUD */
s32 AreWeSolid(s16 p);               /* AreWeSolid */
s32 are_all_cars_stopped(void);                /* are_all_cars_stopped */
void AudSetMusicVol(f32 vol, f32 time); /* AudSetMusicVol */
void *memset(void *, s32, u32);
void End_MDrive(s16 slot);           /* End_MDrive */
void UpdateVisuals(s16 slot);               /* UpdateVisuals */
void modelstop(void);        /* modelstop */
void StopListeners(void);               /* StopListeners */
void updateHighScoreScreen(void);            /* updateHighScoreScreen */
void updateGameOverScreen(void);            /* updateGameOverScreen */
void WorldFogDensity(s32 i, f32 *pos);    /* WorldFogDensity */
void setGameState(u32 state);          /* setGameState */

void EndGame(void);  /* GC _EndGame: names the static after its retail deleted-procedure stub */

void UpdateGame(void)
{
    s32 i;
    s8 allSolid;
    s32 p;
    s32 numDeaths;
    static s8 startCountdown = 0;

    if (gGameState & 0x40000) {
        gSkipToGameState = 1;
        SetOptions();
        CalcPolePositions();
        gUseCatchup = gCatchup;
        setGameState(0x80000);
        goto preplay;
    } else if (gGameState & 0x80000) {
    preplay:
        if (PrePlay(1, 0)) {
            startCountdown = 1;
            setGameState(0x200000);
            ClearRushControls();
            goto UpdateGame;
        } else {
            SaveServer(0, 1);
            goto done;
        }
    } else if (gGameState & 0x200000) {
    UpdateGame:
        if (gRestartGame) goto done;
        CarSetup();
        Update_MDrive();
        TextureAnimation();
        CheckTargetCollision(PC_MODEL_SLOT(0));
        if (startCountdown) {
            SetCountdownTimer(3.5);
            init_model_task(3.5f * gRetraceRate);
            startCountdown = 0;
        }
        CountDown();
        CheckButtons();
        SaveServer(0, 1);
    } else if (gGameState & 0x100000) {
        if (gRace == 1) SetCountdownTimer(1800.0f);
        else if (gRace == 4) {
            if (gStuntMode == 0) SetCountdownTimer(gStuntModeMins * 60);
            else SetCountdownTimer(600.0f);
        } else if (gRace == 5) SetCountdownTimer(300.0f);
        else if (gRace == 6) {
            if (gBattleMode == 1) SetCountdownTimer(gBattleModeMins * 60);
            else SetCountdownTimer(1200.0f);
        } else SetCountdownTimer(CalcRaceTime());
        PlayGameSound(78, 0, 1, 0);
        StartGameMusic();
        InitCPTime();
        setGameState(0x400000);
        save_server_tick_time = gModelTime;
    } else if (gGameState & 0x400000) {
        if (save_server_tick_time + 3.0f < gModelTime) {
            SaveServer(0, 1);
            save_server_tick_time = gModelTime;
        }
        PlayGame();
        CheckButtons();
        if (gRetireGame) {
            FindWinner();
            setGameState(0x1000000);
            GhostEnd();
            RemoveHUD();
        } else if (gCoastFlag) {
            allSolid = 1;
            for (p = 0; p < gNumPlayers; p++) {
                if (gDeathMode) continue;
                if (!AreWeSolid(p)) allSolid = 0;
            }
            if (are_all_cars_stopped() && allSolid) {
                if (gRace == 4) FindWinner();
                setGameState(0x1000000);
                GhostEnd();
                RemoveHUD();
            }
            if (!CountdownTimeOut() && gRace != 4 && gRace != 5 && gRace != 6) {
                gCoastFlag = 0;
                gOutOfTime = 0;
            }
        } else if (gRace == 4) {
            switch (gStuntMode) {
            case 0:
                if (CountdownTimeOut()) { gCoastFlag = 1; gOutOfTime = 1; }
                break;
            case 1:
                for (i = 0; i < gNumPlayers; i++) {
                    if (gStuntPlayer[i].totalPoints >= gStuntModePoints) { gCoastFlag = 1; gOutOfTime = 1; }
                }
                break;
            case 2:
                numDeaths = 0;
                for (i = 0; i < gNumPlayers; i++) {
                    if (PC_CAR_INPUT_AS(Player76, i)->numCrashes >= gStuntModeDeaths) { PC_CAR_STATE(i) = 1; numDeaths++; }
                }
                if (numDeaths == gNumPlayers) {
                    gCoastFlag = 1;
                    gOutOfTime = 1;
                }
                break;
            }
        } else if (gRace == 6) {
            switch (gBattleMode) {
            case 1:
                if (CountdownTimeOut()) {
                    FindWinner();
                    gCoastFlag = 1;
                    gOutOfTime = 1;
                }
                break;
            case 0:
                memset(gTeamPoints, 0, 4);
                for (i = 0; i < gNumPlayers; i++) gTeamPoints[gCarTeam[i]] += PC_CAR_POINTS(i);
                for (i = 0; i < 4; i++) {
                    if (!gGamePointPlayed && gTeamPoints[i] == gBattleModePoints - 1) {
                        gGamePointPlayed = 1;
                        PlayGameSound(2, 0, 2, 0);
                    }
                    if (gTeamPoints[i] >= gBattleModePoints) {
                        FindWinner(); gCoastFlag = 1; gOutOfTime = 1;
                    }
                }
                break;
            }
        } else if (gRace == 0 || gRace == 1 || gRace == 3) {
            for (i = 0; i < gNumPlayers; i++) if (!PC_CAR_FINISHED(gPlayers[i].slot)) goto done;
            gAPlayerReallyFinished = 0;
            for (i = 0; i < gNumPlayers; i++) if (gNumLaps == PC_MODEL_LAP(i)) gAPlayerReallyFinished = 1;
            if (!gCoastFlag) {
                if (gDeathMode) gDeathWaitEnd = 0.0f;
                gCoastFlag = 1;
                FindWinner();
                AudSetMusicVol(0.0f, 3.0f);
            }
        } else {
            if (CountdownTimeOut()) {
                if (!gCoastFlag) {
                    gCoastFlag = 1;
                    FindWinner();
                    AudSetMusicVol(0.0f, 3.0f);
                }
                for (i = 0; i < gNumPlayers; i++) {
                    if (gNumLaps != PC_MODEL_LAP(gPlayers[i].slot)) {
                        if (gRace != 2 || !gGhosts[gPlayers[i].slot] || (*gGhosts[gPlayers[i].slot])->header[0]->flag < 0)
                            PlayGameSound(22, 0, 1, 0);
                        gOutOfTime = 1;
                        break;
                    }
                }
            }
        }
    } else if (gGameState & 0x1000000) {
        EndGame();
    } else if (gGameState & 0x2000000) updateHighScoreScreen();
    else if (gGameState & 0x800000) updateGameOverScreen();
done:
    if (gGameState & 0x600000) {
        for (i = 0; i < gNumWindows; i++) WorldFogDensity(i, gCamera[i].pos);
    }
}

void EndGame(void)
{
    s16 i;
    for (i = 0; i < gNumActiveCars; i++) {
        End_MDrive(PC_MODEL_SLOT(i));
        PC_CAR_FLAGS(i) &= ~8;
        UpdateVisuals(PC_CAR_SLOT(i));
    }
    modelstop();
    StopListeners();
    SetCountdownTimer(4.0f);
    setGameState(0x2000000);
}
