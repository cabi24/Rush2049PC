/* port/assets/tests/test_drones.c: the drones (docs/port/DECOUPLE.md 6) through the game's own code, with more car
 * slots declared than a race holds: the drone lists pc_drones and pc_our_drones, maxpath's controls per car slot
 * (pc_mp_steer, pc_mp_brake, pc_mp_gas, pc_mp_dir, pc_mp_save_dir) and the drone schedule update_drone_models
 * walks. This test and its game objects are built with PC_MAX_LINKS 16 (CMakeLists.txt), so the storages have 16
 * entries where the game build has 12 and the N64 6, and the schedule's second kind of row starts at 17.
 * - maxpath (MaxPathControls, func_800E4B58) for 12 cars on a straight path: each car's steer, brake and gas land in
 *   its own slot and in its model record, its last directions are read and written in its own slot, and the other
 *   slots stay as they were; the camera's drone pick (func_800E451C) walks 11 drones, skipping the one this node
 *   doesn't drive; audio_effect_remove clears every slot's controls.
 * - update_drone_models: for the rows the N64 has (0-6 drones with one human, 0-4 with two) the order of the drones it
 *   updates is the N64's for the same table; for 7-12 drones (5-10 with two humans) each round updates every drone
 *   once, in as many model iterations as the table's last row of that kind, as evenly as it can, the larger first.
 *   The table here is the test's own (neither the retail cartridge's nor the synthetic one's), then the synthetic
 *   cartridge's kind ({n, -1}: every drone each iteration). A drone's and a human's retirement (func_800E56F8) move
 *   the next round to the row with a drone fewer and to the one-human rows.
 * - model_iteration's speed of every car, for the race's cars (PC_RACE_CARS: 6, then 12).
 * - DoDrones (render_large_objects) for 12 cars: the drones' weights by place and the human's neighbour drones.
 * game: src/blob/groups/func_800E56F8/group.c src/blob/groups/maxpath_controls/group.c src/blob/audio_effect_remove.c
 * game: src/blob/groups/render_large_objects/group.c
 */
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

/* ---------------------------------------------------------------- the game's data the code under test uses */

struct pc_link pc_links[PC_LINK_SLOTS];
struct pc_model pc_models[PC_MODEL_SLOTS];
struct pc_car pc_cars[PC_CAR_SLOTS];
short pc_drones[PC_MAX_LINKS], pc_our_drones[PC_MAX_LINKS];
float pc_mp_steer[PC_MAX_LINKS], pc_mp_brake[PC_MAX_LINKS], pc_mp_gas[PC_MAX_LINKS];
float pc_mp_dir[PC_MAX_LINKS][3], pc_mp_save_dir[PC_MAX_LINKS][3];

int16_t D_80120E74[64];                 /* the schedule: Row3[12], and room for a walk past it */
int16_t D_8015274C, D_80153E84, D_80153F08, D_80153F24, D_80153F40, D_80154182;
volatile int16_t D_80152768, D_80153FD2;
volatile uint16_t D_801525F0;
int8_t D_80152744, D_801543C8, D_8014978C, D_80152015, D_80152030, D_80152570, D_80152718;
uint8_t D_8013FECB, D_80142760;
int32_t D_80143FF4, gameplay_mode, state_word_a, D_801174B4, D_8014A110, D_801163F8[36];
float D_801543CC, D_80154190, D_8002AFB8 = 1.0f / 60, D_8012447C, D_80124480, D_801161D4[8], D_801161E4[8];
int8_t D_801440AD[4 * 0x310];
intptr_t D_80152698[16];
int16_t D_80154484[16][38];
uint8_t input_rec0[4 * 80];
struct { char c[4]; } D_80120E50;

typedef struct { int16_t x, y, z; uint8_t flag, hint; } TrackPt;
typedef struct { uint16_t numPoints, pad; TrackPt *points; } Track;
typedef struct { uint8_t pad0[2]; int16_t last; uint8_t pad4[4]; int16_t count; uint8_t padA[0x26]; int16_t start[16]; } Section;
#define NPTS 256
static TrackPt g_pts[NPTS];
Track D_8012E5E8[4];
Section D_80151CE8[2];

