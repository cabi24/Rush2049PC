/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* best: PrePlay = PrePlay@game.c. NOT A MATCH: unit `FAIL PrePlay: 44 of 589 words differ |
 * words 58 ops 2 norm 2 | frame 256/256` (UARGS="--internal InitBattlePoints --internal leStartLoadingEffect --internal
 * leEndLoadingEffect"); 14 of the 58 rows are own-data relocations (inited static, two float literals).
 * Changes from best.c (dlf/d0.c, 395 rows / frame 256/88):
 *  - DEVICE (disclosed): one compiled-out `if (useHud) {}` at the top = two code-free uopt blocks; raises the
 *    callee-saved toll so the constant-1 and gLink/gNumActiveCars address webs split as in retail (any position works).
 *  - carsetup_flag = carsetup_flag and gBattlePoints = gBattlePoints (the draft's D_80143A08 / D_801494A8 were wrong).
 *  - MAT3's loading-effect calls around LoadTrack as empty inlined (void) functions (leStartLoadingEffect /
 *    leEndLoadingEffect): the two 8-byte inline slots that make the frame 256 (exact frame residual; MAT3 text).
 * Remaining: caller-saved colour ties (first 0x2000000 loop: slot v1 / gGameCar address a3 in retail; the
 *  num_drones/num_our_drones lh pair; the Init_MDrive loop's v0/v1 rotation). force.sh: forcing the gGameCar
 *  address web to a3 alone gives 36 rows (the slot follows). */
/* PrePlay (0x800FB2C8) = MAT3 game.c PrePlay (GC 0x80014D20). NOT A MATCH (55 ops rows, frame 256/88).
 * Score in the unit together with groups/update_game/{UpdateGame,CalcPolePositions}.c (both --internal) and --internal InitBattlePoints --internal AudLock --internal AudUnlock --internal AudSend.
 * InitStunt here is the MAT3-shaped InitStunt (EQUAL as its own body, and inlinable as in retail).
 * Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10. */
/* PrePlay (0x800FB2C8) = MAT3 game.c PrePlay(useHud, cancel) (GC 0x80014D20, size 0x624). */
typedef unsigned char u8;
typedef signed char s8;
typedef short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct Player76 { u8 slot; u8 pad01[63]; s16 numCrashes; u8 pad42[0x48 - 0x42]; void *ref; } Player76;
typedef struct Car952 {
    u8 pad000[232];
    s32 appearance;         /* 0x0E8 */
    u8 pad0EC[3];
    s8 place_locked;        /* 0x0EF */
    u8 pad0F0[0x110 - 0xF0]; struct pc_visual visuals[21]; u8 pad0F0_308[0x359 - 0x308];
    s8 dead;                /* 0x359 */
    u8 pad35A[896 - 858];
    void *player;           /* 0x380 */
    u8 pad384[902 - 900];
    s16 health;             /* 0x386 */
    u8 pad388[931 - 904];
    s8 points;              /* 0x3A3 */
    u8 pad3A4[940 - 932];
    f32 reloadingTime;      /* 0x3AC */
    u8 pad3B0[952 - 944];
} Car952;
typedef struct Model2056 {
    u8 pad000[8];
    u8 body_type;           /* 0x008 */
    u8 pad009;
    s8 autotrans;           /* 0x00A */
    u8 pad00B[1990 - 11];
    s16 slot;               /* 0x7C6 */
    u8 pad7C8[2];
    s16 we_control;         /* 0x7CA */
    s8 drone_type;          /* 0x7CC */
    u8 pad7CD[2004 - 1997];
    s32 appearance;         /* 0x7D4 */
    u8 pad7D8[2056 - 2008];
} Model2056;
typedef struct Link8 { u8 track; u8 pad1[4]; u8 polepos; u8 flags; u8 owner; } Link8;
typedef struct Cam152 { u8 pad00[36]; f32 pos[3]; u8 pad30[104]; } Cam152;
typedef struct AudMsg { u8 pad0[2]; s8 type; } AudMsg;

extern u32 gGameState;
extern s8 gRestartGame;
extern s8 gGamePointPlayed;
extern s32 gRace;
extern s16 gNumPlayers;
extern Player76 gPlayers[];   /* gPlayers */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the race roster is pc_links, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s8 gNumActiveCars;
extern s16 num_our_drones;
extern s16 num_drones;
extern s16 num_humans;
/* On the PC the drone lists and CarSetup's flags are pc_drones, pc_our_drones and pc_carsetup_flags, PC_MAX_LINKS
 * long, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md 8). */
extern s16 sched_to_use;
extern s16 new_sched_to_use;
extern s16 sched_index;
extern s16 cur_drone;
extern s8 gCoastFlag;
extern s8 gRetireGame;
extern s8 gOutOfTime;
extern f32 gGravity;
extern s8 gFogColor;
extern s8 gTrackNum;
extern f32 HFOV;
extern s16 gNumWindows;
extern Cam152 gCamera[];
extern u8 gFogColors[][3]; /* gFogColors */
/* game.frame_rate = 60 (docs/port/FRAMERATE.md 7.3): the track's load goes a wave a call, a call a frame, so at 60 it
 * is called on the frames that complete a 30 Hz frame: the countdown begins within a retrace of original's. */
#define PC_LOAD_ON30(call) ((!pc_fps60 || pc_frame_steps) ? (call) : 0)
extern s8 gGravityScale;
extern f32 gravityArray[];        /* gravityArray */
extern u8 audio_lock_queue[];         /* audio lock queue */
extern u8 audio_command_queue[];         /* audio command queue */
extern s16 gCheatState[];        /* gCheatState */
extern s8 gUpsideDownTrack;

void SetOptions(void);
void EnableGameSounds(s32 on);
void SetupCar(s16 slot, u8 body, s32 apply);
void CalcPolePositions(void);         /* _CalcPolePositions */
void InitCPS(void);
void InitDrones(void);
void Init_MDrive(s32 a, s16 slot);
void ShowHUD(void);
void AudKillSong(void);
void AudStopAllEffects(void);
void ZOID_Reset(void);
void InitBlits(void);
void InitWaves(void);
void munge_gLink_data(void);
void GhostStart(void);
void ResetHighScores(void);
void ZOID_ResetWindows(void);
s32 ZOID_SplitMenuScreen(s32 n);           /* ZOID_SplitScreen */
void ReloadTrack(void);
s32 LoadTrack(s32 track, s32 a1, s32 a2);
void InitTextureAnimation(void);
void ZOID_SetFog(s32 a0, s32 a1, s32 r, s32 g, s32 b, s32 a, s32 win);
void WorldFogDensity(s32 i, f32 *pos);
void init_cars(void);
void StartEnvironment(void);
void init_views(void);
void InitBattlePlayer(s16 slot);           /* InitBattlePlayer (image B) */
void ClearRushControls(void);
void StartListeners(void);
void InitBattlePoints(void);
void ShotInit(void);               /* ShotInit (image B) */
void InitStunt(void);
void PS_RumbleGo(void);
void *memset(void *, s32, u32);

extern s8 gBattlePoints[16];       /* gBattlePoints */
extern s8 gTeamPoints[4];        /* gTeamPoints */


extern s32 gNumObjects, cull_vertex_count, gNumBlitInsts, gNumMatrices, gNumPolyInsts;
extern s16 ObjHead;
extern f32 gGlobalCameraFacingScale;
extern s16 gNumLights;
extern intptr_t gLights;   /* Lights2 *: pointer-sized on the PC */
extern u8 default_lights[];
AudMsg *GetCmd(void);
s32 osRecvMesg(void *mq, void *msg, s32 flag);
s32 osSendMesg(void *mq, void *msg, s32 flag);

void leStartLoadingEffect(void)
{
}

void leEndLoadingEffect(void)
{
}

void ZOID_Reset(void);

/* The game's own AudKillSong, AudStopAllEffects and InitBattlePoints (this file had copies of AudKillSong and
 * InitBattlePoints: the same code). */
void InitBattlePoints(void);

typedef struct StuntInfo { s32 active; u8 pad[120]; } StuntInfo;
extern u8 gStuntPlayer[480];      /* gStuntPlayer */
extern StuntInfo gStuntInfo[4]; /* gStuntInfo */
extern f32 stunt_eTime;          /* eTime */
extern s32 gStuntsDone;

static void ResetStunts(void)
{
    s32 playerIdx;
    for (playerIdx = 0; playerIdx < 4; playerIdx++) {
        memset(&gStuntInfo[playerIdx], 0, sizeof(StuntInfo));
        gStuntInfo[playerIdx].active |= 0x15000000;
    }
}

void InitStunt(void);

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
