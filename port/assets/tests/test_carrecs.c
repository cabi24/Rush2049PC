/* port/assets/tests/test_carrecs.c: the car records (MAT3 gGameCar, gGameCar: pc_cars, struct pc_car,
 * port/include/pc_game_types.h) through the game's own readers and writers, with more records declared than the
 * game walks (docs/port/DECOUPLE.md 4). This test and its game objects are built with PC_MAX_LINKS 16
 * (CMakeLists.txt), so the storage has 16 records where the game build has 12 and the N64 6. Every record starts as
 * a sentinel: the code must write and read the fields at their PC offsets (CAR_OFS of the N64 ones), step 0x468
 * between records, and stop where it stops on the N64, whatever the storage's size.
 * InitCPTime clears the race start's +0x108 and copies the positions of gNumActiveCars cars, at m2c's byte strides
 * of CAR_OFS(952) from the array; DetectDroneAbort clears 6 cars' progress and heading (an index walk), and
 * then reaches a player's car through its input record's node, a record pointer in its own view, where
 * AreWeSolid reads the car's +0x358 (a stride of 0x3B8 on the N64, through a label of its own);
 * MissileLock (image B's homing) walks the cars of the race from the array to its end, gGameCar +
 * gNumCars on the N64; initialize resets a car's battle state through the model's slot; Update_MDrive hands
 * cars 1 to 3 to the cameras through what were car1_objnum, car1_dr_pos and car1_dr_car_uvs, car 1's +0, +8 and +0x50;
 * gameover_validOption reads a car's finish, +0xEF with a stride of 0x3B8 (another label on the N64).
 * A race of more cars (docs/port/MORECARS.md 3.1), test_race_cars 12 (what pc_race_cars() returns, support.c):
 * DetectDroneAbort clears 12 cars' progress; CarReportsGameOver locks 12 cars' places in
 * the order they finish, and PlaceUnlockedCars (CalcPlaces) orders the rest by distance, with locals for 12;
 * SetCatchup (set_catchup) scales the 12 cars' catch-up from the one ahead.
 * game: src/blob/InitCPTime.c src/blob/DetectDroneAbort.c src/blob/AreWeSolid.c
 * game: src/ovl_b/groups/missile_lock/group.c src/blob/initialize.c src/blob/Update_MDrive.c
 * game: src/blob/gameover_validOption.c src/blob/groups/car_checkpoints/car_checkpoints_car_setup_confirm.c
 * game: src/blob/groups/car_checkpoints/car_checkpoints_func_800D1AB0.c src/blob/SetCatchup.c
 */
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

/* ---------------------------------------------------------------- the game's data the code under test uses */

struct pc_car pc_cars[PC_CAR_SLOTS];        /* data.c defines it in the game */
struct pc_model pc_models[PC_MODEL_SLOTS];  /* the model records (N64 layout) */
struct pc_link pc_links[PC_LINK_SLOTS];     /* the race roster */
#define SENTINEL 0xEE
#define RAW(i, off) (((const unsigned char *)pc_cars)[(i) * PC_CAR_SIZE + (off)])
#define CF(i, n, T) (*(T *)((uint8_t *)&pc_cars[i] + CAR_OFS(n)))   /* car i's field at N64 offset n */
#define MF(i, off, T) (*(T *)((uint8_t *)&pc_models[i] + (off)))      /* model i's field */

int32_t gRace, gRace;
int16_t gNumPlayers, gNumPlayers;
volatile int16_t gNumCars;
uint8_t gNumActiveCars, gCircuitPlayer;
float gModelTime, gFirstPlaceTime, pc_cp_last_pos[PC_MAX_LINKS][3];
int8_t gInFirstPlace, gNoAutoAbort, gEndGame, gCoastFlag, gSuperSpeed, gCarTeam[PC_CAR_SLOTS];
uint32_t gGameState;
uint8_t gPlayers[6 * 80] __attribute__((aligned(16)));          /* the input records: node at +0 */
struct { uint8_t pad[4]; void *pts; uint8_t pad8[4]; void *branch; } gResPath;
float unitmatrix[9], model_step_dt = 1.0f, initial_thump_threshold = 2.0f, super_speed_unit_diagonal = 3.0f;
int16_t gNumLaps;                 /* the race's laps */
float gLapDistance;
int8_t gDifficulty, gUseCatchup;      /* the drones' level, the catch-up's kind */
float GetCountdownTimeRemaining(void) { return 0.0f; }
void AddCountdownTime(float t) { (void)t; }