/* What the code calls: the path's helpers, and the model update's callees, which record the slots they were given. */
static int g_updated[4096], g_nupdated;
static float g_angle;
void car_select_handler(int16_t slot) { if (g_nupdated < 4096) g_updated[g_nupdated++] = slot; }
int16_t func_800B9338(int16_t pt, int16_t sel) { (void)sel; return (int16_t)((pt + 1) % NPTS); }
int16_t func_800DC080(int16_t pt, int16_t sel) { (void)sel; return (int16_t)((pt + NPTS - 1) % NPTS); }
float func_800BDD90(float *v) { return sqrtf(v[0] * v[0] + v[1] * v[1]); }
float func_8008B3C8(float *v) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
float func_8008C768(float y, float x) { (void)y; (void)x; return g_angle; }
void func_800A61B0(float *d, float *out, float *m) { (void)m; out[0] = d[0], out[1] = d[1], out[2] = d[2]; }
void camera_blend_between(void *m, float *s) { (void)m; (void)s; }
static int g_errors;
void dispatch_handler(int32_t a) { (void)a; g_errors++; }
void state_utility(int16_t x, int32_t y, void *t) { (void)x; (void)y; (void)t; }
void audio_effect_apply(void) {}
void func_800DE860(void) {}
void func_800C3578(int32_t a) { (void)a; }
void func_800C54F0(int16_t a, int32_t b) { (void)a; (void)b; }
void func_800D1248(void *a) { (void)a; }
void func_800D4D84(int32_t a, int32_t b) { (void)a; (void)b; }
void func_800D5828(int16_t a) { (void)a; }
void func_800E05F0(void *a) { (void)a; }
void func_800E3724(void *a) { (void)a; }
void func_800E543C(int16_t a, int16_t b) { (void)a; (void)b; }
void func_800E5C9C(void *a) { (void)a; }
int32_t func_800E5D64(int32_t a, float *t) { (void)a; *t = 0; return -1; }
void func_800E681C(void) {}
void main_menu_render(void) {}
void menu_audio_settings(void *a) { (void)a; }
static int g_round_ends;
void players_race_update(void) { g_round_ends++; }
void track_select_handler(void *a) { (void)a; }

void func_800E4B58(void *m);
void func_800E451C(void *m, void *cp, int32_t change_flag);
void audio_effect_remove(void);
void update_drone_models(void);
void model_iteration(void);
void func_800E56F8(int32_t slot);
void render_large_objects(void);

#define MF(i, off) (*(float *)((uint8_t *)&pc_models[i] + (off)))
#define MS16(i, off) (*(int16_t *)((uint8_t *)&pc_models[i] + (off)))
#define MS8(i, off) (*(int8_t *)((uint8_t *)&pc_models[i] + (off)))
#define NEAR(a, b) (fabsf((a) - (b)) <= 1e-5f * (1 + fabsf(b)))

static void test_layout(void)
{
    CHECK(PC_MAX_LINKS == 16 && PC_DRONE_SCHED_TWO == 17);
    CHECK(sizeof pc_drones == 16 * 2 && sizeof pc_mp_dir == 16 * 12 && sizeof pc_mp_gas == 16 * 4);
}

/* ---------------------------------------------------------------- maxpath for 12 cars */

/* A straight path along z, a point every 20 feet, on all four of the drones' paths; car s 400 feet after car s - 1,
 * so no car is near another (avoid_areas looks 300 feet around). */
static void setup_path(void)
{
    for (int k = 0; k < NPTS; k++)
        g_pts[k] = (TrackPt){0, 0, (int16_t)(20 * k), 0, 0};
    for (int t = 0; t < 4; t++)
        D_8012E5E8[t] = (Track){NPTS, 0, g_pts};
    memset(D_80151CE8, 0, sizeof D_80151CE8);
    D_80151CE8[0].count = 1;
}

