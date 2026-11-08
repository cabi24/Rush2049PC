/* port/assets/tests/test_audiocmds.c: the audio commands the game posts on events (menus, race starts and ends,
 * pausing, the audio shutdown), through the game's own code, when the audio manager's message pool audiomgr_gCmds is
 * empty and when its queue audio_command_queue is full (docs/port/QA.md "Integration soak", fix 6). assets_audiomgr has the
 * per-retrace senders; these are the other callers of GetCmd, the ones a starved host can catch at a
 * screen change. Each must, with the pool empty: give the lock back, send nothing and take no message; with the
 * queue full: give its message back; otherwise: send its command. The ones that go on after posting (the way out
 * of a race, AbortGame) must still do the rest; the shutdown's two posts wait for the manager instead. And
 * GetCmd's test hook (RUSH2049_TEST_AUDIO_POOL_FAIL) fails every Nth call. Made-up values only.
 * game: src/blob/groups/abort_game/GetCmd.c src/blob/AudResumeAllEffects.c src/blob/AudMono.c
 * game: src/blob/AudPlaySong.c src/blob/AudKillSong.c src/blob/AudStopAllEffects.c
 * game: src/blob/AudPauseAllEffects.c src/blob/groups/game_sounds/group.c src/blob/groups/heap_release/AudioShutdown.c
 * game: src/blob/groups/audioscr_update/uninit_w36w3.c src/blob/groups/audioscr_update/init_w36q.c
 * game: src/blob/groups/audioscr_update/updateAudioScreen.c src/blob/groups/highscorescr/highscore_InitScreen.c
 * game: src/blob/groups/gameover/gameover.c src/blob/groups/abort_game/group.c
 * game: nonmatching/game/updatePauseScreen.c nonmatching/game/PrePlay.c
 * game: src/blob/PauseTargets.c src/blob/InitBattlePoints.c
 * game: src/blob/groups/SetupCarColor/ctx_func_800C8918.c
 * game: src/blob/InitStunt.c src/blob/ZOID_Reset.c src/blob/groups/state/halfont.c src/blob/ZOID_SetColor.c
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "pc_game_types.h"

#include "pc_prelude.h"
#include "support.h"

/* ---------------------------------------------------------------- the pool, the lock and the queue */

struct msg { int16_t id; int8_t cmd, used; uint8_t payload[28]; };   /* tAudioMsg on the PC: 32 bytes */
_Static_assert(sizeof(struct msg) == 0x20, "a message's PC size");
/* The pool is the PC's (pc_game_types.h, docs/port/DECOUPLE.md 9): PC_SND_MSG_SLOTS, of which the game walks 128 here. */
struct pc_snd_msg pc_snd_msgs[PC_SND_MSG_SLOTS];
#define audiomgr_gCmds (*(struct msg (*)[PC_SND_MSG_SLOTS])pc_snd_msgs)
uint8_t audio_lock_queue[64], audio_command_queue[64];
extern int pc_audio_pool_fail_every;   /* GetCmd.c's test hook */

static int g_locks, g_nsent, g_queue_full, g_blocked, g_spins, g_drain_on_spin;
static struct msg *g_sent[8];
int32_t osRecvMesg(void *mq, void *msg, int32_t flag)
{
    if (mq == audio_lock_queue)
        g_locks++;
    return 0;
}
int32_t osSendMesg(void *mq, void *msg, int32_t flag)
{
    if (mq == audio_lock_queue) {
        g_locks--;
        return 0;
    }
    if (mq == audio_command_queue) {
        if (g_queue_full) {
            if (flag == 0)
                return -1;   /* OS_MESG_NOBLOCK on a full queue */
            g_blocked++;     /* OS_MESG_BLOCK: the manager takes one, and this one goes in */
        }
        if (g_nsent < 8)
            g_sent[g_nsent++] = msg;
    }
    return 0;
}
void audio_lock(void) { g_locks++; }   /* the lock's helpers (audio_options_menu's) */
void audio_unlock(void) { g_locks--; }
/* A busy-wait's yield: the manager runs, and with g_drain_on_spin takes every message. */
void pc_os_spin_yield(const char *site)
{
    g_spins++;
    if (g_drain_on_spin)
        for (int i = 0; i < 128; i++)
            audiomgr_gCmds[i].used = 0;
}

