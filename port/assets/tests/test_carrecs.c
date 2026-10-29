/* port/assets/tests/test_carrecs.c: the car records (MAT3 gGameCar, player_array: pc_cars, struct pc_car,
 * port/include/pc_game_types.h) through the game's own readers and writers, with more records declared than the
 * game walks (docs/port/DECOUPLE.md 4). This test and its game objects are built with PC_MAX_LINKS 16
 * (CMakeLists.txt), so the storage has 16 records where the game build has 12 and the N64 6. Every record starts as
 * a sentinel: the code must write and read the fields at their PC offsets (CAR_OFS of the N64 ones), step 0x468
 * between records, and stop where it stops on the N64, whatever the storage's size.
 * func_800FBE60 clears the race start's +0x108 and copies the positions of D_80152744 cars, at m2c's byte strides
 * of CAR_OFS(952) from the array; menu_controller_remap clears 6 cars' progress and heading (an index walk), and
 * then reaches a player's car through its input record's node, a record pointer in its own view, where
 * func_800CF604 reads the car's +0x358 through what was the alias D_80152B70 (a stride of 0x3B8 on the N64);
 * func_8038DDDC (image B's homing) walks the cars of the race from the array to its end, D_80152818 +
 * D_801543CA on the N64; func_800D1004 resets a car's battle state through the model's slot; race_init_helper hands
 * cars 1 to 3 to the cameras through what were D_80152BD0, D_80152BD8 and D_80152C20, car 1's +0, +8 and +0x50;
 * gameover_validOption reads a car's finish through what was D_80152907, +0xEF with a stride of 0x3B8.
 * game: src/blob/func_800FBE60.c src/blob/menu_controller_remap.c src/blob/func_800CF604.c
 * game: src/ovl_b/groups/runtime_b_nearest_steering/group.c src/blob/func_800D1004.c src/blob/race_init_helper.c
 * game: src/blob/gameover_validOption.c
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

int32_t gameplay_mode, D_8014A110;
int16_t active_player_count, D_8014A108;
volatile int16_t D_801543CA;
uint8_t D_80152744, D_801543D4;
float D_801543CC, D_80152018, D_80152218[6][3];
int8_t D_80152031, D_801403D0, D_80152718, D_8013FECB, D_801427A1, D_8012E67C[PC_CAR_SLOTS];
uint32_t D_801174B4;
uint8_t D_8014A118[6 * 80] __attribute__((aligned(16)));          /* the input records: node at +0 */
struct { uint8_t pad[4]; void *pts; uint8_t pad8[4]; void *branch; } D_801407F0;
float D_8011418C[9], D_80124160 = 1.0f, D_80124164 = 2.0f, D_80124168 = 3.0f;

/* What the code under test calls: each records what it was given. */
static float g_turn[2];
static int g_nturns, g_ncams, g_nstuck;
static struct { int16_t node; void *pos, *uvs; } g_cam[8];
void func_800C54F0(int16_t node, int32_t arg1) { (void)node; (void)arg1; }
void func_800C4F68(int32_t what, void *m, int32_t arg2) { (void)what; (void)m; (void)arg2; g_nstuck++; }
void func_800B9F60(int32_t idx, int32_t seg, int32_t *nidx, int32_t *nseg) { *nidx = idx; *nseg = seg; }
void func_800A61B0(float *in, float *out, void *uvs) { (void)uvs; out[0] = in[0], out[1] = in[1], out[2] = in[2]; }
float func_8008C768(float x, float z) { return x + z; }
void func_80090E9C(float a, void *uv) { (void)uv; if (g_nturns < 2) g_turn[g_nturns] = a; g_nturns++; }
void func_80090F44(float a, void *uv) { (void)uv; if (g_nturns < 2) g_turn[g_nturns] = a; g_nturns++; }
void math_utility(void *src, void *dst) { (void)src; (void)dst; }
void track_info_display(void *m) { (void)m; }
void menu_control_settings(void *m) { (void)m; }
void func_800D0810(void *m) { (void)m; }
void update_game_data(void) {}
int32_t func_800EB90C(void) { return 0; }
void func_8008D6FC(int16_t node, void *pos, void *uvs)
{
    if (g_ncams < 8)
        g_cam[g_ncams].node = node, g_cam[g_ncams].pos = pos, g_cam[g_ncams].uvs = uvs;
    g_ncams++;
}

