/* port/assets/tests/test_percar.c: the cars' sound records, checkpoint state and finish cameras (pc_engine_sounds,
 * pc_screech_sounds, pc_car_sounds_on, pc_engine_phase, pc_cp_last_pos, pc_cp_plane, pc_finish_cam,
 * pc_finish_cam_time: port/include/pc_game_types.h, docs/port/DECOUPLE.md 6) through the game's own code with 12
 * cars, more than the N64's 6, and more slots declared than the race walks. This test and its game objects are built
 * with PC_MAX_LINKS 16 (CMakeLists.txt). Every slot starts as a sentinel: the code must reach car k's record or
 * element, for k up to 11, and leave the slots after the race's cars alone.
 * StartCarSounds (func_800D5E64) sets 12 cars' engine tables and starts their two layers, a human's through
 * high_scores_display and a drone's through camera_target_track, and resets record 3 and the screech; the engine
 * update (func_800E05F0, func_800E0050) runs each car every other frame, alternating by pc_engine_phase, and sets
 * the layers' pitch and volume and a random sound in record 3; StopCarSounds (func_800D5524) releases records 0, 1
 * and 3 and the screech and sets the phase to (slot & 1) == 0. func_800FBE60 copies 12 cars' positions at the race's
 * start (m2c's 12-byte stride on the N64); CheckCPs (func_800F8EC8) takes each car's driven distance and checkpoint
 * plane and reports 12 crossings to race_countdown_display, which ends the race for the humans (owner 6) and turns
 * on their finish cameras, and calls CalcPlaces (func_800D1AB0), which ranks the 12 cars by distance with its slot
 * arrays; func_800E9E2C circles and then freezes a finish camera; func_800EB90C turns a window to it and places the
 * drone cameras of PC_RACE_CARS cars; render_post_process clears every finish camera, mode_flags_clear the first
 * two.
 * game: src/blob/groups/mode_select/d5e64.c src/blob/groups/mode_select/mode.c src/blob/func_800D5524.c
 * game: src/blob/player_conditional_call.c src/blob/func_800FBE60.c
 * game: src/blob/groups/car_checkpoints/car_checkpoints_func_800F8EC8.c
 * game: src/blob/groups/car_checkpoints/car_checkpoints_func_800D1AB0.c
 * game: src/blob/groups/race_countdown_display/race_countdown_display.c src/blob/func_800E9E2C.c
 * game: src/blob/func_800EB90C.c src/blob/render_post_process.c src/blob/mode_flags_clear.c
 */
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

#define CARS 12                  /* the race's */
#define SENTINEL 0xEE
#define CF(i, n, T) (*(T *)((uint8_t *)&pc_cars[i] + CAR_OFS(n)))   /* car i's field at N64 offset n */
#define MF(i, off, T) (*(T *)((uint8_t *)&pc_models[i] + (off)))      /* model i's field */

/* ---------------------------------------------------------------- the game's data the code under test uses */

struct pc_car pc_cars[PC_CAR_SLOTS];
struct pc_model pc_models[PC_MODEL_SLOTS];
struct pc_link pc_links[PC_LINK_SLOTS];
struct pc_engine_sound pc_engine_sounds[PC_MAX_LINKS];   /* data.c defines these in the game */
struct pc_sound_rec pc_screech_sounds[PC_MAX_LINKS];
signed char pc_car_sounds_on[PC_MAX_LINKS], pc_engine_phase[PC_MAX_LINKS];
float pc_cp_last_pos[PC_MAX_LINKS][3], pc_cp_plane[PC_MAX_LINKS];
unsigned char pc_finish_cam[PC_MAX_LINKS];
float pc_finish_cam_time[PC_MAX_LINKS];

