/* port/assets/tests/test_models.c: the model records (MAT3 model[]: pc_models, struct pc_model,
 * port/include/pc_game_types.h) through the game's own readers and writers, with more records declared than the
 * game walks (docs/port/DECOUPLE.md). This test and its game objects are built with PC_MAX_LINKS 16
 * (CMakeLists.txt), so the storage has 16 records where the game build has 12 and the N64 6. Every record starts as
 * a sentinel: the code must write and read the fields at their N64 offsets, step 0x808 between records, and stop at
 * the 6 it walks on the N64, whatever the storage's size.
 * func_800E762C sets the 6 records' model clock (an index walk); func_800D1248 ends a resurrect through a record
 * pointer and sets another record's modelrun through what was the alias D_8014A96C (+0x71C, a stride of 0x808),
 * whose record 0 it also reads as a scalar; func_800D0380 sets it through the same alias in m2c's spelling;
 * func_800D18C8 raises record 0's obResPt through what was D_8014AA14 (+0x7C4); func_800D60AC hands each car's
 * record to func_800D5E64 by a pointer walk of 0x808 from the array's start; steering_apply reads collidable through
 * what was D_8014AA3A (+0x7EA); menu_audio_settings, the car-to-car collision, walks the cars after one up to the
 * array's end, the next symbol (D_8014D280) on the N64.
 * game: src/blob/func_800E762C.c src/blob/func_800D1248.c src/blob/func_800D0380.c src/blob/groups/path_D169C/path.c
 * game: src/blob/func_800D60AC.c src/blob/steering_apply.c src/blob/menu_audio_settings.c
 */
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

#define ALIAS(name, base, off) __asm__(".globl " #name "\n.set " #name ", " #base " + " #off)

/* ---------------------------------------------------------------- the game's data the code under test uses */

struct pc_model pc_models[PC_MODEL_SLOTS];  /* data.c defines it in the game */
#define SENTINEL 0xEE
#define RAW(i, off) (((const unsigned char *)pc_models)[(i) * 0x808 + (off)])
#define M32(i, off) (*(int32_t *)((uint8_t *)&pc_models[i] + (off)))
#define MF(i, off) (*(float *)((uint8_t *)&pc_models[i] + (off)))
#define M16(i, off) (*(int16_t *)((uint8_t *)&pc_models[i] + (off)))
#define M8(i, off) (*(int8_t *)((uint8_t *)&pc_models[i] + (off)))

int32_t gameplay_mode, D_80143FF4, state_word_a;
float D_8002AFB8 = 1.0f / 60, D_801543CC;
int8_t D_80152744, D_8017A4D8, D_8011EAE4, D_801525F0;
struct { uint16_t count; void *points; } D_801407F0;          /* the track's path graph (path.c's Route) */
uint8_t D_80152818[6 * PC_CAR_SIZE] __attribute__((aligned(16)));   /* the car records, PC layout */
ALIAS(player_array, D_80152818, 0);

/* What the code under test calls: each records what it was given. */
static const void *g_seen[64];
static int g_nseen;
static void seen(const void *p) { if (g_nseen < 64) g_seen[g_nseen] = p; g_nseen++; }
void math_utility(void *src, void *dst) { (void)src; (void)dst; }
void func_800D11BC(void *m) { seen(m); }
void func_800D5E64(void *m) { seen(m); }
void func_8009E820(void *in, float *out, void *basis) { (void)in; (void)basis; out[0] = out[1] = out[2] = 0.0f; }
void func_800A61B0(void *in, float *out, void *basis) { (void)in; (void)basis; out[0] = out[1] = out[2] = 0.0f; }
int func_800CEC8C(void *m, void *local, float f) { (void)local; (void)f; seen(m); return 0; }
void menu_options_screen(void *m, void *a, void *b, void *d, void *l, float f) { (void)m; (void)a; (void)b; (void)d; (void)l; (void)f; }

void func_800E762C(int32_t arg0);
void func_800D1248(void *m);
void func_800D0380(int16_t i);
void func_800D18C8(int32_t index);
void func_800D60AC(void);
int steering_apply(short *player_index, float *point, float *radius_in, float *radial_error);
void menu_audio_settings(void *vehicle);

static void sentinels(void)
{
    memset(pc_models, SENTINEL, sizeof pc_models);
    g_nseen = 0;
}

/* Records from `from` on hold the sentinel in every byte. */
static int untouched_from(int from)
{
    for (int i = from; i < PC_MODEL_SLOTS; i++)
        for (int k = 0; k < 0x808; k++)
            if (RAW(i, k) != SENTINEL)
                return 0;
    return 1;
}

/* Record i holds the sentinel everywhere but [lo, hi) and [lo2, hi2). */
static int only(int i, int lo, int hi, int lo2, int hi2)
{
    for (int k = 0; k < 0x808; k++)
        if (!(k >= lo && k < hi) && !(k >= lo2 && k < hi2) && RAW(i, k) != SENTINEL)
            return 0;
    return 1;
}