/* What the code under test calls: each records what it was given. */
static float g_turn[2];
static int g_nturns, g_ncams, g_nstuck;
static struct { int16_t node; void *pos, *uvs; } g_cam[8];
void AbortCar(int16_t node, int32_t arg1) { (void)node; (void)arg1; }
void SetResurrectionData(int32_t what, void *m, int32_t arg2) { (void)what; (void)m; (void)arg2; g_nstuck++; }
void NextResPathPt(int32_t idx, int32_t seg, int32_t *nidx, int32_t *nseg) { *nidx = idx; *nseg = seg; }
void BodyVector(float *in, float *out, void *uvs) { (void)uvs; out[0] = in[0], out[1] = in[1], out[2] = in[2]; }
float fatan2(float x, float z) { return x + z; }
void YawUV(float a, void *uv) { (void)uv; if (g_nturns < 2) g_turn[g_nturns] = a; g_nturns++; }
void PitchUV(float a, void *uv) { (void)uv; if (g_nturns < 2) g_turn[g_nturns] = a; g_nturns++; }
void CopyUV(void *src, void *dst) { (void)src; (void)dst; }
void carsetup(void *m) { (void)m; }
void init_road(void *m) { (void)m; }
void init_collision(void *m) { (void)m; }
void update_game_data(void) {}
int32_t CheckCameraView(void) { return 0; }
void ZOID_UpdateObject(int16_t node, void *pos, void *uvs)
{
    if (g_ncams < 8)
        g_cam[g_ncams].node = node, g_cam[g_ncams].pos = pos, g_cam[g_ncams].uvs = uvs;
    g_ncams++;
}

void InitCPTime(void);
void DetectDroneAbort(void);
int32_t AreWeSolid(int16_t arg0);
void MissileLock(void *object);
void initialize(void *car);
void Update_MDrive(void);
int32_t gameover_validOption(int32_t arg0);
void CarReportsGameOver(int32_t slot, float score);
void PlaceUnlockedCars(void);
void SetCatchup(void);

static void sentinels(void)
{
    memset(pc_cars, SENTINEL, sizeof pc_cars);
    memset(pc_models, SENTINEL, sizeof pc_models);
    g_nturns = g_ncams = g_nstuck = 0;
}

/* Records from `from` on hold the sentinel in every byte. */
static int untouched_from(int from)
{
    for (int i = from; i < PC_CAR_SLOTS; i++)
        for (int k = 0; k < PC_CAR_SIZE; k++)
            if (RAW(i, k) != SENTINEL)
                return 0;
    return 1;
}

/* Record i holds the sentinel everywhere but the N64 ranges [lo, hi) of `ranges` (PC offsets through CAR_OFS). */
static int only(int i, const int *ranges, int n)
{
    for (int k = 0; k < PC_CAR_SIZE; k++) {
        int in = 0;
        for (int r = 0; r < n; r++)
            in |= k >= CAR_OFS(ranges[2 * r]) && k < CAR_OFS(ranges[2 * r + 1] - 1) + 1;
        if (!in && RAW(i, k) != SENTINEL)
            return 0;
    }
    return 1;
}