/* The engine table D_8010FD80: per car type two 32-byte layers (d5e64.c's Definition32, mode.c's EngTbl). */
struct eng_layer { int32_t object; uint16_t r0, r1, b0, b1, b2; uint8_t padE[2]; float p0, p1, p2; uint8_t pad1C[4]; };
struct eng_layer D_8010FD80[13][2];
int8_t D_8010FFC0 = 1;
float D_801141B0[3];
struct { int32_t handle; float v[4]; } D_801406C0[4][3];   /* the humans' sounds (4 players) */
int32_t D_801407C0[4], D_801407E0[4], D_80140AE0[4];
uint8_t D_80140808[4][120];
int16_t D_80140A08[4];
float D_80140B10[4], D_80140BE0[4], D_80142518[4], D_80110020[4], D_8011F060[4] = {1, 1, 1, 1};
int8_t D_801115CD[4][13];
int16_t active_player_count = 1, gCountDown = 1;
int32_t gameplay_mode, D_80142728, D_8014A110, D_8011735C, D_8015697C, D_8015699C;
uint32_t state_word_a, D_801174B4;
uint8_t input_rec0[4 * 80] __attribute__((aligned(16)));   /* Player76: index +0, mask +4, edges +52 */
uint8_t D_8014A118;
volatile float D_8002EB90 = 50.0f, D_8002EB94 = 1.0f / 60.0f;
struct { float pos[3], normal[3]; int32_t radius; uint8_t pad[0x50 - 0x1C]; } D_80151CF4[4];   /* checkpoints */
int16_t D_80151CE8[6];   /* the checkpoint header: [2] the finish line, [3] the one before */
volatile int16_t D_801543CA, D_80153FD2 = 2;
uint8_t D_80152744;
int16_t D_80152734 = 3;   /* the race's laps */
int8_t D_80152014 = 3, D_80152015, D_80152031;
float D_80152018, D_801543CC, D_80152720[4], D_801106A4[4], D_801106B4[4], D_80152680[4], D_801526A8[4][3];
int8_t D_801525F8[4];
int16_t gNumWindows = 1;
struct camera { float uvs[3][3], pos[3]; uint8_t other48[48]; float out_uvs[3][3], out_pos[3], spring; uint8_t target,
                pad[3]; } gCamera[4], g_cams[PC_MAX_LINKS];   /* func_800EB90C's Camera, 152 bytes */
struct { uint32_t flags, w4; struct camera *cam; float scale, w; unsigned short model; int16_t child, next, mode;
         void *light, *tex[4], *rect, *clip, *pal; uint32_t prim, env; } D_8012E700[PC_MAX_LINKS];   /* Node68 */

/* ---------------------------------------------------------------- what the code under test calls */

static int g_nstart, g_nclip, g_nrelease, g_ncross;
static struct { int32_t handle; void *pos; float pitch, vol; } g_clip[64];
static int32_t g_released[64];
static struct { int32_t slot; float time; } g_cross[PC_MAX_LINKS];
static float g_res[3];
uint32_t high_scores_display(uint32_t object, uint32_t player, uint32_t a, uint8_t b, float c, float d, float e)
{
    g_nstart++;
    return object * 100 + player;
}
int32_t camera_target_track(void *pos, intptr_t ref, float a, float b, float c, float d, int32_t object,
                            int32_t player, int32_t e, uint32_t f)
{
    g_nstart++;
    return object * 100 + player;
}
void camera_clip_planes(int32_t handle, float *pos, float *ref, float pitch, float vol)
{
    if (g_nclip < 64)
        g_clip[g_nclip].handle = handle, g_clip[g_nclip].pos = pos, g_clip[g_nclip].pitch = pitch,
        g_clip[g_nclip].vol = vol;
    g_nclip++;
}
void results_screen_update(int32_t handle) { if (g_nrelease < 64) g_released[g_nrelease] = handle; g_nrelease++; }
void scheduler_recv(int32_t handle) { if (g_nrelease < 64) g_released[g_nrelease] = handle; g_nrelease++; }
static int g_nsync;
static int32_t g_sync;
void client_sync(int32_t h, float v) { if (h == 7036) g_sync = h, g_nsync++; }   /* the screech's level */
void entity_transform_calc(int32_t h, float x, float y, float z, float w) {}
void entity_hierarchy_update(int32_t h, float v) {}   /* an inline in mode.c, its owner elsewhere in the game */
int32_t osRecvMesg(void *q, void *m, int32_t f) { return 0; }
int32_t osSendMesg(void *q, void *m, int32_t f) { return 0; }
int32_t frame_sync(int32_t a, int32_t b, int32_t c, int32_t d) { return 7000 + a; }
float func_8008B2E4(float max) { return 0.0f; }   /* Random: 0, so a drone's random sound starts */
int32_t leaderboard_update(int32_t handle) { return 1; }   /* still playing */
void mode_select_handler(void *m) {}
float func_8008B3C8(float *v) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
int32_t func_800B61A8(int32_t a, int32_t b, int32_t c, uint8_t d) { return -1; }
void world_gravity_apply(void) {}
int32_t func_800B930C(int32_t cp) { return cp + 1; }
void func_800D2054(int32_t slot, float time)
{
    if (g_ncross < PC_MAX_LINKS)
        g_cross[g_ncross].slot = slot, g_cross[g_ncross].time = time;
    g_ncross++;
}
void func_800D2458(int32_t player, float time) {}
void car_setup_confirm(int32_t player, float time) {}
uint32_t car_stats_display(int32_t type, float delay, int32_t cb, int32_t data, int32_t nargs, ...) { return 0; }
int32_t entity_flags_apply(int32_t a, int32_t b, int32_t c, uint8_t d) { return 0; }
float n64_sinf(float x) { return sinf(x); }
float n64_cosf(float x) { return cosf(x); }
void func_800E8D50(void *object, float *carpos, void *matrix, float *res)
{
    g_res[0] = res[0], g_res[1] = res[1], g_res[2] = res[2];
}
void func_800E9234(void *b) {}
void func_800FAD50(void *b) {}
void audio_dsp_process(void) {}
void func_800CC8C8(void *handle, uint8_t view) {}
void func_800CFCA8(void) {}
void func_800EB028(void *object) {}
void world_object_destroy(void *object) {}
float viGetTimeToDeadline(void) { return 10.0f; }
void math_utility(void *src, void *dst) { memcpy(dst, src, 36); }