/* ---------------------------------------------------------------- what the paths read and set */

int8_t gInited_AudioMgr;              /* gInited (the shutdown) */
int32_t gActiveEdges;
int32_t gMainSelect;
int32_t gRace;
int8_t gSoftReset;
int8_t gameover_inited;              /* the game over screen inited */
int32_t gGameOverState;
int32_t gSkipToGameState, gNewGameState;
uint8_t gRestartGame;
void *n64_memcpy(void *d, const void *s, size_t n) { return memcpy(d, s, n); }
void *n64_memset(void *d, int c, size_t n) { return memset(d, c, n); }

/* Everything else the linked files reference, which these paths call or read as nothing: a no-op returning 0, or
 * a zeroed block (tools: nm -u over the objects, a call relocation makes a function). */
#define STUB_FN(n) long n(void) { return 0; }
#define STUB_DATA(n) uint8_t n[0x1000] __attribute__((aligned(16)));
struct pc_link pc_links[PC_LINK_SLOTS];   /* the race roster (pc_game_types.h), which data.c defines */
struct pc_model pc_models[PC_MODEL_SLOTS];   /* the model records (pc_game_types.h) */
struct pc_car pc_cars[PC_CAR_SLOTS];       /* the car records (pc_game_types.h) */
short pc_drones[PC_MAX_LINKS], pc_our_drones[PC_MAX_LINKS];   /* the drone lists (pc_game_types.h) */
signed char pc_carsetup_flags[PC_MAX_LINKS];   /* CarSetup's flags (pc_game_types.h) */
STUB_FN(CalcPolePositions) STUB_FN(GhostStart) STUB_FN(InitBlits) STUB_FN(InitWaves) STUB_FN(LoadTrack)
STUB_FN(SaveFlush) STUB_FN(SaveServer) STUB_FN(SetControllerStickEdges) STUB_FN(SetHighScoreState)
STUB_FN(SetOptions) STUB_FN(ShowCountdown) STUB_FN(ZOID_ResetWindows) STUB_FN(InitPane)
STUB_FN(removeBG) STUB_FN(audio_calcMenuRect) STUB_FN(PlayUDSound) STUB_FN(PlayLRSound)
STUB_FN(DeallocateMemory) STUB_FN(InitTextureAnimation) STUB_FN(createBG) STUB_FN(audio_TitlePane)
STUB_FN(ClearRushControls) STUB_FN(AudPlayEffect) STUB_FN(RemoveObjectTree)
STUB_FN(ReleaseEnviron) STUB_FN(ScaleUV) STUB_FN(ZOID_SetTexture) STUB_FN(PS_RumbleGo)
STUB_FN(game_memcmp) STUB_FN(ZOID_SetFog) STUB_FN(WorldFogDensity) STUB_FN(InitSkids) STUB_FN(MBOX_FindTexture_WORK)
STUB_FN(WPitchUV) STUB_FN(WRollUV) STUB_FN(SaveGlobals) STUB_FN(UnloadTrack) STUB_FN(AudFree3D)
STUB_FN(BlinkResurrectingCars) STUB_FN(StopListeners) STUB_FN(End_MDrive) STUB_FN(modelgo) STUB_FN(StartListeners)
STUB_FN(updateControlScreen) STUB_FN(updateOptionScreen) STUB_FN(Init_MDrive) STUB_FN(munge_gLink_data)
STUB_FN(highscore_UninitScreen) STUB_FN(RankScores) STUB_FN(GetCircuitCode) STUB_FN(UpdateCircuit) STUB_FN(ClearScreen)
STUB_FN(ResetHighScores) STUB_FN(InitDrones) STUB_FN(InitCPS) STUB_FN(InitBattlePlayer) STUB_FN(updateTitleScreen)
STUB_FN(ShotInit)
STUB_FN(gameover_validOption) STUB_FN(GhostEnd) STUB_FN(AudioShutdownSys) STUB_FN(MBOX_HideObject)
STUB_FN(MBOX_ShowObject) STUB_FN(StartEnvironment) STUB_FN(SetupCar) STUB_FN(CalcUnlockedStuff)
STUB_FN(TB_GetFontLineHeight) STUB_FN(HAL_GetTextWidth) STUB_FN(ZeroCameras)
STUB_FN(StartGameMusic) STUB_FN(ResumeGameMusic) STUB_FN(updateVideoScreen) STUB_FN(ReloadTrack)
STUB_FN(modelstop) STUB_FN(updateGameState) STUB_FN(RemoveHUD) STUB_FN(init_views)
STUB_FN(PlayStartABSound) STUB_FN(setPauseState) STUB_FN(MBOX_NewObject) STUB_FN(ShowHUD)
STUB_FN(HAL_SetFontInternal) STUB_FN(NewMultiBlit) STUB_FN(RemovePanes) STUB_FN(RemoveBlit)
STUB_FN(MBOX_FindObject_WORK) STUB_FN(RecordGameOver) STUB_FN(ResumeTimer) STUB_FN(PauseTimer)
STUB_FN(pause_TitlePane) STUB_FN(ZOID_SplitMenuScreen) STUB_FN(init_cars)
STUB_DATA(gFrameSeconds) STUB_DATA(gGameSoundsEnabled) STUB_DATA(gFxTags) STUB_DATA(gSortedFxTags) STUB_DATA(gSequences)
STUB_DATA(audio_buttonNames) STUB_DATA(audio_buttonInfo) STUB_DATA(audio_buttonsInited) STUB_DATA(audio_buttonsJustInited) STUB_DATA(audio_gButtonScale)
STUB_DATA(audio_frontEndBlit) STUB_DATA(audio_pauseBlit) STUB_DATA(audio_inited) STUB_DATA(audio_blit) STUB_DATA(audio_panesCreated)
STUB_DATA(gAudioMenuSelect) STUB_DATA(exitAudioScreen) STUB_DATA(audio_gSong) STUB_DATA(gLastSong) STUB_DATA(audio_gTitlePane)
STUB_DATA(audio_gControllerRemoved) STUB_DATA(unitmatrix) STUB_DATA(gGamePointPlayed) STUB_DATA(gameover_multiblit) STUB_DATA(gameover_titlePos)
STUB_DATA(gameover_blit) STUB_DATA(highscore_inited) STUB_DATA(gCheatState) STUB_DATA(gPauseState) STUB_DATA(pause_gControllerRemoved)
STUB_DATA(pause_gYes) STUB_DATA(gGameState) STUB_DATA(default_lights) STUB_DATA(audio_init_texture_name_1)
STUB_DATA(audio_init_texture_name_2) STUB_DATA(audio_init_texture_name_3) STUB_DATA(gravityArray) STUB_DATA(fx_volume_fade_time) STUB_DATA(ObjInstanceList)
STUB_DATA(gEnvList) STUB_DATA(gCoastFlag) STUB_DATA(gNumLights) STUB_DATA(gLights) STUB_DATA(gThisTrack)
STUB_DATA(gOutOfTime) STUB_DATA(gStuffUnlocked) STUB_DATA(gRetireGame) STUB_DATA(gPrevUnlockedTracks) STUB_DATA(gPrevUnlockedCars)
STUB_DATA(gPrevUnlockedCircuits) STUB_DATA(gGravity) STUB_DATA(gPrevUnlockedTrans) STUB_DATA(gFogColor) STUB_DATA(gPrevUnlockedEngines)
STUB_DATA(gPrevUnlockedTires) STUB_DATA(gPrevUnlockedFrames) STUB_DATA(circuitCode) STUB_DATA(highscore_gCircuitCode)
STUB_DATA(gDrawName) STUB_DATA(gTargetList) STUB_DATA(gOptions) STUB_DATA(gSavedGhost)
STUB_DATA(text_lock_queue) STUB_DATA(gBattlePoints) STUB_DATA(gActiveAutoEdges) STUB_DATA(gTrackNum) STUB_DATA(pauseMenuSelect)
STUB_DATA(gPlayers_save) STUB_DATA(gUnlockedTracks) STUB_DATA(gUnlockedCars) STUB_DATA(gUnlockedCircuits)
STUB_DATA(gUnlockedTrans) STUB_DATA(gUnlockedEngines) STUB_DATA(gUnlockedTires) STUB_DATA(gUnlockedFrames) STUB_DATA(gNumWindows)
STUB_DATA(gUpsideDownTrack) STUB_DATA(gStuntPlayer) STUB_DATA(gTeamPoints) STUB_DATA(gBackwards) STUB_DATA(gStuntsDone)
STUB_DATA(gNumActiveCars) STUB_DATA(num_our_drones) STUB_DATA(num_drones)
STUB_DATA(sched_to_use) STUB_DATA(new_sched_to_use) STUB_DATA(sched_index) STUB_DATA(cur_drone)
STUB_DATA(num_humans) STUB_DATA(HFOV) STUB_DATA(controlScrActive) STUB_DATA(multiPlayerBlit) STUB_DATA(gGravityScale)
STUB_DATA(gModelTime) STUB_DATA(audio_cursorSpeed) STUB_DATA(gCircuitPlayer) STUB_DATA(gCircuitTrack) STUB_DATA(gCircuitSize)
STUB_DATA(gContStick) STUB_DATA(playerInPause) STUB_DATA(gNumObjects) STUB_DATA(gContEdges) STUB_DATA(gStuntInfo)
STUB_DATA(gNumMatrices) STUB_DATA(cull_vertex_count) STUB_DATA(ObjHead) STUB_DATA(stunt_eTime) STUB_DATA(gNumBlitInsts)
STUB_DATA(gNumPolyInsts) STUB_DATA(gGlobalCameraFacingScale) STUB_DATA(gNumPlayers) STUB_DATA(gText) STUB_DATA(gCamera)
STUB_DATA(gControllerData) STUB_DATA(gFogColors) STUB_DATA(gHighScoreState) STUB_DATA(gNumModels) STUB_DATA(gNumWindows)
STUB_DATA(gPlayers) STUB_DATA(gGameState)

