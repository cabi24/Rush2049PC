/* port/assets/tests/test_models.c: the model records (MAT3 model[]: pc_models, tModelData,
 * include/game/modeldata.h) through the game's own readers and writers, with more records declared than the
 * game walks (docs/port/DECOUPLE.md). This test and its game objects are built with PC_MAX_LINKS 16
 * (CMakeLists.txt), so the storage has 16 records where the game build has 12 and the N64 6. Every record starts as
 * a sentinel: the code must write and read the fields where the record's host layout has them (MAT3's offsets, the
 * five pointers 8 bytes wide: MODEL_OFS, support.h), step by the record's size (0x830) and stop at the 6 it walks
 * on the N64, whatever the storage's size.
 * init_model_task sets the 6 records' model clock (an index walk); FinishResurrecting ends a resurrect through a record
 * pointer and sets another record's modelrun through what was the alias model_modelrun (+0x71C, record by record),
 * whose record 0 it also reads as a scalar; multigo sets it through the same alias; raise_ob_res_pt raises record
 * 0's obResPt (+0x7C4, a label of its own on the N64); modelgo hands each car's record to StartCarSounds by index
 * from the array's start; TargetCollRadius reads collidable
 * (+0x7EA, another label); collision, the car-to-car collision, walks the cars after one up to the array's end, the
 * next symbol (gTargetArray) on the N64. A race of more cars (docs/port/MORECARS.md 3.1), test_race_cars 12 (what
 * pc_race_cars() returns, support.c): the clock of 12 records, and the collision's walk to the 12th, the end of the
 * race's cars.
 * game: src/blob/init_model_task.c src/blob/FinishResurrecting.c src/blob/multigo.c src/blob/groups/get_res_pt/GetResPt.c
 * game: src/blob/modelgo.c src/blob/TargetCollRadius.c src/blob/collision.c
 */
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"
#include "game/modeldata.h"

#define ALIAS(name, base, off) __asm__(".globl " #name "\n.set " #name ", " #base " + " #off)

/* ---------------------------------------------------------------- the game's data the code under test uses */

tModelData pc_models[PC_MODEL_SLOTS];  /* data.c defines it in the game */
#define SENTINEL 0xEE
#define SIZE sizeof(tModelData)
#define RAW(i, off) (((const unsigned char *)pc_models)[(i) * SIZE + (off)])
#define M32(i, off) (*(int32_t *)((uint8_t *)&pc_models[i] + MODEL_OFS(off)))
#define MF(i, off) (*(float *)((uint8_t *)&pc_models[i] + MODEL_OFS(off)))
#define M16(i, off) (*(int16_t *)((uint8_t *)&pc_models[i] + MODEL_OFS(off)))
#define M8(i, off) (*(int8_t *)((uint8_t *)&pc_models[i] + MODEL_OFS(off)))

int32_t gRace, gModelIteration, gGameState;
float gRetraceTime = 1.0f / 60, gModelTime;
int8_t gNumActiveCars, gNoCarCollisions, gRumble;
uint16_t modelrun;   /* volatile u16 in the game (physics.h) */
struct { uint16_t count; void *points; } gResPath;          /* the track's path graph (path.c's Route) */
struct pc_car pc_cars[PC_CAR_SLOTS];   /* the car records (pc_game_types.h), PC layout */

/* What the code under test calls: each records what it was given. */
static const void *g_seen[64];
static int g_nseen;
static void seen(const void *p) { if (g_nseen < 64) g_seen[g_nseen] = p; g_nseen++; }
void CopyUV(void *src, void *dst) { (void)src; (void)dst; }
void syminit(void *m) { seen(m); }
void StartCarSounds(void *m) { seen(m); }
void WorldVector(void *in, float *out, void *basis) { (void)in; (void)basis; out[0] = out[1] = out[2] = 0.0f; }
void BodyVector(void *in, float *out, void *basis) { (void)in; (void)basis; out[0] = out[1] = out[2] = 0.0f; }
int PointInBody(void *m, void *local, float f) { (void)local; (void)f; seen(m); return 0; }
void setFBCollisionForce(void *m, void *a, void *b, void *d, void *l, float f) { (void)m; (void)a; (void)b; (void)d; (void)l; (void)f; }