void func_800D5E64(void *vehicle);
void func_800E05F0(void *model);
void func_800D5524(void *model);
void func_800FBE60(void);
void func_800F8EC8(void);
void func_800E9E2C(void *object, float *carpos, void *matrix);
void func_800EB90C(void);
void render_post_process(void);
void mode_flags_clear(void);

/* ---------------------------------------------------------------- helpers */

static int all_bytes(const void *p, size_t n, int v)
{
    for (size_t i = 0; i < n; i++)
        if (((const uint8_t *)p)[i] != v)
            return 0;
    return 1;
}

static int reset_rec(const struct pc_sound_rec *r)   /* player_conditional_call's -1 and four -2.0f */
{
    return r->handle == -1 && r->level == -2.0f && r->style == -2.0f && r->f0C == -2.0f && r->f10 == -2.0f;
}

static int near(float a, float b) { return fabsf(a - b) <= 1e-5f * (1.0f + fabsf(b)); }

static int type_of(int k) { return (k * 5 + 1) % 13; }   /* a car type each, 12 of the 13 */

static void sentinels(void)
{
    memset(pc_cars, SENTINEL, sizeof pc_cars);
    memset(pc_models, SENTINEL, sizeof pc_models);
    memset(pc_links, SENTINEL, sizeof pc_links);
    memset(pc_engine_sounds, SENTINEL, sizeof pc_engine_sounds);
    memset(pc_screech_sounds, SENTINEL, sizeof pc_screech_sounds);
    memset(pc_car_sounds_on, SENTINEL, sizeof pc_car_sounds_on);
    memset(pc_engine_phase, SENTINEL, sizeof pc_engine_phase);
    memset(pc_cp_last_pos, SENTINEL, sizeof pc_cp_last_pos);
    memset(pc_cp_plane, SENTINEL, sizeof pc_cp_plane);
    memset(pc_finish_cam, SENTINEL, sizeof pc_finish_cam);
    memset(pc_finish_cam_time, SENTINEL, sizeof pc_finish_cam_time);
}

/* ---------------------------------------------------------------- the sounds */