static int index_of(const void *p)
{
    ptrdiff_t d = (const uint8_t *)p - (const uint8_t *)pc_models;
    return d >= 0 && d % 0x808 == 0 && d / 0x808 < PC_MODEL_SLOTS ? (int)(d / 0x808) : -1;
}

static void test_layout(void)
{
    CHECK(PC_MODEL_SLOTS == 16 && PC_MAX_LINKS == 16 && PC_N64_MODEL_SLOTS == 6);   /* this test's build */
    CHECK(sizeof(struct pc_model) == 0x808 && sizeof pc_models == 16 * 0x808);
    CHECK(offsetof(struct pc_model, car) == 0x000 && offsetof(struct pc_model, tune) == 0x004);
    CHECK(offsetof(struct pc_model, body) == 0x008 && offsetof(struct pc_model, input_flags) == 0x009);
    CHECK(offsetof(struct pc_model, autotrans) == 0x00A && offsetof(struct pc_model, b00F) == 0x00F);
    CHECK(offsetof(struct pc_model, rwv) == 0x220 && offsetof(struct pc_model, rwr) == 0x22C);
    CHECK(offsetof(struct pc_model, speed) == 0x3F0 && offsetof(struct pc_model, scale) == 0x400);
    CHECK(offsetof(struct pc_model, f544) == 0x544 && offsetof(struct pc_model, viscode) == 0x61C);
    CHECK(offsetof(struct pc_model, crashflag) == 0x640 && offsetof(struct pc_model, topscrape) == 0x641);
    CHECK(offsetof(struct pc_model, resurrect_pos) == 0x660 && offsetof(struct pc_model, moving_state) == 0x6C4);
    CHECK(offsetof(struct pc_model, iteration) == 0x710 && offsetof(struct pc_model, model_time) == 0x714);
    CHECK(offsetof(struct pc_model, dt) == 0x718 && offsetof(struct pc_model, modelrun) == 0x71C);
    CHECK(offsetof(struct pc_model, steer) == 0x720 && offsetof(struct pc_model, brake) == 0x728);
    CHECK(offsetof(struct pc_model, gas) == 0x72C && offsetof(struct pc_model, gear) == 0x730);
    CHECK(offsetof(struct pc_model, flag_b) == 0x731 && offsetof(struct pc_model, flag_a) == 0x732);
    CHECK(offsetof(struct pc_model, suspension) == 0x75C && offsetof(struct pc_model, reckon_rwr) == 0x794);
    CHECK(offsetof(struct pc_model, ob_res_pt) == 0x7C4 && offsetof(struct pc_model, slot) == 0x7C6);
    CHECK(offsetof(struct pc_model, active) == 0x7C8 && offsetof(struct pc_model, we_control) == 0x7CA);
    CHECK(offsetof(struct pc_model, drone_type) == 0x7CC && offsetof(struct pc_model, color) == 0x7CD);
    CHECK(offsetof(struct pc_model, s7D0) == 0x7D0 && offsetof(struct pc_model, flags) == 0x7D4);
    CHECK(offsetof(struct pc_model, stuck_time) == 0x7D8 && offsetof(struct pc_model, can_cheat) == 0x7DE);
    CHECK(offsetof(struct pc_model, hide_car) == 0x7DF && offsetof(struct pc_model, last_cp) == 0x7E2);
    CHECK(offsetof(struct pc_model, drone_target) == 0x7E6 && offsetof(struct pc_model, lap) == 0x7E8);
    CHECK(offsetof(struct pc_model, collidable) == 0x7EA && offsetof(struct pc_model, drone_scale) == 0x7EC);
    CHECK(offsetof(struct pc_model, time_boost) == 0x7F0);
    CHECK(PC_MODEL(3) == (void *)((uint8_t *)pc_models + 3 * 0x808) && PC_MODELS == (void *)pc_models);
    CHECK(PC_MODELS_END == (void *)((uint8_t *)pc_models + 6 * 0x808));
}

/* func_800E762C: the 6 records' model clock (iteration, modelTime, dt at +0x710..+0x71B); nothing else. */
static void test_clock(void)
{
    sentinels();
    gameplay_mode = 0;
    func_800E762C(3);
    CHECK(D_80143FF4 == -3 && D_801543CC == -3 * D_8002AFB8);
    for (int i = 0; i < 6; i++) {
        CHECK(M32(i, 0x710) == -3 && MF(i, 0x714) == D_801543CC && MF(i, 0x718) == D_8002AFB8);
        CHECK(PC_MODEL_ITERATION(i) == -3 && PC_MODEL_DT(i) == D_8002AFB8);
        CHECK(only(i, 0x710, 0x71C, 0, 0));
    }
    CHECK(untouched_from(6));
}