static void test_layout(void)
{
    CHECK(PC_CAR_SLOTS == 16 && PC_MAX_LINKS == 16 && PC_N64_CAR_SLOTS == 6);   /* this test's build */
    CHECK(sizeof(struct pc_car) == PC_CAR_SIZE && sizeof pc_cars == 16 * PC_CAR_SIZE);
#define AT(f, n) (offsetof(struct pc_car, f) == CAR_OFS(n))
    CHECK(AT(pos, 0x008) && AT(uvs, 0x02C) && AT(uvs50, 0x050) && AT(wheel_pos, 0x074) && AT(flags, 0x0E8));
    CHECK(AT(place, 0x0EE) && AT(finished, 0x0EF) && AT(time, 0x0F0) && AT(node, 0x0F4) && AT(mph, 0x0F8));
    CHECK(AT(distance, 0x100) && AT(f108, 0x108) && AT(visuals, 0x110) && AT(nav, 0x314));
    CHECK(AT(fwd_progress, 0x340) && AT(ir_color, 0x34C) && AT(prev_ir_color, 0x350) && AT(weight_index, 0x356));
    CHECK(AT(b358, 0x358) && AT(state, 0x359) && AT(disabled, 0x35A) && AT(slot, 0x35B) && AT(view, 0x35C));
    CHECK(AT(sheen_alpha, 0x360) && AT(heading, 0x364) && AT(input, 0x380) && AT(kind, 0x384));
    CHECK(AT(lives, 0x385) && AT(health, 0x386) && AT(battle_flags, 0x38C) && AT(b3A0, 0x3A0));
    CHECK(AT(b3A2, 0x3A2) && AT(points, 0x3A3) && AT(reloading_time, 0x3AC) && AT(f3B0, 0x3B0) && AT(f3B4, 0x3B4));
#undef AT
    CHECK(offsetof(struct pc_car, visuals[20]) == CAR_OFS(0x110 + 20 * 0x18) && sizeof(struct pc_visual) == 0x20);
    CHECK(PC_CAR(3) == (void *)((uint8_t *)pc_cars + 3 * 0x468) && PC_CARS == (void *)pc_cars);
    CHECK(PC_CARS_END(6) == (void *)((uint8_t *)pc_cars + 6 * 0x468));
}

/* InitCPTime, the race start: for each of gNumActiveCars cars, +0x108 cleared and the position (+0x8) copied into
 * pc_cp_last_pos[i] (MAT3 checkpoint.c's gPrevPos), at m2c's strides of CAR_OFS(952) from the array; nothing else. */
static void test_start(void)
{
    static const int f108[] = {0x108, 0x10C};
    sentinels();
    for (int i = 0; i < PC_CAR_SLOTS; i++)
        PC_CAR_POS(i)[0] = 10.0f * i, PC_CAR_POS(i)[1] = 1.0f + i, PC_CAR_POS(i)[2] = -2.0f * i;
    memset(pc_cp_last_pos, 0, sizeof pc_cp_last_pos);
    gNumActiveCars = 6;
    InitCPTime();
    for (int i = 0; i < 6; i++) {
        CHECK(pc_cp_last_pos[i][0] == 10.0f * i && pc_cp_last_pos[i][1] == 1.0f + i);
        CHECK(pc_cp_last_pos[i][2] == -2.0f * i);
        CHECK(CF(i, 0x108, float) == 0.0f && PC_CAR_F108(i) == 0.0f);
        memset(&PC_CAR_POS(i), SENTINEL, 12);                                      /* set above */
        CHECK(only(i, f108, 1));
    }
    for (int i = 6; i < PC_CAR_SLOTS; i++)
        memset(&PC_CAR_POS(i), SENTINEL, 12);
    CHECK(untouched_from(6) && gFirstPlaceTime == 0 && gInFirstPlace == 0);
}

/* DetectDroneAbort: before 5 seconds of model time, the 6 cars' progress (+0x340) and heading (+0x364)
 * cleared and the models' stuck time; then car 9 (a slot the storage has past the N64's 6), reached through player
 * 0's input record: moving forward, its progress is cleared and its distance kept; stopped and not drivable (its
 * +0x358 set, AreWeSolid), cleared too; stopped and drivable, its progress is the model's time. */
