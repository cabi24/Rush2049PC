#include "game/game.h"

typedef struct Player76 { u8 slot; u8 pad01[63]; u16 numCrashes; u8 pad42[0x48 - 0x42]; void *ref; } Player76;

typedef struct Stunt120 { u8 pad00[20]; s32 totalPoints; u8 pad18[96]; } Stunt120;
typedef struct Cam152 { u8 pad00[36]; f32 pos[3]; u8 pad30[104]; } Cam152;
typedef struct Inner5 { u8 pad0[5]; s8 flag; } Inner5;
/* A ghost (SaveInit' Ghost): link, file and data handles before its header, pointer-sized on the PC
 * (docs/port/CASTS.md). */
typedef struct Ghost40 { void *ll; void **fileh; u8 pad8[0x24 - 8]; void *datah; Inner5 **header; } Ghost40;

extern Player76 gPlayers[];   /* gPlayers */

VIEW(Stunt120[], gStuntPlayer);
#define gStuntPlayer gStuntPlayer_view
VIEW(s8[4], gTeamPoints);
#define gTeamPoints gTeamPoints_view

VIEW(Ghost40 **[], gGhosts);
#define gGhosts gGhosts_view

VIEW(Cam152[], gCamera);
#define gCamera gCamera_view

s32 CountdownTimeOut(void);

void *memset(void *, s32, u32);

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