static void test_sounds(void)
{
    sentinels();
    for (int t = 0; t < 13; t++)
        for (int l = 0; l < 2; l++) {
            struct eng_layer *e = &D_8010FD80[t][l];
            e->object = 10 * t + l + 1;
            e->r0 = e->r1 = 1000, e->b0 = 2000, e->b1 = 3000, e->b2 = 4000;
            e->p0 = 1.0f, e->p1 = 2.0f, e->p2 = 3.0f;
        }
    /* car 0 a human (mode 2), the other 11 drones; each car's model record: its slot, type, mode, engine */
    for (int k = 0; k < CARS; k++) {
        PC_MODEL_SLOT(k) = (short)k;
        MF(k, 0xC, int8_t) = (int8_t)type_of(k);
        PC_MODEL_CRASHFLAG(k) = 0;
        PC_MODEL_DRONE_TYPE(k) = k == 0 ? 2 : 1;
        PC_MODEL_S7D0(k) = (short)(100 * (k + 1));   /* the rpm */
        MF(k, 0x404, float) = 700.0f;                 /* the load: (700 + 200) / 900, a factor of 1 */
        for (int w = 0; w < 4; w++)
            PC_MODEL_VISCODE(k)[w] = 1, MF(k, 0x7F8 + 4 * w, float) = 0.0f;
        MF(k, 0x65C, int16_t) = 0;
        MF(k, 0x75A, int16_t) = 0;                    /* the steering */
    }
    PC_MODEL_VISCODE(0)[0] = 0, MF(0, 0x7F8, float) = 1.0f;   /* the human's tire 0 on the road, sliding: a screech */
    memset(input_rec0, 0, sizeof input_rec0);
    g_nstart = 0;
    for (int k = 0; k < CARS; k++)
        func_800D5E64(PC_MODEL(k));
    CHECK(g_nstart == 2 * CARS);
    for (int k = 0; k < CARS; k++) {
        struct pc_engine_sound *s = &pc_engine_sounds[k];
        CHECK(s->table == (void *)D_8010FD80[type_of(k)]);
        for (int l = 0; l < 2; l++)
            CHECK(s->rec[l].handle == (10 * type_of(k) + l + 1) * 100 + k && s->rec[l].level == -2.0f);
        CHECK(all_bytes(&s->rec[2], sizeof s->rec[2], SENTINEL));   /* record 2: never used */
        CHECK(reset_rec(&s->rec[3]) && reset_rec(&pc_screech_sounds[k]) && pc_car_sounds_on[k] == 1);
    }
    for (int k = CARS; k < PC_MAX_LINKS; k++) {
        CHECK(all_bytes(&pc_engine_sounds[k], sizeof pc_engine_sounds[k], SENTINEL));
        CHECK(all_bytes(&pc_screech_sounds[k], sizeof pc_screech_sounds[k], SENTINEL));
        CHECK(all_bytes(&pc_car_sounds_on[k], 1, SENTINEL));
    }

    /* The engine update, two frames: the phase the image gives (odd slots 1) skips each car one frame of two. A
     * drone's run starts its random sound in record 3 (camera_target_track's object 98) and sets the two layers. */
    for (int k = 0; k < PC_MAX_LINKS; k++)
        pc_engine_phase[k] = k < CARS ? (signed char)(k & 1) : (signed char)SENTINEL;
    for (int frame = 0; frame < 2; frame++) {
        g_nclip = 0;
        for (int k = 0; k < CARS; k++)
            func_800E05F0(PC_MODEL(k));
        int n = 0;
        for (int k = 1; k < CARS; k++) {
            int runs = (k & 1) == frame;
            CHECK(pc_engine_phase[k] == (runs ? 1 : 0));
            if (!runs)
                continue;
            float rpm = 100.0f * (k + 1) < 890.0f ? 100.0f * (k + 1) : 890.0f;   /* a drone's is capped */
            float vol = (1000.0f / 1000.0f) * (rpm / 1000.0f - 1.0f) + 1.0f;
            CHECK(pc_engine_sounds[k].rec[3].handle == 98 * 100 + k);
            CHECK(n + 3 <= g_nclip && g_clip[n].handle == 98 * 100 + k && g_clip[n].pitch == 0.8f);
            for (int l = 0; l < 2 && n + 1 + l < g_nclip; l++) {
                CHECK(g_clip[n + 1 + l].handle == pc_engine_sounds[k].rec[l].handle);
                CHECK(g_clip[n + 1 + l].pos == PC_MODEL_RWR(k) && near(g_clip[n + 1 + l].pitch, 0.75f));
                CHECK(near(g_clip[n + 1 + l].vol, vol) && near(pc_engine_sounds[k].rec[l].style, vol));
            }
            n += 3;
        }
        CHECK(n == g_nclip);
        CHECK(pc_engine_phase[0] == (frame == 0 ? 1 : 0));   /* the human alternates too */
        /* the human's screech started (frame_sync 36) and its level and style sent: 1 x 0.6, 1 - 1 / 2 + 1 */
        CHECK(pc_screech_sounds[0].handle == 7036 && near(pc_screech_sounds[0].level, 0.6f));
        CHECK(near(pc_screech_sounds[0].style, 1.5f) && g_nsync == 1 && g_sync == 7036);
        for (int k = CARS; k < PC_MAX_LINKS; k++)
            CHECK(all_bytes(&pc_engine_phase[k], 1, SENTINEL) &&
                  all_bytes(&pc_engine_sounds[k], sizeof pc_engine_sounds[k], SENTINEL));
    }

    /* StopCarSounds: records 0, 1 and 3 and the screech released (a drone's through results_screen_update, the
     * human's through scheduler_recv: both recorded) and reset; the flag cleared; the phase (slot & 1) == 0. */
    g_nrelease = 0;
    for (int k = 0; k < CARS; k++) {
        int32_t h0 = pc_engine_sounds[k].rec[0].handle, h1 = pc_engine_sounds[k].rec[1].handle;
        int32_t h3 = pc_engine_sounds[k].rec[3].handle;
        int at = g_nrelease;
        func_800D5524(PC_MODEL(k));
        CHECK(g_released[at] == h0 && g_released[at + 1] == h1);
        if (k == 0)   /* the human's record 3 never started: the screech next */
            CHECK(h3 == -1 && g_released[at + 2] == 7036);
        else
            CHECK(h3 == 98 * 100 + k && g_released[at + 2] == h3 && g_released[at + 3] == -1);
        CHECK(reset_rec(&pc_engine_sounds[k].rec[0]) && reset_rec(&pc_engine_sounds[k].rec[1]));
        CHECK(reset_rec(&pc_engine_sounds[k].rec[3]) && reset_rec(&pc_screech_sounds[k]));
        CHECK(all_bytes(&pc_engine_sounds[k].rec[2], sizeof pc_engine_sounds[k].rec[2], SENTINEL));
        CHECK(pc_car_sounds_on[k] == 0 && pc_engine_phase[k] == ((k & 1) == 0));
    }
    for (int k = CARS; k < PC_MAX_LINKS; k++)
        CHECK(all_bytes(&pc_engine_sounds[k], sizeof pc_engine_sounds[k], SENTINEL) &&
              all_bytes(&pc_car_sounds_on[k], 1, SENTINEL) && all_bytes(&pc_engine_phase[k], 1, SENTINEL));
}