static void test_remap(void)
{
    static const int progress[] = {0x340, 0x344, 0x364, 0x368};
    static const int moved[] = {0x104, 0x108, 0x340, 0x344, 0x364, 0x368};
    sentinels();
    gModelTime = 1.0f;
    DetectDroneAbort();
    for (int i = 0; i < 6; i++) {
        CHECK(PC_CAR_FWD_PROGRESS(i) == 0.0f && PC_CAR_HEADING(i) == 0.0f && only(i, progress, 2));
        CHECK(PC_MODEL_STUCK_TIME(i) == 0.0f);
    }
    CHECK(untouched_from(6) && MF(6, 0x7D8, uint32_t) == 0xEEEEEEEEu);

    sentinels();
    gModelTime = 10.0f, gNumActiveCars = 0, gNumPlayers = 1, gPlayers[0] = 9;
    MF(9, 0x7C6, int16_t) = 9, MF(9, 0x714, float) = 7.5f;                          /* model 9: slot, time */
    CF(9, 0x100, float) = 50.0f, CF(9, 0x104, float) = 10.0f;                       /* distance > the last */
    DetectDroneAbort();
    CHECK(CF(9, 0x340, float) == 0.0f && CF(9, 0x104, float) == 50.0f && CF(9, 0x364, float) == 0.0f);
    memset(&CF(9, 0x100, float), SENTINEL, 4);                                      /* set above */
    CHECK(only(9, moved, 3) && untouched_from(10));
    for (int i = 0; i < 9; i++)
        CHECK(only(i, NULL, 0));

    sentinels();                                                                    /* stopped, +0x358 set */
    MF(9, 0x7C6, int16_t) = 9, MF(9, 0x714, float) = 7.5f;
    CF(9, 0x100, float) = 10.0f, CF(9, 0x104, float) = 10.0f, CF(9, 0x340, float) = 3.0f;
    CF(9, 0x358, int8_t) = 1;
    CHECK(AreWeSolid(9) == 0);
    DetectDroneAbort();
    CHECK(CF(9, 0x340, float) == 0.0f && CF(9, 0x364, float) == 0.0f);

    CF(9, 0x358, int8_t) = 0, CF(9, 0x340, float) = 0.0f;                          /* drivable */
    MF(9, 0x640, int8_t) = 0, MF(9, 0x6C4, int16_t) = -1, MF(9, 0x7EB, int8_t) = 1, MF(9, 0x7DF, int8_t) = 0;
    CHECK(AreWeSolid(9) == 1 && PC_CAR_B358(9) == 0);
    DetectDroneAbort();
    CHECK(CF(9, 0x340, float) == 7.5f && PC_CAR_FWD_PROGRESS(9) == 7.5f && g_nstuck == 0);
}

/* DetectDroneAbort with 12 cars: before 5 seconds, the 12 cars' progress and heading, and the models' stuck
 * time, cleared; records 12 to 15 not. */
static void test_remap12(void)
{
    static const int progress[] = {0x340, 0x344, 0x364, 0x368};
    sentinels();
    test_race_cars = 12;
    gModelTime = 1.0f;
    DetectDroneAbort();
    for (int i = 0; i < 12; i++) {
        CHECK(PC_CAR_FWD_PROGRESS(i) == 0.0f && PC_CAR_HEADING(i) == 0.0f && only(i, progress, 2));
        CHECK(PC_MODEL_STUCK_TIME(i) == 0.0f);
    }
    CHECK(untouched_from(12) && MF(12, 0x7D8, uint32_t) == 0xEEEEEEEEu);
    test_race_cars = 6;
}

/* A race of 12 cars, model i the car of slot i, car 0 the human: none lapped (their lap, +0x7E8, is the race's).
 * Cars finish from 11 down to 0, each later: CarReportsGameOver locks each one's place (+0xEE) in the order of the
 * scores (+0xF0), and PlaceUnlockedCars places the others after them by distance (+0x100), the farthest first. */
static void race12(void)
{
    sentinels();
    gNumActiveCars = 12, gNumLaps = 3, gRace = 0, gNumPlayers = 1, gEndGame = 0;
    gNumCars = 12;
    for (int i = 0; i < PC_CAR_SLOTS; i++) {
        MF(i, 0x7C6, int16_t) = (int16_t)i, MF(i, 0x7CC, int8_t) = i == 0 ? 2 : 1, MF(i, 0x7E8, int8_t) = 3;
        PC_CAR_FINISHED(i) = 0, PC_CAR_STATE(i) = 0;
        PC_CAR_DISTANCE(i) = 100.0f * (float)((i * 7) % 12);   /* every distance different */
    }
}