static void setup_cars(int n)
{
    memset(pc_models, 0, sizeof pc_models);
    memset(pc_cars, 0, sizeof pc_cars);
    D_80152744 = (int8_t)n;
    for (int s = 0; s < n; s++) {
        float z = 400.0f * s + 5;
        MS16(s, 0x7C6) = (int16_t)s;            /* slot */
        MS16(s, 0x7C8) = 1;                     /* in the race */
        MS8(s, 0x7CC) = 1;                      /* a drone */
        MF(s, 0x22C + 8) = z;                   /* RWR z */
        MF(s, 0x794 + 8) = z;                   /* the dead-reckoned position */
        MF(s, 0x228) = 2.0f + s;                /* the velocity's z: AdjustSpeed's speed */
        MF(s, 0x3F0) = 10;                      /* the speed avoid_areas tests */
        MF(s, 0x634) = 1;
        MF(s, 0x638) = 1;
        MS16(s, 0x6C4) = -1;                    /* not resurrecting */
        MF(s, 0x7EC) = 0;                       /* drone_scale: no target speed */
        PC_MODEL_SLOT(s) = (short)s;
        PC_CAR_NAV(s).pt = (short)(20 * s);
        PC_CAR_NAV(s).flag = -1;
    }
}

static void test_maxpath12(void)
{
    float before[PC_MAX_LINKS][9];

    setup_path();
    setup_cars(12);
    for (int s = 0; s < PC_MAX_LINKS; s++) {
        pc_mp_steer[s] = 0.5f + s, pc_mp_brake[s] = 0.01f * s, pc_mp_gas[s] = 0;
        pc_mp_dir[s][0] = 1.0f + s, pc_mp_dir[s][1] = 0, pc_mp_dir[s][2] = 2.0f + s;
        pc_mp_save_dir[s][0] = pc_mp_save_dir[s][1] = pc_mp_save_dir[s][2] = 3.0f + s;
    }
    D_801543CC = 100, D_80154190 = 100;         /* no camera pick now */
    D_80154182 = -1;
    for (int s = 0; s < 12; s++) {
        float dir0 = pc_mp_dir[s][0], gas, brake0 = pc_mp_brake[s];

        for (int o = 0; o < PC_MAX_LINKS; o++) {
            before[o][0] = pc_mp_steer[o], before[o][1] = pc_mp_brake[o], before[o][2] = pc_mp_gas[o];
            memcpy(&before[o][3], pc_mp_dir[o], 12);
            memcpy(&before[o][6], pc_mp_save_dir[o], 12);
        }
        g_angle = 0.1f * s;
        func_800E4B58(&pc_models[s]);
        /* AdjustSteer: 1 - angle * 2/pi, kept and given to the car. */
        CHECK(MF(s, 0x720) == pc_mp_steer[s] && NEAR(pc_mp_steer[s], 1.0f - 0.1f * s * 0.63661975f));
        /* AdjustSpeed with no target speed: the gas falls by half the speed * 0.183 from 0, and below -0.75 the
         * brake takes the rest; both kept in [0, 1] and given to the car. */
        gas = -(2.0f + s) * 0.5f * 0.183f;
        CHECK(MF(s, 0x72C) == pc_mp_gas[s] && pc_mp_gas[s] == 0);
        CHECK(MF(s, 0x728) == pc_mp_brake[s]);
        if (gas < -0.75f) {
            float b = brake0 - (gas + 0.75f) / 4;
            CHECK(NEAR(pc_mp_brake[s], b > 1 ? 1 : b));
        } else {
            CHECK(pc_mp_brake[s] == 0);
        }
        /* avoid_areas: the target is straight ahead, so this direction's x is half the last one's (read from this
         * slot) and the direction to the target has none; both are kept in this slot. */
        CHECK(pc_mp_dir[s][0] == dir0 * 0.5f && pc_mp_save_dir[s][0] == 0);
        CHECK(pc_mp_dir[s][2] > 0 && pc_mp_dir[s][1] == pc_mp_save_dir[s][1]);
        CHECK(pc_mp_dir[s][2] == pc_mp_save_dir[s][2]);
        for (int o = 0; o < PC_MAX_LINKS; o++)
            if (o != s) {
                float now[9] = {pc_mp_steer[o], pc_mp_brake[o], pc_mp_gas[o]};
                memcpy(&now[3], pc_mp_dir[o], 12);
                memcpy(&now[6], pc_mp_save_dir[o], 12);
                CHECK(memcmp(now, before[o], sizeof now) == 0);
            }
    }
    CHECK(pc_mp_brake[11] > 0 && pc_mp_brake[11] != pc_mp_brake[10]);   /* slots past the N64's 6 have their own */
    CHECK(g_errors == 0);

    /* audio_effect_remove (maxpath's reset) clears every slot's steer, brake and gas. */
    for (int s = 0; s < PC_MAX_LINKS; s++)
        pc_mp_steer[s] = pc_mp_brake[s] = pc_mp_gas[s] = 7;
    D_801543CC = 3;
    audio_effect_remove();
    for (int s = 0; s < PC_MAX_LINKS; s++)
        CHECK(pc_mp_steer[s] == 0 && pc_mp_brake[s] == 0 && pc_mp_gas[s] == 0);
    CHECK(D_80154182 == -1 && D_80154190 == 3);
}