/* ---------------------------------------------------------------- the game's functions */

struct msg *GetCmd(void);
void AudResumeAllEffects(void);                                 /* 9: resume the paused effects */
void AudMono(int32_t mono);                         /* 11: mono */
void AudPlaySong(int32_t song, int32_t loop);     /* 0: play a song */
void AudKillSong(void);                           /* 1: stop the song */
void AudStopAllEffects(void);                           /* 7: stop every effect */
void AudPauseAllEffects(void);
void AudSetMasterVol(float blend, float amount, int32_t first, int32_t second);   /* 10: the volumes */
void post_type1(void);                                    /* the audio shutdown's (carcolor's, their owner) */
void post_type7(void);
void AudioShutdown(void);                                 /* the audio shutdown */
void controlMain(void);                                 /* the game over screen's choice */
void updateGameOverScreen(void);                              /* the game over screen */
void AbortGame(void);

static void sync_entry_register_3(void) { AudPlaySong(3, 1); }
static void AudMono_1(void) { AudMono(1); }
static void speed_set_half(void) { AudSetMasterVol(0.5f, 0.25f, 1, 0); }

static void drain(void)
{
    for (int i = 0; i < 128; i++)
        audiomgr_gCmds[i].used = 1, audiomgr_gCmds[i].cmd = -1;
}
static int in_use(void)
{
    int n = 0;
    for (int i = 0; i < 128; i++)
        n += audiomgr_gCmds[i].used != 0;
    return n;
}
static void reset(void)
{
    g_locks = g_nsent = g_queue_full = g_blocked = g_spins = g_drain_on_spin = 0;
    memset(g_sent, 0, sizeof g_sent);
}