void init_model_task(int32_t arg0);
void FinishResurrecting(void *m);
void multigo(int16_t i);
void raise_ob_res_pt(int32_t index);
void modelgo(void);
int TargetCollRadius(short *player_index, float *point, float *radius_in, float *radial_error);
void collision(void *vehicle);

static void sentinels(void)
{
    memset(pc_models, SENTINEL, sizeof pc_models);
    g_nseen = 0;
}

/* Records from `from` on hold the sentinel in every byte. */
static int untouched_from(int from)
{
    for (int i = from; i < PC_MODEL_SLOTS; i++)
        for (size_t k = 0; k < SIZE; k++)
            if (RAW(i, k) != SENTINEL)
                return 0;
    return 1;
}

/* Record i holds the sentinel everywhere but MAT3's [lo, hi) and [lo2, hi2) (ranges without a pointer). */
static int only(int i, int lo, int hi, int lo2, int hi2)
{
    for (size_t k = 0; k < SIZE; k++)
        if (!(k >= MODEL_OFS(lo) && k < MODEL_OFS(hi)) && !(k >= MODEL_OFS(lo2) && k < MODEL_OFS(hi2)) &&
            RAW(i, k) != SENTINEL)
            return 0;
    return 1;
}

static int index_of(const void *p)
{
    ptrdiff_t d = (const uint8_t *)p - (const uint8_t *)pc_models;
    return d >= 0 && d % SIZE == 0 && d / SIZE < PC_MODEL_SLOTS ? (int)(d / SIZE) : -1;
}

static void test_layout(void)
{
    CHECK(PC_MODEL_SLOTS == 16 && PC_MAX_LINKS == 16 && PC_N64_MODEL_SLOTS == 6);   /* this test's build */
    CHECK(sizeof(tModelData) == 0x830 && sizeof pc_models == 16 * 0x830 && MODEL_OFS(0x808) == 0x82C);
    /* the pointers: 8 bytes each, torquecurve after a 4-byte hole */
    CHECK(offsetof(tModelData, car) == 0x000 && offsetof(tModelData, tune) == 0x008);
    CHECK(offsetof(tModelData, lasttp) == 0x3A8 && sizeof pc_models[0].lasttp == 32);
    CHECK(offsetof(tModelData, torquecurve) == 0x428 && offsetof(tModelData, movingSurface) == 0x668);
    CHECK(MODEL_OFS(0x3A0) == 0x3A8 && MODEL_OFS(0x40C) == 0x428 && MODEL_OFS(0x648) == 0x668);
    /* the fields at MAT3's offsets as the host lays them out (MODEL_OFS): a sample of each part of the record */
    static const struct { size_t host; unsigned mat3; const char *name; } fields[] = {
#define FIELD(f, off) {offsetof(tModelData, f), off, #f}
        FIELD(body_type, 0x008), FIELD(trans, 0x009), FIELD(autotrans, 0x00A), FIELD(tireRims, 0x00F),
        FIELD(f, 0x010), FIELD(v, 0x040), FIELD(tireForce, 0x064), FIELD(bodyR, 0x0F4), FIELD(peak_body_force, 0x148),
        FIELD(tireuvs, 0x178), FIELD(rwv, 0x220), FIELD(rwr, 0x22C), FIELD(uvs, 0x2EC), FIELD(roaduvs, 0x310),
        FIELD(steerangle, 0x3B0), FIELD(magvel, 0x3F0), FIELD(wingSnd, 0x3FC), FIELD(catchup, 0x400),
        FIELD(engangvel, 0x408), FIELD(dwtorque, 0x410), FIELD(tires, 0x430), FIELD(tires[3].tradius, 0x544),
        FIELD(tires[0].slipflag, 0x488), FIELD(tiresli, 0x5A0), FIELD(wheelbase, 0x5C8), FIELD(suscomp, 0x5CC),
        FIELD(roadcode, 0x60C), FIELD(sviscode, 0x61C), FIELD(roadboost, 0x624), FIELD(dt, 0x634),
        FIELD(crashflag, 0x640), FIELD(topscrape, 0x641), FIELD(thump_side, 0x644), FIELD(thumpthreshold, 0x64C),
        FIELD(colrad, 0x654), FIELD(bogState, 0x65C), FIELD(resurrect.pos, 0x660), FIELD(resurrect.velocity, 0x6BC),
        FIELD(resurrect.moving_state, 0x6C4), FIELD(resurrect.autoAbort, 0x6CC), FIELD(initin.polepos, 0x6D0),
        FIELD(initin.pos, 0x6D4), FIELD(initin.offset, 0x704), FIELD(fastin.modelIteration, 0x710),
        FIELD(fastin.modelTime, 0x714), FIELD(fastin.dt, 0x718), FIELD(fastin.modelrun, 0x71C),
        FIELD(mainin.wheel, 0x720), FIELD(mainin.brake, 0x728), FIELD(mainin.throttle, 0x72C),
        FIELD(mainin.gear, 0x730), FIELD(mainin.wing, 0x731), FIELD(mainin.abort, 0x732),
        FIELD(reckon.base_rwa, 0x734), FIELD(reckon.rwmph, 0x758), FIELD(reckon.suscomp, 0x75C),
        FIELD(reckon.rwr, 0x794), FIELD(reckon.uvs, 0x7A0), FIELD(obResPt, 0x7C4), FIELD(slot, 0x7C6),
        FIELD(in_game, 0x7C8), FIELD(we_control, 0x7CA), FIELD(drone_type, 0x7CC), FIELD(color, 0x7CD),
        FIELD(rpm, 0x7D0), FIELD(appearance, 0x7D4), FIELD(stuck_time, 0x7D8), FIELD(started, 0x7DC),
        FIELD(can_we_cheat, 0x7DE), FIELD(hide_car, 0x7DF), FIELD(please_abort_me, 0x7E0),
        FIELD(last_cp_index, 0x7E2), FIELD(drone_target, 0x7E6), FIELD(laps, 0x7E8), FIELD(collidable, 0x7EA),
        FIELD(solid, 0x7EB), FIELD(drone_scale, 0x7EC), FIELD(time_boost, 0x7F0), FIELD(time_fudge, 0x7F4),
        FIELD(skidIntensity, 0x7F8),
#undef FIELD
    };
    for (size_t k = 0; k < sizeof fields / sizeof fields[0]; k++) {
        CHECK(fields[k].host == MODEL_OFS(fields[k].mat3));
        if (fields[k].host != MODEL_OFS(fields[k].mat3))
            fprintf(stderr, "    %s at host 0x%zX, MAT3 0x%X\n", fields[k].name, fields[k].host, fields[k].mat3);
    }
    CHECK(&pc_models[3] == (void *)((uint8_t *)pc_models + 3 * SIZE));
    test_race_cars = 6;
}