/* The camera's pick among the drones (MP_IntervalPos, func_800E451C): with 11 drones, slots 1-11, of which this node
 * drives all but slot 7, the pick goes to the next drone it drives each time the picked drone is updated half a
 * second after the last pick: indices 0-5, 7-10 of the list, and round again. */
static void test_pick(void)
{
    static const int want[] = {0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 0, 1};

    setup_path();
    setup_cars(12);
    MS8(0, 0x7CC) = 2;
    D_80152768 = 0;
    for (int s = 1; s < 12; s++) {
        pc_drones[D_80152768++] = (short)s;
        PC_MODEL_WE_CONTROL(s) = s != 7;
    }
    D_80154182 = -1, D_80154190 = 0;
    for (unsigned k = 0; k < sizeof want / sizeof want[0]; k++) {
        int node = D_80154182 < 0 ? 1 : pc_drones[D_80154182];
        D_801543CC = 1.0f + k;
        func_800E451C(&pc_models[node], &PC_CAR_NAV(node), 1);
        CHECK(D_80154182 == want[k]);
        func_800E451C(&pc_models[pc_drones[D_80154182]], &PC_CAR_NAV(pc_drones[D_80154182]), 1);
        CHECK(D_80154182 == want[k]);           /* not again within half a second */
    }
}

/* ---------------------------------------------------------------- the drone schedule */

/* The test's table, Row3[12]: rows 0-2 {n, -1}, rows 3-6 {1, n - 1, -1}; rows 7-8 {n, -1}, rows 9-11
 * {n - 1, 1, -1} (n the row's drones). Or, with `all`, every row {n, -1} as the synthetic cartridge has it. */
static void set_table(int all)
{
    memset(D_80120E74, 0x7F, sizeof D_80120E74);
    for (int r = 0; r < 12; r++) {
        int n = r < 7 ? r : r - 7, *v;
        int16_t *row = &D_80120E74[3 * r];
        int one[3] = {n, -1, -1}, split1[3] = {1, n - 1, -1}, split2[3] = {n - 1, 1, -1};
        v = all || (r < 7 ? n < 3 : n < 2) ? one : r < 7 ? split1 : split2;
        row[0] = (int16_t)v[0], row[1] = (int16_t)v[1], row[2] = (int16_t)v[2];
    }
}

/* update_drone_models as the N64 has it, on the same table (MAT3 mdrive.c), giving the slots it would update. */
static int16_t ref_sched, ref_index, ref_cur, ref_next;
static int ref_step(int n, int *out)
{
    int k = 0, todo;

    if (D_80120E74[3 * ref_sched + ++ref_index] == -1) {
        ref_index = 0;
        ref_sched = ref_next;
    }
    todo = D_80120E74[3 * ref_sched + ref_index];
    for (int i = 0; i < todo; i++) {
        out[k++] = pc_our_drones[ref_cur];
        if (++ref_cur >= n)
            ref_cur = 0;
    }
    return k;
}