void func_800FBE60(void);
void menu_controller_remap(void);
int32_t func_800CF604(int16_t arg0);
void func_8038DDDC(void *object);
void func_800D1004(void *car);
void race_init_helper(void);
int32_t gameover_validOption(int32_t arg0);

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

/* func_800FBE60, the race start: for each of D_80152744 cars, +0x108 cleared and the position (+0x8) copied into
 * D_80152218[i], at m2c's strides of CAR_OFS(952) from the array; nothing else. */
static void test_start(void)
{
    static const int f108[] = {0x108, 0x10C};
    sentinels();
    for (int i = 0; i < PC_CAR_SLOTS; i++)
        PC_CAR_POS(i)[0] = 10.0f * i, PC_CAR_POS(i)[1] = 1.0f + i, PC_CAR_POS(i)[2] = -2.0f * i;
    memset(D_80152218, 0, sizeof D_80152218);
    D_80152744 = 6;
    func_800FBE60();
    for (int i = 0; i < 6; i++) {
        CHECK(D_80152218[i][0] == 10.0f * i && D_80152218[i][1] == 1.0f + i && D_80152218[i][2] == -2.0f * i);
        CHECK(CF(i, 0x108, float) == 0.0f && PC_CAR_F108(i) == 0.0f);
        memset(&PC_CAR_POS(i), SENTINEL, 12);                                      /* set above */
        CHECK(only(i, f108, 1));
    }
    for (int i = 6; i < PC_CAR_SLOTS; i++)
        memset(&PC_CAR_POS(i), SENTINEL, 12);
    CHECK(untouched_from(6) && D_80152018 == 0 && D_80152031 == 0);
}

/* menu_controller_remap: before 5 seconds of model time, the 6 cars' progress (+0x340) and heading (+0x364)
 * cleared and the models' stuck time; then car 9 (a slot the storage has past the N64's 6), reached through player
 * 0's input record: moving forward, its progress is cleared and its distance kept; stopped and not drivable (its
 * +0x358 set, func_800CF604), cleared too; stopped and drivable, its progress is the model's time. */
static void test_remap(void)
{
    static const int progress[] = {0x340, 0x344, 0x364, 0x368};
    static const int moved[] = {0x104, 0x108, 0x340, 0x344, 0x364, 0x368};
    sentinels();
    D_801543CC = 1.0f;
    menu_controller_remap();
    for (int i = 0; i < 6; i++) {
        CHECK(PC_CAR_FWD_PROGRESS(i) == 0.0f && PC_CAR_HEADING(i) == 0.0f && only(i, progress, 2));
        CHECK(PC_MODEL_STUCK_TIME(i) == 0.0f);
    }
    CHECK(untouched_from(6) && MF(6, 0x7D8, uint32_t) == 0xEEEEEEEEu);

    sentinels();
    D_801543CC = 10.0f, D_80152744 = 0, D_8014A108 = 1, D_8014A118[0] = 9;
    MF(9, 0x7C6, int16_t) = 9, MF(9, 0x714, float) = 7.5f;                          /* model 9: slot, time */
    CF(9, 0x100, float) = 50.0f, CF(9, 0x104, float) = 10.0f;                       /* distance > the last */
    menu_controller_remap();
    CHECK(CF(9, 0x340, float) == 0.0f && CF(9, 0x104, float) == 50.0f && CF(9, 0x364, float) == 0.0f);
    memset(&CF(9, 0x100, float), SENTINEL, 4);                                      /* set above */
    CHECK(only(9, moved, 3) && untouched_from(10));
    for (int i = 0; i < 9; i++)
        CHECK(only(i, NULL, 0));

    sentinels();                                                                    /* stopped, +0x358 set */
    MF(9, 0x7C6, int16_t) = 9, MF(9, 0x714, float) = 7.5f;
    CF(9, 0x100, float) = 10.0f, CF(9, 0x104, float) = 10.0f, CF(9, 0x340, float) = 3.0f;
    CF(9, 0x358, int8_t) = 1;
    CHECK(func_800CF604(9) == 0);
    menu_controller_remap();
    CHECK(CF(9, 0x340, float) == 0.0f && CF(9, 0x364, float) == 0.0f);

    CF(9, 0x358, int8_t) = 0, CF(9, 0x340, float) = 0.0f;                          /* drivable */
    MF(9, 0x640, int8_t) = 0, MF(9, 0x6C4, int16_t) = -1, MF(9, 0x7EB, int8_t) = 1, MF(9, 0x7DF, int8_t) = 0;
    CHECK(func_800CF604(9) == 1 && PC_CAR_B358(9) == 0);
    menu_controller_remap();
    CHECK(CF(9, 0x340, float) == 7.5f && PC_CAR_FWD_PROGRESS(9) == 7.5f && g_nstuck == 0);
}

