#include "game/game.h"

typedef struct Player76 { u8 slot; u8 pad01[63]; s16 numCrashes; u8 pad42[0x48 - 0x42]; void *ref; } Player76;


extern Player76 gPlayers[];   /* gPlayers */

/* game.frame_rate = 60 (docs/port/FRAMERATE.md 7.3): the track's load goes a wave a call, a call a frame, so at 60 it
 * is called on the frames that complete a 30 Hz frame: the countdown begins within a retrace of original's. */
#define PC_LOAD_ON30(call) ((!pc_fps60 || pc_frame_steps) ? (call) : 0)

void *memset(void *, s32, u32);

void leStartLoadingEffect(void)
{
}

void leEndLoadingEffect(void)
{
}

static void ResetStunts(void)
{
    s32 playerIdx;
    for (playerIdx = 0; playerIdx < 4; playerIdx++) {
        memset(&gStuntInfo[playerIdx], 0, sizeof(StuntInfo));
        gStuntInfo[playerIdx].active |= 0x15000000;
    }
}

s32 PrePlay(s32 useHud, s32 cancel)
{
    static s8 inited = 0;
    s32 i, slot, rrr;

    if (useHud) {}
    if (cancel) { inited = 0; return 0; }
    if (!inited) {
        inited = 1;
        SetOptions();
        EnableGameSounds(!(gGameState & GS_DEMO) && !(gGameState & GS_HIGH_SCORE));
        if (gGameState & GS_HIGH_SCORE) {
            num_our_drones = 0;
            *(volatile s16 *)&num_drones = num_our_drones;
            *(volatile s16 *)&num_humans = num_our_drones;
            for (i = 0; i < gNumPlayers; i++) {
                slot = gPlayers[i].slot;
                PC_LINK_OWNER(slot) -= 6;
                PC_CAR_DEAD(slot) = 0;
                PC_CAR_PLACE_LOCKED(slot) = 0;
                PC_CAR_INPUT(slot) = 0;
                PC_MODEL_DRONE_TYPE(slot) = 1;
                PC_MODEL_WE_CONTROL(slot) = 1;
                PC_MODEL_AUTOTRANS(slot) = 1;
                PC_MODEL_FLAGS(slot) = 0;
                PC_CAR_FLAGS(slot) = 0;
                SetupCar(slot, PC_MODEL_BODY(slot), 1);
                PC_DRONE(num_drones++) = i;
                PC_OUR_DRONE(num_our_drones++) = i;
            }
            for (; i < gNumActiveCars; i++) {
                PC_CAR_DEAD(i) = 0;
                PC_CAR_PLACE_LOCKED(i) = 0;
                PC_CAR_FLAGS(i) = 0;
                PC_MODEL_FLAGS(i) = 0;
                SetupCar(i, PC_MODEL_BODY(i), 1);
                PC_DRONE(num_drones++) = i;
                PC_OUR_DRONE(num_our_drones++) = i;
            }
            new_sched_to_use = sched_to_use = num_our_drones;
            sched_index = -1;
            cur_drone = 0;
            CalcPolePositions();
            InitCPS();
            InitDrones();
            gCoastFlag = 0;
            gRetireGame = 0;
            gGravity = 32.2f;
            for (i = 0; i < gNumActiveCars; i++) Init_MDrive(0, PC_MODEL_SLOT(i));
            ShowHUD();
            inited = 0;
            return 1;
        }
        HFOV = 90.0f;
        if (!(gGameState & GS_DEMO)) AudKillSong();
        if (gRestartGame) { ZOID_Reset(); InitBlits(); }
        else if (!(gGameState & GS_DEMO)) InitWaves();
        munge_gLink_data();
        GhostStart();
        ResetHighScores();
    }
    if (gFogColor < 19) gFogColor = gTrackNum;
    if (!(gGameState & GS_DEMO)) {
        HFOV = 90.0f;
        ZOID_ResetWindows();
        gNumWindows = gRace == kGhost ? (s16)1 : gNumPlayers;
        ZOID_SplitMenuScreen(gNumWindows);
    }
    if (gRestartGame) ReloadTrack();
    else {
        leStartLoadingEffect();
        rrr = PC_LOAD_ON30(LoadTrack(gTrackNum, 1, 0));
        leEndLoadingEffect();
        if (!rrr) return 0;
    }
    InitTextureAnimation();
    gRestartGame = 0;
    for (i = 0; i < gNumWindows; i++) {
        ZOID_SetFog(1000, 999, gFogColors[gFogColor][0], gFogColors[gFogColor][1], gFogColors[gFogColor][2], 255, i);
        WorldFogDensity(i, gCamera[i].mat.pos);
    }
    if (!(gGameState & GS_DEMO)) AudStopAllEffects();
    InitCPS();
    init_cars();
    for (i = 0; i < PC_MAX_LINKS; i++) PC_CARSETUP_FLAG(i) = 0;
    InitDrones();
    gCoastFlag = 0; gRetireGame = 0; gOutOfTime = 0;
    gGravity = 32.2f * gravityArray[gGravityScale];
    for (i = 0; i < gNumActiveCars; i++) {
        slot = PC_MODEL_SLOT(i);
        if (PC_LINK_OWNER(slot) == 0 || PC_LINK_OWNER(slot) == 6) Init_MDrive(0, slot);
    }
    StartEnvironment();
    init_views();
    if (useHud) ShowHUD();
    for (i = 0; i < gNumPlayers; i++) {
        gPlayers[i].numCrashes = 0;
        PC_CAR_HEALTH(i) = 800;
        PC_CAR_POINTS(i) = 0;
        PC_CAR_RELOADING_TIME(i) = 0.0f;
        if (gRace == kBattle) InitBattlePlayer(i);
        gGamePointPlayed = 0;
    }
    if (!(gGameState & GS_DEMO)) ClearRushControls();
    StartListeners();
    if (gRace == kBattle) { InitBattlePoints(); ShotInit(); }
    if (gRace == kStunt) InitStunt();
    if (!(gGameState & GS_DEMO)) PS_RumbleGo();
    gUpsideDownTrack = gCheatState[3];
    inited = 0;
    return 1;
}
