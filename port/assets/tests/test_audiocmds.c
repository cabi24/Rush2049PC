/* port/assets/tests/test_audiocmds.c: the audio commands the game posts on events (menus, race starts and ends,
 * pausing, the audio shutdown), through the game's own code, when the audio manager's message pool D_80142DD8 is
 * empty and when its queue D_801427A8 is full (docs/port/QA.md "Integration soak", fix 6). assets_audiomgr has the
 * per-retrace senders; these are the other callers of func_80091B00, the ones a starved host can catch at a
 * screen change. Each must, with the pool empty: give the lock back, send nothing and take no message; with the
 * queue full: give its message back; otherwise: send its command. The ones that go on after posting (the way out
 * of a race, AbortGame) must still do the rest; the shutdown's two posts wait for the manager instead. And
 * func_80091B00's test hook (RUSH2049_TEST_AUDIO_POOL_FAIL) fails every Nth call. Made-up values only.
 * game: src/blob/groups/slot18_alloc/func_80091B00.c src/blob/func_800D6348.c src/blob/func_800D6E00.c
 * game: src/blob/sync_entry_register.c src/blob/object_type1_create.c src/blob/object_type7_create.c
 * game: src/blob/object_type7_create_alt.c src/blob/groups/vsync/group.c src/blob/groups/heap_release/func_800C8918.c
 * game: src/blob/groups/audio_options_menu/uninit_w36w3.c src/blob/groups/audio_options_menu/init_w36q.c
 * game: src/blob/groups/audio_options_menu/func_800D7634.c src/blob/groups/billboard_render/track_texture_load.c
 * game: src/blob/groups/gameover/gameover.c src/blob/groups/slot18_alloc/group.c
 * game: nonmatching/game/RaceStateMachine_Update.c nonmatching/game/display_list_flush.c
 * game: src/blob/groups/carcolor/ctx_func_800C8918.c
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

/* ---------------------------------------------------------------- the pool, the lock and the queue */

struct msg { int16_t id; int8_t cmd, used; uint8_t payload[28]; };   /* tAudioMsg on the PC: 32 bytes */
_Static_assert(sizeof(struct msg) == 0x20, "a message's PC size");
struct msg D_80142DD8[128];
uint8_t D_80142728[64], D_801427A8[64];
extern int pc_audio_pool_fail_every;   /* func_80091B00.c's test hook */