/* ---------------------------------------------------------------- the posters: lock, take, fill, unlock, send */

static const struct { const char *name; void (*post)(void); int cmd; } POSTERS[] = {
    {"AudResumeAllEffects", AudResumeAllEffects, 9},          {"AudMono", AudMono_1, 11},
    {"AudPlaySong", sync_entry_register_3, 0}, {"AudKillSong", AudKillSong, 1},
    {"AudStopAllEffects", AudStopAllEffects, 7}, {"AudPauseAllEffects", AudPauseAllEffects, 7},
    {"AudSetMasterVol", speed_set_half, 10},
};

static void posters(void)
{
    for (size_t k = 0; k < sizeof POSTERS / sizeof POSTERS[0]; k++) {
        int failures = test_failures;
        /* the pool empty: the lock given back, nothing sent, no message taken */
        reset();
        drain();
        POSTERS[k].post();
        CHECK(g_locks == 0 && g_nsent == 0 && in_use() == 128);
        /* one message free, the queue full: it is given back */
        audiomgr_gCmds[77].used = 0;
        g_queue_full = 1;
        POSTERS[k].post();
        CHECK(g_locks == 0 && g_nsent == 0 && audiomgr_gCmds[77].used == 0 && in_use() == 127);
        /* and with room, the command goes */
        g_queue_full = 0;
        POSTERS[k].post();
        CHECK(g_locks == 0 && g_nsent == 1 && g_sent[0] == &audiomgr_gCmds[77] && audiomgr_gCmds[77].used == 1 &&
              audiomgr_gCmds[77].cmd == POSTERS[k].cmd);
        if (test_failures != failures)
            fprintf(stderr, "  in %s\n", POSTERS[k].name);
    }
}