/* func_800D1248 on record 2, a resurrect that ends (+0x6C4 == -2) with slot (+0x7C6) 4: record 2's own fields
 * through the pointer, then record 4's modelrun (+0x71C) through the alias's stride; record 0's is read only. */
static void test_resurrect(void)
{
    sentinels();
    M16(2, 0x6C4) = -2, M16(2, 0x7C6) = 4, M32(2, 0x6BC) = 50, MF(2, 0x660) = 1.5f, MF(2, 0x430) = 2.0f;
    func_800D1248(PC_MODEL(2));
    CHECK(g_nseen == 1 && index_of(g_seen[0]) == 2);   /* func_800D11BC(arg0) */
    CHECK(M16(2, 0x6C4) == -1 && M16(2, 0x71C) == 0 && MF(2, 0x6D4) == 1.5f && M8(2, 0x730) == 2);
    CHECK(M16(4, 0x71C) == 1 && PC_MODEL_MODELRUN(4) == 1 && only(4, 0x71C, 0x71E, 0, 0));
    CHECK(only(0, 0, 0, 0, 0) && only(1, 0, 0, 0, 0) && only(3, 0, 0, 0, 0) && only(5, 0, 0, 0, 0));
    CHECK(untouched_from(6));
    /* func_800D0380: the same field in m2c's spelling, *((s16 *) ((u8 *) &D_8014A96C + (arg0 * 0x808))) */
    sentinels();
    func_800D0380(5);
    CHECK(M16(5, 0x71C) == 1 && only(5, 0x71C, 0x71E, 0, 0) && only(4, 0, 0, 0, 0) && untouched_from(6));
}

/* func_800D18C8: record 0's obResPt (+0x7C4) only goes up. */
static void test_ob_res_pt(void)
{
    sentinels();
    M16(0, 0x7C4) = 2;
    func_800D18C8(1);
    CHECK(M16(0, 0x7C4) == 2);
    func_800D18C8(8);
    CHECK(M16(0, 0x7C4) == 8 && PC_MODEL_OB_RES_PT(0) == 8 && only(0, 0x7C4, 0x7C6, 0, 0));
    CHECK(only(1, 0, 0, 0, 0) && untouched_from(6));
}

/* func_800D60AC: each car's record by a pointer walk of 0x808 from the array's start, D_80152744 of them. */
static void test_walk(void)
{
    sentinels();
    state_word_a = 8, D_80152744 = 5;
    func_800D60AC();
    CHECK(g_nseen == 5);
    for (int i = 0; i < 5; i++)
        CHECK(index_of(g_seen[i]) == i);
    CHECK(D_801525F0 == 1 && untouched_from(0));
}

/* steering_apply: a car near the point counts only if collidable (+0x7EA, what was D_8014AA3A[i]). */
static void test_collidable(void)
{
    float point[3] = {0, 0, 0}, radius = 0.0f, err = 0.0f;
    short index = 3;

    sentinels();
    memset(D_80152818, 0, sizeof D_80152818);
    M8(3, 0x7EA) = 0;
    CHECK(steering_apply(&index, point, &radius, &err) == 0);
    M8(3, 0x7EA) = 1;
    CHECK(steering_apply(&index, point, &radius, &err) == 1 && err == -3.5f * 3.5f);
    M8(3, 0x7EA) = 0, M8(4, 0x7EA) = 1;
    CHECK(steering_apply(&index, point, &radius, &err) == 0);
}

/* menu_audio_settings for record 3: every record is active (+0x7C8) and enabled (+0x7EA) at the same place, so
 * each one after it up to the array's end is tested against it, corner by corner (func_800CEC8C, 4 corners each
 * way): records 4 and 5, not 6 to 15 that the storage has after them. */
static void test_collisions(void)
{
    sentinels();
    for (int i = 0; i < PC_MODEL_SLOTS; i++) {
        M16(i, 0x7C8) = 1, M8(i, 0x7EA) = 1, MF(i, 0x654) = 1.0f;
        for (int k = 0; k < 3; k++)
            MF(i, 0x794 + 4 * k) = 0.0f;
    }
    D_8017A4D8 = 0, gameplay_mode = 0;
    menu_audio_settings(PC_MODEL(3));
    CHECK(g_nseen == 16);
    for (int n = 0; n < 16 && n < g_nseen; n++)
        CHECK(index_of(g_seen[n]) == (n < 4 ? 4 : n < 8 ? 3 : n < 12 ? 5 : 3));
    g_nseen = 0;
    menu_audio_settings(PC_MODEL(5));   /* the last of the 6: nothing after it */
    CHECK(g_nseen == 0);
}

int main(void)
{
    test_layout();
    test_clock();
    test_resurrect();
    test_ob_res_pt();
    test_walk();
    test_collidable();
    test_collisions();
    return TEST_DONE();
}