static int g_locks, g_nsent, g_queue_full, g_blocked, g_spins, g_drain_on_spin;
static struct msg *g_sent[8];
int32_t osRecvMesg(void *mq, void *msg, int32_t flag)
{
    if (mq == D_80142728)
        g_locks++;
    return 0;
}
int32_t osSendMesg(void *mq, void *msg, int32_t flag)
{
    if (mq == D_80142728) {
        g_locks--;
        return 0;
    }
    if (mq == D_801427A8) {
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
void func_80091AF0(void) { g_locks++; }   /* the lock's helpers (audio_options_menu's) */
void func_80091AF8(void) { g_locks--; }
/* A busy-wait's yield: the manager runs, and with g_drain_on_spin takes every message. */
void pc_os_spin_yield(const char *site)
{
    g_spins++;
    if (g_drain_on_spin)
        for (int i = 0; i < 128; i++)
            D_80142DD8[i].used = 0;
}

/* ---------------------------------------------------------------- what the paths read and set */

int8_t D_8011023C;              /* gInited (the shutdown) */
int32_t D_8015694C;             /* gActiveEdges */
int32_t D_801391E0;             /* gMainSelect */
int32_t gameplay_mode;          /* gRace */
int8_t D_80157244;              /* gSoftReset */
int8_t D_80114728;              /* the game over screen inited */
int32_t D_80138870;             /* gGameOverState */
int32_t gSkipToGameState, state_word_b;
uint8_t D_80114650;             /* gRestartGame */
void *n64_memcpy(void *d, const void *s, size_t n) { return memcpy(d, s, n); }
void *n64_memset(void *d, int c, size_t n) { return memset(d, c, n); }

/* Everything else the linked files reference, which these paths call or read as nothing: a no-op returning 0, or
 * a zeroed block (tools: nm -u over the objects, a call relocation makes a function). */
#define STUB_FN(n) long n(void) { return 0; }
#define STUB_DATA(n) uint8_t n[0x1000] __attribute__((aligned(16)));
struct pc_link pc_links[PC_LINK_SLOTS];   /* the race roster (pc_game_types.h), which data.c defines */
STUB_FN(CalcPolePositions) STUB_FN(GhostStart) STUB_FN(InitBlits) STUB_FN(InitWaves) STUB_FN(LoadTrack)
STUB_FN(SaveFlush) STUB_FN(SaveServer) STUB_FN(SetControllerStickEdges) STUB_FN(SetHighScoreState)
STUB_FN(SetOptions) STUB_FN(ShowCountdown) STUB_FN(ZOID_ResetWindows) STUB_FN(ambient_sound_set)
STUB_FN(ambient_sounds_clear) STUB_FN(audio_calcMenuRect) STUB_FN(audio_distance_atten) STUB_FN(audio_doppler)
STUB_FN(audio_effect_process) STUB_FN(camera_shake_start) STUB_FN(createBG) STUB_FN(credits_scroll)
STUB_FN(entity_audio_update) STUB_FN(entity_flags_apply) STUB_FN(entity_spawn_callback)
STUB_FN(entity_transform_apply) STUB_FN(func_8008B32C) STUB_FN(func_8008D870) STUB_FN(func_800A14FC)
STUB_FN(func_800A1910) STUB_FN(func_800A7480) STUB_FN(func_800AB18C) STUB_FN(func_800B0580) STUB_FN(func_800B24EC)
STUB_FN(func_800B5898) STUB_FN(func_800B5940) STUB_FN(func_800B8810) STUB_FN(func_800B912C) STUB_FN(func_800C885C)
STUB_FN(func_800CFCA8) STUB_FN(func_800D5374) STUB_FN(func_800D5828) STUB_FN(func_800D60AC) STUB_FN(func_800D6530)
STUB_FN(func_800960CC) STUB_FN(func_800D91A0) STUB_FN(func_800DB1E0) STUB_FN(func_800E543C) STUB_FN(func_800EC914)
STUB_FN(func_800F1114) STUB_FN(func_800F2888) STUB_FN(func_800F42C8) STUB_FN(func_800F45F8) STUB_FN(func_800F7448)
STUB_FN(func_800F7EB0) STUB_FN(func_800FAEE4) STUB_FN(func_800FAF50) STUB_FN(func_8038CA24) STUB_FN(func_8038F454)
STUB_FN(func_803908D0)
STUB_FN(gameover_validOption) STUB_FN(ghost_race_setup) STUB_FN(init_wait_completion) STUB_FN(model_data_load)
STUB_FN(model_transform_setup) STUB_FN(music_control) STUB_FN(music_tempo_set) STUB_FN(net_state_validate)
STUB_FN(object_bytes_sum_global) STUB_FN(object_manager_update) STUB_FN(particle_velocity_set)
STUB_FN(physics_init_mode0) STUB_FN(physics_init_mode1) STUB_FN(physics_sym) STUB_FN(player_flag_clear_process)
STUB_FN(players_frame_update) STUB_FN(playgame_state_change) STUB_FN(records_screen) STUB_FN(render_post_process)
STUB_FN(resource_type_select) STUB_FN(setPauseState) STUB_FN(sign_extend_call) STUB_FN(skid_mark_render)
STUB_FN(slot_state_setup) STUB_FN(sound_control) STUB_FN(sound_handles_clear) STUB_FN(sound_stop)
STUB_FN(string_copy_format) STUB_FN(track_lighting_setup) STUB_FN(viDisableAccum) STUB_FN(viEnableAccum)
STUB_FN(voice_stop) STUB_FN(wheel_render_full) STUB_FN(world_physics_tick)
STUB_DATA(D_8002EB94) STUB_DATA(D_8010FFC0) STUB_DATA(D_80110244) STUB_DATA(D_80110248) STUB_DATA(D_80110270)
STUB_DATA(D_80110290) STUB_DATA(D_801102B4) STUB_DATA(D_801105B4) STUB_DATA(D_801105B8) STUB_DATA(D_801105BC)
STUB_DATA(D_801105C0) STUB_DATA(D_801105E4) STUB_DATA(D_8011062C) STUB_DATA(D_80110630) STUB_DATA(D_80110634)
STUB_DATA(D_80110638) STUB_DATA(D_8011063C) STUB_DATA(D_80110640) STUB_DATA(D_80110644) STUB_DATA(D_80110648)
STUB_DATA(D_8011064C) STUB_DATA(D_8011418C) STUB_DATA(D_80114654) STUB_DATA(D_80114700) STUB_DATA(D_80114724)
STUB_DATA(D_8011472C) STUB_DATA(D_801148A4) STUB_DATA(D_801164A4) STUB_DATA(D_801170FC) STUB_DATA(D_80117350)
STUB_DATA(D_80117354) STUB_DATA(D_801174B4) STUB_DATA(D_801174B8) STUB_DATA(D_8011EA18) STUB_DATA(D_8011F6F0)
STUB_DATA(D_8011F6F8) STUB_DATA(D_8011F6FC) STUB_DATA(D_801202F8) STUB_DATA(D_801247EC) STUB_DATA(D_8012E700)
STUB_DATA(D_801391F0) STUB_DATA(D_8013FECB) STUB_DATA(D_80140618) STUB_DATA(D_801406B8) STUB_DATA(D_80142528)
STUB_DATA(D_80142690) STUB_DATA(D_80142694) STUB_DATA(D_80142699) STUB_DATA(D_801426A0) STUB_DATA(D_801426F0)
STUB_DATA(D_80142750) STUB_DATA(D_80142764) STUB_DATA(D_80142780) STUB_DATA(D_801427A0) STUB_DATA(D_80142AD8)
STUB_DATA(D_80142D78) STUB_DATA(D_80142DC0) STUB_DATA(D_801439F8) STUB_DATA(D_80143CE8) STUB_DATA(D_80143EF0)
STUB_DATA(D_80143F18) STUB_DATA(D_80143FD8) STUB_DATA(D_80146108) STUB_DATA(D_80146115) STUB_DATA(D_801461A8)
STUB_DATA(D_801461D0) STUB_DATA(D_80149428) STUB_DATA(D_80149784) STUB_DATA(D_8014978C) STUB_DATA(D_80149D98)
STUB_DATA(D_8014A160) STUB_DATA(D_8014A250) STUB_DATA(D_80150DD8) STUB_DATA(D_80150E30) STUB_DATA(D_80150E88)
STUB_DATA(D_80150EB8) STUB_DATA(D_80150ED8) STUB_DATA(D_80150F00) STUB_DATA(D_80150F40) STUB_DATA(D_80151AD0)
STUB_DATA(D_80151AD8) STUB_DATA(D_80152038) STUB_DATA(D_8015256C) STUB_DATA(D_80152570) STUB_DATA(D_80152738)
STUB_DATA(D_80152744) STUB_DATA(D_8015274C) STUB_DATA(D_80152768) STUB_DATA(D_801527D8) STUB_DATA(D_80152808)
STUB_DATA(D_80153E84) STUB_DATA(D_80153F08) STUB_DATA(D_80153F24) STUB_DATA(D_80153F40)
STUB_DATA(D_80153FD2) STUB_DATA(D_80154188) STUB_DATA(D_8015418C) STUB_DATA(D_801541A4) STUB_DATA(D_801543C8)
STUB_DATA(D_801543CC) STUB_DATA(D_801543D0) STUB_DATA(D_801543D4) STUB_DATA(D_80154628) STUB_DATA(D_80154640)
STUB_DATA(D_80156958) STUB_DATA(D_8015698C) STUB_DATA(D_80156990) STUB_DATA(D_80156998) STUB_DATA(D_801569B8)
STUB_DATA(D_80156BAC) STUB_DATA(D_80156CE0) STUB_DATA(D_8015B254) STUB_DATA(D_8016139C) STUB_DATA(D_801613AC)
STUB_DATA(D_801613B4) STUB_DATA(D_801613B8) STUB_DATA(active_player_count) STUB_DATA(countdown_state) STUB_DATA(gCamera)
STUB_DATA(gControllerData) STUB_DATA(gFogColors) STUB_DATA(gHighScoreState) STUB_DATA(gNumModels) STUB_DATA(gNumWindows)
STUB_DATA(input_rec0) STUB_DATA(player_array) STUB_DATA(state_word_a)

/* ---------------------------------------------------------------- the game's functions */

struct msg *func_80091B00(void);
void func_800D6348(void);                                 /* 9: resume the paused effects */
void func_800D6E00(int32_t mono);                         /* 11: mono */
void sync_entry_register(int32_t song, int32_t loop);     /* 0: play a song */
void object_type1_create(void);                           /* 1: stop the song */
void object_type7_create(void);                           /* 7: stop every effect */
void object_type7_create_alt(void);
void speed_set(float blend, float amount, int32_t first, int32_t second);   /* 10: the volumes */
void us_AudKillSong(void);                                /* the audio screen's way out */
void di_post_type1(void);                                 /* its way in */
void post_msg_a(void);                                    /* updateAudioScreen's two */
void post_msg_b(void);
void AudKillSong_TTL(void);                               /* the high score screen's */
void AudKillSong(void);                                   /* display_list_flush.c's */
void AudPauseAllEffects(void);                            /* RaceStateMachine_Update.c's */
void AudResumeAllEffects(void);
void AudStopAllEffects(void);
void post_type1(void);                                    /* the audio shutdown's (carcolor's, their owner) */
void post_type7(void);
void func_800C8918(void);                                 /* the audio shutdown */
void func_800F87A0(void);                                 /* the game over screen's choice */
void finish_state_alt(void);                              /* the game over screen */
void func_800D5A04(void);                                 /* AbortGame */

static void sync_entry_register_3(void) { sync_entry_register(3, 1); }
static void func_800D6E00_1(void) { func_800D6E00(1); }
static void speed_set_half(void) { speed_set(0.5f, 0.25f, 1, 0); }

static void drain(void)
{
    for (int i = 0; i < 128; i++)
        D_80142DD8[i].used = 1, D_80142DD8[i].cmd = -1;
}
static int in_use(void)
{
    int n = 0;
    for (int i = 0; i < 128; i++)
        n += D_80142DD8[i].used != 0;
    return n;
}
static void reset(void)
{
    g_locks = g_nsent = g_queue_full = g_blocked = g_spins = g_drain_on_spin = 0;
    memset(g_sent, 0, sizeof g_sent);
}

/* ---------------------------------------------------------------- the posters: lock, take, fill, unlock, send */

static const struct { const char *name; void (*post)(void); int cmd; } POSTERS[] = {
    {"func_800D6348", func_800D6348, 9},          {"func_800D6E00", func_800D6E00_1, 11},
    {"sync_entry_register", sync_entry_register_3, 0}, {"object_type1_create", object_type1_create, 1},
    {"object_type7_create", object_type7_create, 7}, {"object_type7_create_alt", object_type7_create_alt, 7},
    {"speed_set", speed_set_half, 10},            {"us_AudKillSong", us_AudKillSong, 1},
    {"di_post_type1", di_post_type1, 1},          {"post_msg_a", post_msg_a, 1},
    {"post_msg_b", post_msg_b, 1},                {"AudKillSong_TTL", AudKillSong_TTL, 1},
    {"AudKillSong", AudKillSong, 1},              {"AudPauseAllEffects", AudPauseAllEffects, 7},
    {"AudResumeAllEffects", AudResumeAllEffects, 9}, {"AudStopAllEffects", AudStopAllEffects, 7},
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
        D_80142DD8[77].used = 0;
        g_queue_full = 1;
        POSTERS[k].post();
        CHECK(g_locks == 0 && g_nsent == 0 && D_80142DD8[77].used == 0 && in_use() == 127);
        /* and with room, the command goes */
        g_queue_full = 0;
        POSTERS[k].post();
        CHECK(g_locks == 0 && g_nsent == 1 && g_sent[0] == &D_80142DD8[77] && D_80142DD8[77].used == 1 &&
              D_80142DD8[77].cmd == POSTERS[k].cmd);
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
    D_80142DD8[5].used = 0;
    g_queue_full = 1;
    post_type7();
    CHECK(g_locks == 0 && g_spins == 0 && g_blocked == 1 && g_nsent == 1 && g_sent[0] == &D_80142DD8[5] &&
          D_80142DD8[5].cmd == 7);
    /* the whole shutdown from an empty pool: both posts get through, and its wait for the pool ends */
    reset();
    drain();
    g_drain_on_spin = 1;
    D_8011023C = 1;
    func_800C8918();
    CHECK(D_8011023C == 0 && g_locks == 0 && g_nsent == 2 && g_sent[0]->cmd == 1 && g_sent[1]->cmd == 7);
}

/* ---------------------------------------------------------------- the way out of a race: the rest still runs */

static void race_exits(void)
{
    /* the game over screen's "quit" (selection 1, a championship race): the stop is dropped, the state still moves */
    reset();
    drain();
    D_8015694C = 3, D_801391E0 = 1, gameplay_mode = 2, gSkipToGameState = 0, state_word_b = 0;
    func_800F87A0();
    CHECK(g_locks == 0 && g_nsent == 0 && in_use() == 128 && gSkipToGameState == 0x40000 && state_word_b == 4);
    /* selection 1 in another mode: its stop of every effect, then AbortGame's, both dropped; AbortGame still runs */
    reset();
    drain();
    D_8015694C = 3, D_801391E0 = 1, gameplay_mode = 0, D_80114650 = 0, state_word_b = 0;
    func_800F87A0();
    CHECK(g_locks == 0 && g_nsent == 0 && in_use() == 128 && D_80114650 == 1 && state_word_b == 0x40000);
    /* selections 2-6 (restart, the menus): the stop dropped, the next state set */
    reset();
    drain();
    D_8015694C = 3, D_801391E0 = 2, gSkipToGameState = 0, state_word_b = 0;
    func_800F87A0();
    CHECK(g_locks == 0 && g_nsent == 0 && gSkipToGameState == 0x40 && state_word_b == 4);
    /* with the queue full, the message is given back and the state still moves */
    reset();
    drain();
    D_80142DD8[9].used = 0;
    g_queue_full = 1;
    D_8015694C = 3, D_801391E0 = 3, gSkipToGameState = 0, state_word_b = 0;
    func_800F87A0();
    CHECK(g_locks == 0 && g_nsent == 0 && D_80142DD8[9].used == 0 && gSkipToGameState == 0x80 && state_word_b == 4);
    /* and with room it is sent as before */
    g_queue_full = 0;
    D_8015694C = 3, D_801391E0 = 4, gSkipToGameState = 0, state_word_b = 0;
    func_800F87A0();
    CHECK(g_locks == 0 && g_nsent == 1 && g_sent[0] == &D_80142DD8[9] && D_80142DD8[9].cmd == 1 &&
          gSkipToGameState == 0x100 && state_word_b == 4);

    /* the game over screen's soft reset: the stop dropped, the state set */
    reset();
    drain();
    D_80114728 = 1, D_80157244 = 1, gSkipToGameState = 0, state_word_b = 0;
    finish_state_alt();
    CHECK(g_locks == 0 && g_nsent == 0 && in_use() == 128 && gSkipToGameState == 1 && state_word_b == 4);

    /* AbortGame itself, restarting: the stop dropped, the restart state set */
    reset();
    drain();
    D_80114650 = 1, state_word_b = 0;
    func_800D5A04();
    CHECK(g_locks == 0 && g_nsent == 0 && in_use() == 128 && state_word_b == 0x40000);
    /* with the queue full, its message is given back */
    D_80142DD8[100].used = 0;
    g_queue_full = 1;
    state_word_b = 0;
    func_800D5A04();
    CHECK(g_locks == 0 && g_nsent == 0 && D_80142DD8[100].used == 0 && state_word_b == 0x40000);
    g_queue_full = 0;
    func_800D5A04();
    CHECK(g_locks == 0 && g_nsent == 1 && g_sent[0] == &D_80142DD8[100] && D_80142DD8[100].cmd == 7);
}

/* ---------------------------------------------------------------- the test hook */

static void hook(void)
{
    memset(D_80142DD8, 0, sizeof D_80142DD8);
    pc_audio_pool_fail_every = 3;
    struct msg *a = func_80091B00(), *b = func_80091B00(), *c = func_80091B00(), *d = func_80091B00();
    CHECK(a == &D_80142DD8[0] && b == &D_80142DD8[1] && c == NULL && d == &D_80142DD8[2] && in_use() == 3);
    pc_audio_pool_fail_every = 0;
    CHECK(func_80091B00() == &D_80142DD8[3] && func_80091B00() == &D_80142DD8[4] && func_80091B00() == &D_80142DD8[5]);
}

int main(void)
{
    posters();
    shutdown();
    race_exits();
    hook();
    return TEST_DONE();
}