/* ---------------------------------------------------------------- the audio shutdown: its posts wait */

static void shutdown(void)
{
    /* post_type1 with the pool empty waits (yielding) for the manager to free a message */
    reset();
    drain();
    g_drain_on_spin = 1;
    post_type1();
    CHECK(g_locks == 0 && g_spins == 1 && g_nsent == 1 && g_sent[0]->cmd == 1 && g_sent[0]->used == 1);
    /* post_type7 with the queue full blocks until the manager takes one, instead of losing its message */
    reset();
    drain();
    audiomgr_gCmds[5].used = 0;
    g_queue_full = 1;
    post_type7();
    CHECK(g_locks == 0 && g_spins == 0 && g_blocked == 1 && g_nsent == 1 && g_sent[0] == &audiomgr_gCmds[5] &&
          audiomgr_gCmds[5].cmd == 7);
    /* the whole shutdown from an empty pool: both posts get through, and its wait for the pool ends */
    reset();
    drain();
    g_drain_on_spin = 1;
    gInited_AudioMgr = 1;
    AudioShutdown();
    CHECK(gInited_AudioMgr == 0 && g_locks == 0 && g_nsent == 2 && g_sent[0]->cmd == 1 && g_sent[1]->cmd == 7);
}

/* ---------------------------------------------------------------- the way out of a race: the rest still runs */