static void start_race(int n, int two)
{
    for (int i = 0; i < PC_MAX_LINKS; i++)
        pc_our_drones[i] = (short)(i < n ? 15 - i : -1);   /* the drones' slots, from the storage's last */
    D_8015274C = (int16_t)n;
    D_80153E84 = (int16_t)(n + (two ? PC_DRONE_SCHED_TWO : 0));   /* world_physics_tick's choice */
    D_80153F08 = D_80153E84;
    D_80153F24 = -1;
    D_80153F40 = 0;
    ref_sched = ref_next = (int16_t)(n + (two ? 7 : 0));
    ref_index = -1;
    ref_cur = 0;
}

static int step(void)
{
    g_nupdated = 0;
    update_drone_models();
    return g_nupdated;
}

/* The N64's rows: the same slots in the same order, iteration by iteration, for 60 iterations. */
static void test_sched_n64(int all)
{
    setup_cars(16);
    set_table(all);
    for (int two = 0; two < 2; two++)
        for (int n = 0; n <= (two ? 4 : 6); n++) {
            int want[16], k, same = 1;
            start_race(n, two);
            for (int it = 0; it < 60; it++) {
                k = ref_step(n, want);
                same &= step() == k && memcmp(g_updated, want, k * sizeof want[0]) == 0;
            }
            CHECK(same);
            if (!same)
                fprintf(stderr, "  N64 row of %d drones, %s\n", n, two ? "two humans" : "one human");
        }
}

/* Past the N64's rows: every round (the iterations from one -1 to the next) updates each drone once, in as many
 * iterations as the table's last row of the kind has groups, n / groups each, the first n % groups one more. */
static void test_sched_pc(int all)
{
    setup_cars(16);
    set_table(all);
    for (int two = 0; two < 2; two++) {
        int groups = all ? 1 : 2;
        for (int n = two ? 5 : 7; n <= (two ? 10 : 12); n++) {
            int ok = 1;
            start_race(n, two);
            for (int round = 0; round < 5; round++) {
                int seen[16] = {0};
                for (int g = 0; g < groups; g++) {
                    int got = step();
                    ok &= got == n / groups + (g < n % groups);
                    for (int i = 0; i < got; i++)
                        seen[15 - g_updated[i]]++;
                    ok &= D_80153F24 == g;      /* the round's g-th iteration */
                }
                for (int i = 0; i < n; i++)
                    ok &= seen[i] == 1;
                ok &= D_80153F40 == 0;          /* the cursor is back at the first drone: players_race_update */
            }
            CHECK(ok);
            if (!ok)
                fprintf(stderr, "  PC row of %d drones, %s, table %d\n", n, two ? "two humans" : "one human", all);
        }
    }
}

/* Retirements (func_800E56F8 when a car is done): a drone is done as it is updated, at the cursor; it leaves
 * our_drones and the next round has a drone fewer. A human's retirement, of two, moves the next round to the
 * one-human rows. Each round still updates every drone once. */
static int round_once(int n, int first, int second, int *seen)
{
    int ok = step() == first;
    for (int i = 0; i < g_nupdated; i++)
        seen[g_updated[i]]++;
    ok &= step() == second;
    for (int i = 0; i < g_nupdated; i++)
        seen[g_updated[i]]++;
    return ok && D_80153F40 == 0 && D_8015274C == n;
}

static void test_sched_retire(void)
{
    static uint8_t input[80];
    int seen[16] = {0};

    set_table(0);
    setup_cars(16);
    start_race(11, 1);                          /* 11 drones (slots 15 to 5) with two humans: a PC row */
    CHECK(round_once(11, 6, 5, seen));
    /* The list's first drone, slot 15, is done when the next round updates it: 6 asked, 5 updated. */
    pc_cars[15].state = 1;
    memset(seen, 0, sizeof seen);
    CHECK(round_once(10, 5, 5, seen));
    CHECK(D_80153F08 == PC_DRONE_SCHED_TWO + 10 && D_80153E84 == PC_DRONE_SCHED_TWO + 11);
    CHECK(pc_our_drones[0] == 14 && pc_our_drones[9] == 5);
    for (int i = 0; i < 16; i++)
        CHECK(seen[i] == (i >= 5 && i < 15));
    /* A human (slot 0, with an input record) is done: the next round uses the one-human row of 10 drones. */
    pc_cars[0].input = input;
    pc_cars[0].state = 1;
    func_800E56F8(0);
    CHECK(D_80153F08 == 10 && D_8015274C == 10);
    memset(seen, 0, sizeof seen);
    CHECK(round_once(10, 5, 5, seen) && D_80153E84 == 10);
    for (int i = 0; i < 16; i++)
        CHECK(seen[i] == (i >= 5 && i < 15));
}