static int ahead(int car, int n)   /* the cars of the first n with a greater distance */
{
    int k = 0;
    for (int j = 0; j < n; j++)
        k += PC_CAR_DISTANCE(j) > PC_CAR_DISTANCE(car);
    return k;
}

static void test_places12(void)
{
    race12();
    PlaceUnlockedCars();                   /* nobody finished: all 12 by distance */
    for (int i = 0; i < 12; i++)
        CHECK(PC_CAR_PLACE(i) == ahead(i, 12));
    for (int i = 12; i < PC_CAR_SLOTS; i++)
        CHECK(only(i, (const int[]){0x100, 0x104, 0x359, 0x35A, 0xEF, 0xF0}, 3));

    race12();
    for (int k = 0; k < 12; k++) {
        int car = 11 - k;
        CarReportsGameOver(car, 10.0f + (float)k);
        for (int j = 11; j >= car; j--)                         /* the finished, in their order */
            CHECK(PC_CAR_FINISHED(j) == 1 && PC_CAR_PLACE(j) == 11 - j && PC_CAR_TIME(j) == 10.0f + (float)(11 - j));
        for (int j = 0; j < car; j++)                           /* the others after them, by distance */
            CHECK(PC_CAR_FINISHED(j) == 0 && PC_CAR_PLACE(j) == k + 1 + ahead(j, car));
    }
    CHECK(gEndGame == 1);                                       /* the human's finish ends the race's clock */
    for (int i = 12; i < PC_CAR_SLOTS; i++)
        CHECK(PC_CAR_FINISHED(i) == 0 && CF(i, 0xEE, uint8_t) == SENTINEL);
}

/* set_catchup over 12 cars: with no catch-up, each car of the 12 a drone or human (owner 0 or 6) at scale 1; with
 * it for every car (the drones' level 5), each one's scale moves toward 1 + its distance behind the farthest of the
 * 12 / 15,000. Cars 12 to 15, farther than all, are not of the race. */
static void test_catchup12(void)
{
    sentinels();
    test_race_cars = 12;
    for (int i = 0; i < PC_LINK_SLOTS; i++)
        pc_links[i].owner = i == 0 ? 6 : 0;
    gUseCatchup = 0;
    SetCatchup();
    for (int i = 0; i < PC_MODEL_SLOTS; i++)
        CHECK(MF(i, 0x400, uint32_t) == (i < 12 ? 0x3F800000u : 0xEEEEEEEEu));

    for (int i = 0; i < PC_CAR_SLOTS; i++) {
        MF(i, 0x7C8, int16_t) = 1, MF(i, 0x7CC, int8_t) = i == 0 ? 2 : 1, MF(i, 0x400, float) = 1.0f;
        PC_CAR_STATE(i) = 0, PC_CAR_DISTANCE(i) = i < 12 ? 100.0f * (float)i : 1e6f;
    }
    gNumPlayers = 1, gDifficulty = 5, gUseCatchup = 2;
    SetCatchup();
    for (int i = 0; i < 11; i++)
        CHECK(MF(i, 0x400, float) > MF(i + 1, 0x400, float));
    CHECK(MF(11, 0x400, float) > 0.9999f && MF(11, 0x400, float) < 1.0001f);   /* the farthest: its own scale */
    for (int i = 12; i < PC_MODEL_SLOTS; i++)
        CHECK(MF(i, 0x400, float) == 1.0f);
    test_race_cars = 6;
}

/* MissileLock, a homing shot of car 0's: it turns toward the nearest car in its forward cone among the
 * gNumCars cars of the race, by a walk from the array to their end in its view (0x468 bytes a car). Cars 6 to 15,
 * after the race's, are nearer still. */