/* init_model_task: the race's records' model clock (iteration, modelTime, dt at +0x710..+0x71B), the N64's 6 or 12;
 * nothing else. */
static void model_clock(int cars)
{
    sentinels();
    test_race_cars = cars;
    gRace = 0;
    init_model_task(3);
    CHECK(gModelIteration == -3 && gModelTime == -3 * gRetraceTime);
    for (int i = 0; i < cars; i++) {
        CHECK(M32(i, 0x710) == -3 && MF(i, 0x714) == gModelTime && MF(i, 0x718) == gRetraceTime);
        CHECK(pc_models[i].fastin.modelIteration == -3 && pc_models[i].fastin.dt == gRetraceTime);
        CHECK(only(i, 0x710, 0x71C, 0, 0));
    }
    CHECK(untouched_from(cars));
    test_race_cars = 6;
}

static void test_clock(void)
{
    model_clock(6);
    model_clock(12);
}

/* FinishResurrecting on record 2, a resurrect that ends (+0x6C4 == -2) with slot (+0x7C6) 4: record 2's own fields
 * through the pointer, then record 4's modelrun (+0x71C) through the alias's stride; record 0's is read only. */
static void test_resurrect(void)
{
    sentinels();
    M16(2, 0x6C4) = -2, M16(2, 0x7C6) = 4, M32(2, 0x6BC) = 50, MF(2, 0x660) = 1.5f, MF(2, 0x430) = 2.0f;
    FinishResurrecting(&pc_models[2]);
    CHECK(g_nseen == 1 && index_of(g_seen[0]) == 2);   /* syminit(arg0) */
    CHECK(M16(2, 0x6C4) == -1 && M16(2, 0x71C) == 0 && MF(2, 0x6D4) == 1.5f && M8(2, 0x730) == 2);
    CHECK(M16(4, 0x71C) == 1 && pc_models[4].fastin.modelrun == 1 && only(4, 0x71C, 0x71E, 0, 0));
    CHECK(only(0, 0, 0, 0, 0) && only(1, 0, 0, 0, 0) && only(3, 0, 0, 0, 0) && only(5, 0, 0, 0, 0));
    CHECK(untouched_from(6));
    /* multigo: the same field through record arg0 */
    sentinels();
    multigo(5);
    CHECK(M16(5, 0x71C) == 1 && only(5, 0x71C, 0x71E, 0, 0) && only(4, 0, 0, 0, 0) && untouched_from(6));
}

