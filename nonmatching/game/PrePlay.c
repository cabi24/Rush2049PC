#include "game/game.h"

typedef struct Player76 { u8 slot; u8 pad01[63]; s16 numCrashes; u8 pad42[0x48 - 0x42]; void *ref; } Player76;

typedef struct Cam152 { u8 pad00[36]; f32 pos[3]; u8 pad30[104]; } Cam152;

extern Player76 gPlayers[];   /* gPlayers */

VIEW(s16, num_drones);
#define num_drones num_drones_view
VIEW(s16, num_humans);
#define num_humans num_humans_view

VIEW(Cam152[], gCamera);
#define gCamera gCamera_view
VIEW(u8[][3], gFogColors);
#define gFogColors gFogColors_view
/* game.frame_rate = 60 (docs/port/FRAMERATE.md 7.3): the track's load goes a wave a call, a call a frame, so at 60 it
 * is called on the frames that complete a 30 Hz frame: the countdown begins within a retrace of original's. */
#define PC_LOAD_ON30(call) ((!pc_fps60 || pc_frame_steps) ? (call) : 0)

void ZOID_SetFog(s32 a0, s32 a1, s32 r, s32 g, s32 b, s32 a, s32 win);

void *memset(void *, s32, u32);

void leStartLoadingEffect(void)
{
}

void leEndLoadingEffect(void)
{
}

VIEW(StuntInfo[4], gStuntInfo);
#define gStuntInfo gStuntInfo_view

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
        EnableGameSounds(!(gGameState & 8) && !(gGameState & 0x2000000));
        if (gGameState & 0x2000000) {
            num_our_drones = 0;
            *(volatile s16 *)&num_drones = num_our_drones;
            *(volatile s16 *)&num_humans = num_our_drones;
            for (i = 0; i < gNumPlayers; i++) {
                slot = gPlayers[i].slot;
                PC_LINK_OWNER(slot) -= 6;
                PC_CAR_STATE(slot) = 0;
                PC_CAR_FINISHED(slot) = 0;
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
                PC_CAR_STATE(i) = 0;
                PC_CAR_FINISHED(i) = 0;
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
        if (!(gGameState & 8)) AudKillSong();
        if (gRestartGame) { ZOID_Reset(); InitBlits(); }
        else if (!(gGameState & 8)) InitWaves();
        munge_gLink_data();
        GhostStart();
        ResetHighScores();
    }
    if (gFogColor < 19) gFogColor = gTrackNum;
    if (!(gGameState & 8)) {
        HFOV = 90.0f;
        ZOID_ResetWindows();
        gNumWindows = gRace == 2 ? (s16)1 : gNumPlayers;
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
        WorldFogDensity(i, gCamera[i].pos);
    }
    if (!(gGameState & 8)) AudStopAllEffects();
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
        if (gRace == 6) InitBattlePlayer(i);
        gGamePointPlayed = 0;
    }
    if (!(gGameState & 8)) ClearRushControls();
    StartListeners();
    if (gRace == 6) { InitBattlePoints(); ShotInit(); }
    if (gRace == 4) InitStunt();
    if (!(gGameState & 8)) PS_RumbleGo();
    gUpsideDownTrack = gCheatState[3];
    inited = 0;
    return 1;
}