/* ---------------------------------------------------------------- model_iteration's speeds */

static void test_speeds(void)
{
    for (int cars = 6; cars <= 12; cars += 6) {
        setup_cars(16);
        for (int s = 0; s < 16; s++) {
            MF(s, 0x788) = 3, MF(s, 0x78C) = 4, MF(s, 0x790) = 0;   /* |v| = 5 */
            MF(s, 0x3F0) = -1;
            PC_MODEL_WE_CONTROL(s) = 0;
        }
        set_table(0);
        start_race(0, 0);
        test_race_cars = cars;
        state_word_a = 0x400000, D_801525F0 = 1, D_80153FD2 = 0;
        model_iteration();
        for (int s = 0; s < 16; s++)
            CHECK(MF(s, 0x3F0) == (s < cars ? 5.0f : -1.0f));
    }
    test_race_cars = 6;
}

/* ---------------------------------------------------------------- DoDrones for 12 cars */

/* One human (slot 4, in place 6) and 11 drones: slots 11 to 6 in places 0-5, slot 5 and slots 3 to 0 in places 7-11,
 * so the places aren't the slots. The drones' weights are their order by place; the drones just ahead of and behind
 * the human are the two candidates, and the nearer (behind, 30 feet against 50) targets the human, every other
 * drone itself; every drone's catch-up moves toward a scale below 1. */
static void test_dodrones(void)
{
    static const int order[12] = {11, 10, 9, 8, 7, 6, 4, 5, 3, 2, 1, 0};
    int w = 0;

    setup_cars(12);
    for (int p = 0; p < 12; p++) {
        int s = order[p];
        PC_CAR_PLACE(s) = (signed char)p;
        PC_CAR_DISTANCE(s) = 1000.0f - 100.0f * p;
        PC_CAR_POS(s)[0] = 0, PC_CAR_POS(s)[1] = 0, PC_CAR_POS(s)[2] = 1000.0f - 100.0f * p;
        PC_MODEL_DRONE_TYPE(s) = (signed char)(s == 4 ? 2 : 1);
        PC_MODEL_WE_CONTROL(s) = s != 4;
        PC_MODEL_DRONE_SCALE(s) = 1, PC_MODEL_TIME_BOOST(s) = 1;
    }
    PC_CAR_POS(order[5])[2] = PC_CAR_POS(4)[2] + 50;
    PC_CAR_POS(order[7])[2] = PC_CAR_POS(4)[2] - 30;
    D_801543CC = 10, D_801174B4 = 0, D_8014A110 = 0, D_80152030 = 2, D_8013FECB = 0, D_80152015 = 0;
    render_large_objects();
    for (int p = 0; p < 12; p++) {
        int s = order[p];
        if (s == 4)
            continue;
        CHECK(PC_CAR_WEIGHT_INDEX(s) == w++);
        CHECK(PC_MODEL_DRONE_TARGET(s) == (p == 7 ? 4 : s));
        CHECK(PC_MODEL_DRONE_SCALE(s) < 1);
    }
    CHECK(PC_MODEL_DRONE_SCALE(4) == 1);
}

int main(void)
{
    test_layout();
    test_maxpath12();
    test_pick();
    test_sched_n64(0);
    test_sched_n64(1);
    test_sched_pc(0);
    test_sched_pc(1);
    test_sched_retire();
    test_speeds();
    test_dodrones();
    return TEST_DONE();
}