static void race_exits(void)
{
    /* the game over screen's "quit" (selection 1, a championship race): the stop is dropped, the state still moves */
    reset();
    drain();
    gActiveEdges = 3, gMainSelect = 1, gRace = 2, gSkipToGameState = 0, gNewGameState = 0;
    controlMain();
    CHECK(g_locks == 0 && g_nsent == 0 && in_use() == 128 && gSkipToGameState == 0x40000 && gNewGameState == 4);
    /* selection 1 in another mode: its stop of every effect, then AbortGame's, both dropped; AbortGame still runs */
    reset();
    drain();
    gActiveEdges = 3, gMainSelect = 1, gRace = 0, gRestartGame = 0, gNewGameState = 0;
    controlMain();
    CHECK(g_locks == 0 && g_nsent == 0 && in_use() == 128 && gRestartGame == 1 && gNewGameState == 0x40000);
    /* selections 2-6 (restart, the menus): the stop dropped, the next state set */
    reset();
    drain();
    gActiveEdges = 3, gMainSelect = 2, gSkipToGameState = 0, gNewGameState = 0;
    controlMain();
    CHECK(g_locks == 0 && g_nsent == 0 && gSkipToGameState == 0x40 && gNewGameState == 4);
    /* with the queue full, the message is given back and the state still moves */
    reset();
    drain();
    audiomgr_gCmds[9].used = 0;
    g_queue_full = 1;
    gActiveEdges = 3, gMainSelect = 3, gSkipToGameState = 0, gNewGameState = 0;
    controlMain();
    CHECK(g_locks == 0 && g_nsent == 0 && audiomgr_gCmds[9].used == 0 && gSkipToGameState == 0x80 && gNewGameState == 4);
    /* and with room it is sent as before */
    g_queue_full = 0;
    gActiveEdges = 3, gMainSelect = 4, gSkipToGameState = 0, gNewGameState = 0;
    controlMain();
    CHECK(g_locks == 0 && g_nsent == 1 && g_sent[0] == &audiomgr_gCmds[9] && audiomgr_gCmds[9].cmd == 1 &&
          gSkipToGameState == 0x100 && gNewGameState == 4);

    /* the game over screen's soft reset: the stop dropped, the state set */
    reset();
    drain();
    gameover_inited = 1, gSoftReset = 1, gSkipToGameState = 0, gNewGameState = 0;
    updateGameOverScreen();
    CHECK(g_locks == 0 && g_nsent == 0 && in_use() == 128 && gSkipToGameState == 1 && gNewGameState == 4);

    /* AbortGame itself, restarting: the stop dropped, the restart state set */
    reset();
    drain();
    gRestartGame = 1, gNewGameState = 0;
    AbortGame();
    CHECK(g_locks == 0 && g_nsent == 0 && in_use() == 128 && gNewGameState == 0x40000);
    /* with the queue full, its message is given back */
    audiomgr_gCmds[100].used = 0;
    g_queue_full = 1;
    gNewGameState = 0;
    AbortGame();
    CHECK(g_locks == 0 && g_nsent == 0 && audiomgr_gCmds[100].used == 0 && gNewGameState == 0x40000);
    g_queue_full = 0;
    AbortGame();
    CHECK(g_locks == 0 && g_nsent == 1 && g_sent[0] == &audiomgr_gCmds[100] && audiomgr_gCmds[100].cmd == 7);
}

/* ---------------------------------------------------------------- the test hook */

static void hook(void)
{
    memset(audiomgr_gCmds, 0, sizeof audiomgr_gCmds);
    pc_audio_pool_fail_every = 3;
    struct msg *a = GetCmd(), *b = GetCmd(), *c = GetCmd(), *d = GetCmd();
    CHECK(a == &audiomgr_gCmds[0] && b == &audiomgr_gCmds[1] && c == NULL && d == &audiomgr_gCmds[2] && in_use() == 3);
    pc_audio_pool_fail_every = 0;
    CHECK(GetCmd() == &audiomgr_gCmds[3] && GetCmd() == &audiomgr_gCmds[4] && GetCmd() == &audiomgr_gCmds[5]);
}

int main(void)
{
    posters();
    shutdown();
    race_exits();
    hook();
    return TEST_DONE();
}
