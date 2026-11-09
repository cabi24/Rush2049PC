/* UpdateGame and EndGame: a race from its set-up to the high score screen, one turn of the game loop at a time
 * (MAT3 game.c). */
#include "game/game.h"

/* A player's input record (gPlayers): its car slot and its crashes. */
typedef struct Player76 { u8 slot; u8 pad01[63]; u16 numCrashes; u8 pad42[0x48 - 0x42]; void *ref; } Player76;

/* A ghost's save header (GhostSaveHeader): the flag that says it is a ghost to race against. */
typedef struct Inner5 { u8 pad0[5]; s8 flag; } Inner5;
/* A ghost (SaveInit' Ghost): link, file and data handles before its header, pointer-sized on the PC
 * (docs/port/CASTS.md). */
typedef struct Ghost40 { void *ll; void **fileh; u8 pad8[0x24 - 8]; void *datah; Inner5 **header; } Ghost40;

extern Player76 gPlayers[];   /* gPlayers */

VIEW(s8[4], gTeamPoints);
#define gTeamPoints gTeamPoints_view

VIEW(Ghost40 **[], gGhosts);
#define gGhosts gGhosts_view

VIEW(tCamera[], gCamera);
#define gCamera gCamera_view

s32 CountdownTimeOut(void);

void *memset(void *, s32, u32);

/* One turn of a race, by game state: SETUP (the options, the pole positions, the catch-up) goes on to LOAD
 * (PrePlay until it is done, then the countdown of 3.5 seconds starts the model thread's clock); COUNTDOWN moves
 * the cars to the start and counts down (CountDown ends it); START sets the race's clock (practice 30 minutes,
 * stunt and battle by their modes, the obstacle course 5, else CalcRaceTime), plays the start and the music; PLAY
 * runs the race (PlayGame) and decides its end: retiring, or once coasting all the cars stopped (all humans solid);
 * stunt by time, points or crashes; battle by time or a team's points ("game point" one short); a lap race when
 * every human has finished; any other when the clock runs out (the "time's up" call for a human short of the
 * laps). At the end the race END (EndGame), then the high score and game over screens. In the countdown and the
 * race, every window's fog follows its camera. */
void UpdateGame(void)
{
    s32 i;
    s8 allSolid;
    s32 p;
    s32 numDeaths;
    static s8 startCountdown = 0;

    if (gGameState & GS_RACE_SETUP) {
        gSkipToGameState = 1;
        SetOptions();
        CalcPolePositions();
        gUseCatchup = gCatchup;
        setGameState(GS_RACE_LOAD);
        goto load;
    } else if (gGameState & GS_RACE_LOAD) {
    load:
        if (PrePlay(1, 0)) {
            startCountdown = 1;
            setGameState(GS_RACE_COUNTDOWN);
            ClearRushControls();
            goto countdown;
        } else {
            SaveServer(0, 1);
            goto done;
        }
    } else if (gGameState & GS_RACE_COUNTDOWN) {
    countdown:
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
    } else if (gGameState & GS_RACE_START) {
        if (gRace == kPractice) SetCountdownTimer(1800.0f);
        else if (gRace == kStunt) {
            if (gStuntMode == 0) SetCountdownTimer(gStuntModeMins * 60);
            else SetCountdownTimer(600.0f);
        } else if (gRace == kObstacle) SetCountdownTimer(300.0f);
        else if (gRace == kBattle) {
            if (gBattleMode == 1) SetCountdownTimer(gBattleModeMins * 60);
            else SetCountdownTimer(1200.0f);
        } else SetCountdownTimer(CalcRaceTime());
        PlayGameSound(78, 0, 1, 0);
        StartGameMusic();
        InitCPTime();
        setGameState(GS_RACE_PLAY);
        save_server_tick_time = gModelTime;
    } else if (gGameState & GS_RACE_PLAY) {
        if (save_server_tick_time + 3.0f < gModelTime) {
            SaveServer(0, 1);
            save_server_tick_time = gModelTime;
        }
        PlayGame();
        CheckButtons();
        if (gRetireGame) {
            FindWinner();
            setGameState(GS_RACE_END);
            GhostEnd();
            RemoveHUD();
        } else if (gCoastFlag) {
            allSolid = 1;
            for (p = 0; p < gNumPlayers; p++) {
                if (gDeathMode) continue;
                if (!AreWeSolid(p)) allSolid = 0;
            }
            if (are_all_cars_stopped() && allSolid) {
                if (gRace == kStunt) FindWinner();
                setGameState(GS_RACE_END);
                GhostEnd();
                RemoveHUD();
            }
            if (!CountdownTimeOut() && gRace != kStunt && gRace != kObstacle && gRace != kBattle) {
                gCoastFlag = 0;
                gOutOfTime = 0;
            }
        } else if (gRace == kStunt) {
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
        } else if (gRace == kBattle) {
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
        } else if (gRace == kOneRace || gRace == kPractice || gRace == kCircuit) {
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
                        if (gRace != kGhost || !gGhosts[gPlayers[i].slot] || (*gGhosts[gPlayers[i].slot])->header[0]->flag < 0)
                            PlayGameSound(22, 0, 1, 0);
                        gOutOfTime = 1;
                        break;
                    }
                }
            }
        }
    } else if (gGameState & GS_RACE_END) {
        EndGame();
    } else if (gGameState & GS_HIGH_SCORE) updateHighScoreScreen();
    else if (gGameState & GS_GAME_OVER) updateGameOverScreen();
done:
    if (gGameState & (GS_RACE_COUNTDOWN | GS_RACE_PLAY)) {
        for (i = 0; i < gNumWindows; i++) WorldFogDensity(i, gCamera[i].mat.pos);
    }
}

/* Ends the race: every car's model stops (End_MDrive), its flag 8 cleared, the model thread and the listeners
 * stop, and the high score screen follows in 4 seconds. */
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
    setGameState(GS_HIGH_SCORE);
}