/* func_8038DDDC, a homing shot of car 0's: it turns toward the nearest car in its forward cone among the
 * D_801543CA cars of the race, by a walk from the array to their end in its view (0x468 bytes a car). Cars 6 to 15,
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
        D_8012E67C[i] = (int8_t)i;
    }
    D_801543CA = 6;
    func_8038DDDC(&shot);
    CHECK(g_nturns == 0);                           /* behind it, all six; the nearer ones aren't in the race */
    PC_CAR_POS(3)[0] = -5.0f, PC_CAR_POS(3)[1] = 5.0f, PC_CAR_POS(3)[2] = 100.0f;
    func_8038DDDC(&shot);
    CHECK(g_nturns == 2 && g_turn[0] == 0.04f && g_turn[1] == 0.03f);   /* toward x < 0 and y > 0 */
    g_nturns = 0, D_801543CA = 3;
    func_8038DDDC(&shot);
    CHECK(g_nturns == 0);                           /* car 3 isn't one of the first 3 */
}

/* func_800D1004 for model 10 (slot 10): car 10's battle state reset, +0x358, +0x386 (800), +0x38C, +0x3A0,
 * +0x3A2, +0x3B0 and +0x3B4; nothing else of the cars. */
static void test_battle_reset(void)
{
    static const int reset[] = {0x358, 0x359, 0x386, 0x388, 0x38C, 0x390, 0x3A0, 0x3A1, 0x3A2, 0x3A3, 0x3B0, 0x3B8};
    sentinels();
    MF(10, 0x7C6, int16_t) = 10;
    func_800D1004(PC_MODEL(10));
    CHECK(PC_CAR_HEALTH(10) == 800 && CF(10, 0x386, int16_t) == 800 && PC_CAR_B358(10) == 0);
    CHECK(PC_CAR_BATTLE_FLAGS(10) == 0 && PC_CAR_B3A0(10) == 0 && PC_CAR_B3A2(10) == 0);
    CHECK(PC_CAR_F3B0(10) == 0.0f && PC_CAR_F3B4(10) == 0.0f && only(10, reset, 6));
    for (int i = 0; i < PC_CAR_SLOTS; i++)
        CHECK(i == 10 || only(i, NULL, 0));
}

/* race_init_helper in a race of 4 players (gameplay_mode 2): cars 1 to 3 to the cameras, each its node handle's
 * low half (+0xF4), its position (+0x8) and its +0x50, through what were car 1's names and a stride of 0x3B8. */
static void test_cameras(void)
{
    sentinels();
    for (int i = 0; i < PC_CAR_SLOTS; i++)
        PC_CAR_NODE(i) = 0x10000 * i + 0x40 + i;
    gameplay_mode = 2, active_player_count = 4;
    race_init_helper();
    CHECK(g_ncams == 3);
    for (int k = 0; k < 3 && k < g_ncams; k++)
        CHECK(g_cam[k].node == 0x41 + k && g_cam[k].pos == (void *)PC_CAR_POS(k + 1)
              && g_cam[k].uvs == (void *)PC_CAR_UVS50(k + 1));
}

/* gameover_validOption in a stunt (gameplay_mode 3): option 5 only once car D_801543D4 has finished (+0xEF, what
 * was D_80152907 with a stride of 0x3B8), here car 7. */
static void test_finished(void)
{
    sentinels();
    gameplay_mode = 3, D_801543D4 = 7;
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
    test_homing();
    test_battle_reset();
    test_cameras();
    test_finished();
    return TEST_DONE();
}