/* ---------------------------------------------------------------- the checkpoints and places */

static int is_human(int k) { return k == 0 || k == 9; }   /* owner 6: a slot past the N64's 6 among them */
static int rank_of(int k) { return 11 - (5 * k) % 12; }   /* the distances below, the largest first */

static void test_checkpoints(void)
{
    sentinels();
    /* The race's start: func_800FBE60 copies each of D_80152744 cars' position and clears its +0x108. */
    for (int k = 0; k < PC_CAR_SLOTS; k++)
        PC_CAR_POS(k)[0] = (float)k, PC_CAR_POS(k)[1] = 0.0f, PC_CAR_POS(k)[2] = 5.0f;
    D_80152744 = CARS;
    func_800FBE60();
    for (int k = 0; k < CARS; k++)
        CHECK(pc_cp_last_pos[k][0] == (float)k && pc_cp_last_pos[k][1] == 0.0f && pc_cp_last_pos[k][2] == 5.0f &&
              PC_CAR_F108(k) == 0.0f);
    for (int k = CARS; k < PC_MAX_LINKS; k++)
        CHECK(all_bytes(pc_cp_last_pos[k], sizeof pc_cp_last_pos[k], SENTINEL));

    /* A frame later each car is 10 feet on, across checkpoint 0's plane (x = 0, facing +x), which ends the last lap.
     * CheckCPs: the distance driven, the position kept, the plane's side and distance, and the crossing reported at
     * the interpolated time; race_countdown_display finishes the race for the humans, who get the finish camera;
     * CalcPlaces ranks the 12 by their distance along the track. */
    for (int k = 0; k < CARS; k++) {
        PC_CAR_POS(k)[0] = (float)k + 10.0f;
        PC_LINK_OWNER(k) = is_human(k) ? 6 : 0;
        CF(k, 0x004, float) = 100.0f + (float)k;    /* the car's clock */
        PC_CAR_B358(k) = 0, PC_CAR_STATE(k) = 0;    /* not crashed; racing */
        PC_CAR_FINISHED(k) = 0, PC_CAR_PLACE(k) = 1;
        PC_CAR_DISTANCE(k) = 1000.0f + 37.0f * (float)((5 * k) % 12);
        PC_MODEL_SLOT(k) = (short)k, PC_MODEL_MOVING_STATE(k) = -1, PC_MODEL_FLAGS(k) = 0xFF;
        MF(k, 0x7E4, int16_t) = 0;                  /* the next checkpoint */
        MF(k, 0x7E9, int8_t) = -1;                  /* behind its plane */
        MF(k, 0x7DC, int8_t) = 1;                   /* laps count */
        PC_MODEL_LAP(k) = 2;
        pc_cp_plane[k] = -(float)(k + 1);
    }
    D_80151CF4[0].pos[0] = 0.0f, D_80151CF4[0].pos[1] = 0.0f, D_80151CF4[0].pos[2] = 5.0f;
    D_80151CF4[0].normal[0] = 1.0f, D_80151CF4[0].normal[1] = 0.0f, D_80151CF4[0].normal[2] = 0.0f;
    D_80151CF4[0].radius = 10000;
    D_80151CE8[2] = 0, D_80151CE8[3] = 5;           /* the finish line is checkpoint 0 */
    D_801543CA = CARS, D_80152744 = CARS, D_8014A110 = 0, D_80152014 = 3;
    g_ncross = 0;
    func_800F8EC8();
    CHECK(g_ncross == CARS);
    for (int k = 0; k < CARS; k++) {
        float plane = (float)k + 10.0f, cent = plane / (plane - -(float)(k + 1)) * (1.0f / 60.0f);
        CHECK(PC_CAR_F108(k) == 10.0f && pc_cp_last_pos[k][0] == (float)k + 10.0f && pc_cp_last_pos[k][2] == 5.0f);
        CHECK(MF(k, 0x7E9, int8_t) == 1 && pc_cp_plane[k] == plane);
        CHECK(g_cross[k].slot == k && near(g_cross[k].time, 100.0f + (float)k - cent));
        CHECK(PC_MODEL_LAP(k) == 3 && PC_MODEL_LAST_CP(k) == 0 && MF(k, 0x7E4, int16_t) == 1);
        CHECK(PC_CAR_PLACE(k) == rank_of(k));
        if (is_human(k))
            CHECK(pc_finish_cam[k] == 1 && pc_finish_cam_time[k] == 50.0f);
        else
            CHECK(all_bytes(&pc_finish_cam[k], 1, SENTINEL) && all_bytes(&pc_finish_cam_time[k], 4, SENTINEL));
    }
    for (int k = CARS; k < PC_MAX_LINKS; k++) {
        CHECK(all_bytes(pc_cp_last_pos[k], sizeof pc_cp_last_pos[k], SENTINEL));
        CHECK(all_bytes(&pc_cp_plane[k], 4, SENTINEL) && all_bytes(&pc_finish_cam[k], 1, SENTINEL));
        CHECK(all_bytes(&PC_CAR_PLACE(k), 1, SENTINEL));
    }
}