static void test_homing(void)
{
    struct { void *next; int8_t owner, u05, kind, flags; float life, timer, elapsed, vel[3], pos[3], prev[3],
             uv[3][3]; } shot;
    sentinels();
    memset(&shot, 0, sizeof shot);
    for (int i = 0; i < PC_CAR_SLOTS; i++) {
        CF(i, 0x308, int8_t) = 1, CF(i, 0x359, int8_t) = 0, CF(i, 0x35B, int8_t) = (int8_t)i;   /* in, slot i */
        PC_CAR_POS(i)[0] = 0.0f, PC_CAR_POS(i)[1] = 0.0f, PC_CAR_POS(i)[2] = i < 6 ? -100.0f : 10.0f;
        MF(i, 0x640, int8_t) = 0;
        gCarTeam[i] = (int8_t)i;
    }
    gNumCars = 6;
    MissileLock(&shot);
    CHECK(g_nturns == 0);                           /* behind it, all six; the nearer ones aren't in the race */
    PC_CAR_POS(3)[0] = -5.0f, PC_CAR_POS(3)[1] = 5.0f, PC_CAR_POS(3)[2] = 100.0f;
    MissileLock(&shot);
    CHECK(g_nturns == 2 && g_turn[0] == 0.04f && g_turn[1] == 0.03f);   /* toward x < 0 and y > 0 */
    g_nturns = 0, gNumCars = 3;
    MissileLock(&shot);
    CHECK(g_nturns == 0);                           /* car 3 isn't one of the first 3 */
}

/* initialize for model 10 (slot 10): car 10's battle state reset, +0x358, +0x386 (800), +0x38C, +0x3A0,
 * +0x3A2, +0x3B0 and +0x3B4; nothing else of the cars. */
static void test_battle_reset(void)
{
    static const int reset[] = {0x358, 0x359, 0x386, 0x388, 0x38C, 0x390, 0x3A0, 0x3A1, 0x3A2, 0x3A3, 0x3B0, 0x3B8};
    sentinels();
    MF(10, 0x7C6, int16_t) = 10;
    initialize(PC_MODEL(10));
    CHECK(PC_CAR_HEALTH(10) == 800 && CF(10, 0x386, int16_t) == 800 && PC_CAR_B358(10) == 0);
    CHECK(PC_CAR_BATTLE_FLAGS(10) == 0 && PC_CAR_B3A0(10) == 0 && PC_CAR_B3A2(10) == 0);
    CHECK(PC_CAR_F3B0(10) == 0.0f && PC_CAR_F3B4(10) == 0.0f && only(10, reset, 6));
    for (int i = 0; i < PC_CAR_SLOTS; i++)
        CHECK(i == 10 || only(i, NULL, 0));
}

/* Update_MDrive in a race of 4 players (gRace 2): cars 1 to 3 to the cameras, each its node handle's
 * low half (+0xF4), its position (+0x8) and its +0x50, through what were car 1's names and a stride of 0x3B8. */
static void test_cameras(void)
{
    sentinels();
    for (int i = 0; i < PC_CAR_SLOTS; i++)
        PC_CAR_NODE(i) = 0x10000 * i + 0x40 + i;
    gRace = 2, gNumPlayers = 4;
    Update_MDrive();
    CHECK(g_ncams == 3);
    for (int k = 0; k < 3 && k < g_ncams; k++)
        CHECK(g_cam[k].node == 0x41 + k && g_cam[k].pos == (void *)PC_CAR_POS(k + 1)
              && g_cam[k].uvs == (void *)PC_CAR_UVS50(k + 1));
}

/* gameover_validOption in a stunt (gRace 3): option 5 only once car gCircuitPlayer has finished (+0xEF, with a
 * stride of 0x3B8 on the N64), here car 7. */
static void test_finished(void)
{
    sentinels();
    gRace = 3, gCircuitPlayer = 7;
    PC_CAR_FINISHED(7) = 0, PC_CAR_FINISHED(6) = 1, PC_CAR_FINISHED(8) = 1;
    CHECK(gameover_validOption(5) == 0 && gameover_validOption(0) == 1);
    PC_CAR_FINISHED(7) = 1, PC_CAR_FINISHED(6) = 0;
    CHECK(gameover_validOption(5) == 1 && CF(7, 0xEF, int8_t) == 1);
}

int main(void)
{
    test_layout();
    test_start();
    test_remap();
    test_remap12();
    test_places12();
    test_catchup12();
    test_homing();
    test_battle_reset();
    test_cameras();
    test_finished();
    return TEST_DONE();
}