/* raise_ob_res_pt: record 0's obResPt (+0x7C4) only goes up. */
static void test_ob_res_pt(void)
{
    sentinels();
    M16(0, 0x7C4) = 2;
    raise_ob_res_pt(1);
    CHECK(M16(0, 0x7C4) == 2);
    raise_ob_res_pt(8);
    CHECK(M16(0, 0x7C4) == 8 && pc_models[0].obResPt == 8 && only(0, 0x7C4, 0x7C6, 0, 0));
    CHECK(only(1, 0, 0, 0, 0) && untouched_from(6));
}

/* modelgo: each car's record by index from the array's start, gNumActiveCars of them. */
static void test_walk(void)
{
    sentinels();
    gGameState = 8, gNumActiveCars = 5;
    modelgo();
    CHECK(g_nseen == 5);
    for (int i = 0; i < 5; i++)
        CHECK(index_of(g_seen[i]) == i);
    CHECK(modelrun == 1 && untouched_from(0));
}

/* TargetCollRadius: a car near the point counts only if collidable (+0x7EA, record i's on the N64). */
static void test_collidable(void)
{
    float point[3] = {0, 0, 0}, radius = 0.0f, err = 0.0f;
    short index = 3;

    sentinels();
    memset(pc_cars, 0, sizeof pc_cars);
    M8(3, 0x7EA) = 0;
    CHECK(TargetCollRadius(&index, point, &radius, &err) == 0);
    M8(3, 0x7EA) = 1;
    CHECK(TargetCollRadius(&index, point, &radius, &err) == 1 && err == -3.5f * 3.5f);
    M8(3, 0x7EA) = 0, M8(4, 0x7EA) = 1;
    CHECK(TargetCollRadius(&index, point, &radius, &err) == 0);
}

/* collision for record 3: every record is active (+0x7C8) and enabled (+0x7EA) at the same place, so
 * each one after it up to the array's end is tested against it, corner by corner (PointInBody, 4 corners each
 * way): records 4 and 5, not 6 to 15 that the storage has after them. */
static void test_collisions(void)
{
    sentinels();
    for (int i = 0; i < PC_MODEL_SLOTS; i++) {
        M16(i, 0x7C8) = 1, M8(i, 0x7EA) = 1, MF(i, 0x654) = 1.0f;
        for (int k = 0; k < 3; k++)
            MF(i, 0x794 + 4 * k) = 0.0f;
    }
    gNoCarCollisions = 0, gRace = 0;
    collision(&pc_models[3]);
    CHECK(g_nseen == 16);
    for (int n = 0; n < 16 && n < g_nseen; n++)
        CHECK(index_of(g_seen[n]) == (n < 4 ? 4 : n < 8 ? 3 : n < 12 ? 5 : 3));
    g_nseen = 0;
    collision(&pc_models[5]);   /* the last of the 6: nothing after it */
    CHECK(g_nseen == 0);

    /* 12 cars: from record 7, records 8 to 11, not 12 to 15 */
    test_race_cars = 12;
    g_nseen = 0;
    collision(&pc_models[7]);
    CHECK(g_nseen == 32);
    for (int n = 0; n < 32 && n < g_nseen; n++)
        CHECK(index_of(g_seen[n]) == (n % 8 < 4 ? 8 + n / 8 : 7));
    g_nseen = 0;
    collision(&pc_models[3]);   /* from record 3: the 8 after it */
    CHECK(g_nseen == 64 && index_of(g_seen[56]) == 11 && index_of(g_seen[63]) == 3);
    g_nseen = 0;
    collision(&pc_models[11]);  /* the last of the 12 */
    CHECK(g_nseen == 0);
    test_race_cars = 6;
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