/* ---------------------------------------------------------------- the finish cameras */

static void test_cameras(void)
{
    /* func_800E9E2C circles car 9's camera with the clock, then 10 s after the finish freezes it there. */
    PC_CAR_SLOT(9) = 9, PC_CAR_VIEW(9) = 0;
    PC_CAR_SLOT(0) = 0, PC_CAR_VIEW(0) = 0;
    D_801543CC = 61.0f;
    func_800E9E2C(PC_CAR(9), PC_CAR_POS(9), NULL);
    float ang = (float)(61000u % 6000) * (1.0f / 6000) * 6.2831855f;
    CHECK(pc_finish_cam[9] == 2 && pc_finish_cam_time[9] == 61.0f);
    CHECK(g_res[1] == 10.0f && near(g_res[0], cosf(ang) * 15.0f) && near(g_res[2], sinf(ang) * 15.0f));
    float res0 = g_res[0], res2 = g_res[2];
    D_801543CC = 75.0f;
    func_800E9E2C(PC_CAR(9), PC_CAR_POS(9), NULL);
    CHECK(g_res[0] == res0 && g_res[2] == res2 && pc_finish_cam_time[9] == 61.0f);   /* frozen */
    CHECK(pc_finish_cam[0] == 1 && pc_finish_cam_time[0] == 50.0f);                     /* car 0's untouched */

    /* func_800EB90C: the window following car 9, whose player has the finish camera, turns to view 5; then every
     * car the race holds but the humans and the hidden gets its drone camera on it. */
    gNumWindows = 1, gCamera[0].target = 9, input_rec0[0] = 9;
    CF(9, 0x35D, int8_t) = 0;                        /* its view */
    for (int k = 0; k < 4; k++)
        MF(9, 0x5EC + 4 * k, float) = 10.0f;         /* no tire compressed: no suspension average */
    for (int j = 0; j < PC_MAX_LINKS; j++) {
        PC_MODEL_ACTIVE(j) = 1, PC_MODEL_HIDE_CAR(j) = 0, PC_MODEL_DRONE_TYPE(j) = is_human(j) ? 2 : 1;
        PC_CAR_NODE(j) = j;
        PC_CAR_POS(j)[0] = 10.0f + (float)j, PC_CAR_POS(j)[1] = 0.0f, PC_CAR_POS(j)[2] = 5.0f;
        for (int e = 0; e < 9; e++)
            PC_CAR_UVS50(j)[e / 3][e % 3] = (float)(j * 10 + e);
        D_8012E700[j].cam = &g_cams[j];
    }
    for (int pass = 0; pass < 2; pass++) {
        test_race_cars = pass == 0 ? CARS : 6;
        memset(g_cams, SENTINEL, sizeof g_cams);
        func_800EB90C();
        CHECK(CF(9, 0x35D, int8_t) == 5 && CF(9, 0x35E, int8_t) == 5);
        for (int j = 0; j < PC_MAX_LINKS; j++) {
            if (j < test_race_cars && !is_human(j))
                CHECK(g_cams[j].pos[0] == 10.0f + (float)j && g_cams[j].pos[1] == 0.25f && g_cams[j].pos[2] == 5.0f &&
                      memcmp(g_cams[j].uvs, PC_CAR_UVS50(j), 36) == 0);
            else
                CHECK(all_bytes(&g_cams[j], sizeof g_cams[j], SENTINEL));
        }
    }
    test_race_cars = 6;

    /* render_post_process clears every slot's finish camera; mode_flags_clear the first two. */
    gCamera[0].target = 9;
    render_post_process();
    CHECK(all_bytes(pc_finish_cam, sizeof pc_finish_cam, 0));
    memset(pc_finish_cam, 1, sizeof pc_finish_cam);
    mode_flags_clear();
    CHECK(pc_finish_cam[0] == 0 && pc_finish_cam[1] == 0);
    for (int k = 2; k < PC_MAX_LINKS; k++)
        CHECK(pc_finish_cam[k] == 1);
}

int main(void)
{
    CHECK(PC_MAX_LINKS == 16 && PC_N64_CARS == 6);   /* this test's build */
    CHECK(sizeof(struct pc_engine_sound) == 8 + 4 * 20 && sizeof(struct pc_sound_rec) == 20);
    CHECK(offsetof(struct pc_engine_sound, rec) == 8 && offsetof(struct pc_sound_rec, style) == 8);
    test_sounds();
    test_checkpoints();
    test_cameras();
    return TEST_DONE();
}
