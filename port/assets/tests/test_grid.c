/* port/assets/tests/test_grid.c: the start grid for 12 places (docs/port/MORECARS.md 4), through the game's own code:
 * Init_MDrive (func_800E543C, groups/init_mdrive/group.c) places each car (multiinit: its place, or a respawn's
 * index + PC_RESPAWN_BASE) and Resurrect (stunt_combo_display, groups/resurrect/resurrect.c) offsets it from its start
 * point: places 0-5 from the start point by the table's rows, places 6-11 from the point of the resurrection path
 * their row's distance behind it (pc_grid_start). The table and the paths here are the test's own (pole_pos_offset is
 * D_801210E8 in the game, data the ROM fills; nothing of the ROM's is used): the table in two shapes, columns
 * staggered forward and rows shifted sideways every other row; the path straight, bent, too short, and through its
 * loop point. Checked: places 0-5 start at the start point with the table's rows, bit for bit, as on the N64; places
 * 6-11's rows continue each column parity two rows back by its two-row step; on a straight path they stand where the
 * straight rows are, on a bent one at their distance along it, facing the path's segment there; where the path runs
 * out the rest is straight back; twelve cars placed by Init_MDrive stand no two closer than the table's own closest
 * pair or a car's length; the attract demo keeps straight rows; a human's respawn reads the table's respawn rows (slot
 * + 6, row 6 in a ghost race) and a drone's none, as on the N64; battle gives no offset.
 * game: src/blob/groups/resurrect/resurrect.c src/blob/groups/init_mdrive/group.c
 */
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

#define ALIAS(name, base, off) __asm__(".globl " #name "\n.set " #name ", " #base " + " #off)

/* ---------------------------------------------------------------- the game's data the code under test uses */

struct pc_model pc_models[PC_MODEL_SLOTS];
struct pc_car pc_cars[PC_CAR_SLOTS];
struct pc_link pc_links[PC_LINK_SLOTS];
float D_801210E8[12][3];   /* pole_pos_offset: this test's own rows */
float D_8011418C[3][3], D_80117360[4], D_80154390;
uint32_t D_801174B4;       /* gGameState: 8 the attract demo */
int8_t D_801407B0, D_80142760;
int16_t D_8014A108, D_80152734;
volatile int16_t D_801525F0;
int32_t D_8014A110;
ALIAS(gameplay_mode, D_8014A110, 0);
uint8_t D_8012E5E8[1024] __attribute__((aligned(16)));
/* gResPath (resurrect.c's tResPath, host layout) and gCheckpoints (tCheckpoints: loopPt at +2, the points from +0xC,
 * 0x50 bytes each, a point's pathPts[0] at +0x22) */
struct { uint16_t numPts; int16_t (*pts)[3]; uint8_t numAlt; void *alt; } D_801407F0;
uint8_t D_80151CE8[0xC + 10 * 0x50] __attribute__((aligned(16)));

/* What it calls: Resurrect's start branch calls func_800D11BC (syminit) with the record it placed; camera_follow_path
 * (FindSurfaceUvsFromResPoint) faces the path's segment from a point on: here it records the point and gives the
 * identity. */
static int g_placed[PC_MODEL_SLOTS];
void func_800D11BC(void *m)
{
    ptrdiff_t d = (uint8_t *)m - (uint8_t *)pc_models;
    if (d >= 0 && d % 0x808 == 0 && d / 0x808 < PC_MODEL_SLOTS)
        g_placed[d / 0x808]++;
}
static int g_faced = -1;
void camera_follow_path(int res, int path, int idx, float (*uvs)[3])
{
    g_faced = res == 1 && path == -1 ? idx : -2;
    for (int i = 0; i < 3; i++)
        for (int k = 0; k < 3; k++)
            uvs[i][k] = i == k;
}
static uint8_t g_surface[16];
void *camera_trigger_check(void *a, void *b, void *c) { (void)a; (void)b; (void)c; return g_surface; }
int difficulty_select(int a, int b, int c, int d) { (void)a; (void)b; (void)c; (void)d; return 0; }
float func_8008B2E4(float max) { (void)max; return 0.0f; }
void func_8009E820(float *s, float *d, void *u) { (void)u; d[0] = s[0], d[1] = s[1], d[2] = s[2]; }
void func_800C4180(void *a, float *b) { (void)a; (void)b; }
void func_800C4C9C(void *m, int16_t i) { (void)m; (void)i; }
void func_800D14F4(void *a, float *b) { (void)a; (void)b; }
int16_t func_800D2C10(int16_t *p, int16_t q) { (void)p; (void)q; return 0; }
int func_800D3430(int a, int b, int *c, int *d, int e) { (void)a; (void)b; (void)c; (void)d; (void)e; return 0; }
void func_8038CA24(int s) { (void)s; }
void fyaw(float s, float c, void *u) { (void)s; (void)c; (void)u; }
void lap_count_select(int a, int b, int c, int d, int *e, int *f) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; }
void math_utility(void *s, void *d) { memcpy(d, s, 36); }
float n64_sinf(float x) { return sinf(x); }
float n64_cosf(float x) { return cosf(x); }
void race_countdown_display(void *m, float t) { (void)m; (void)t; }
int viDeadlinePassed(void) { return 0; }
void car_setup_confirm(int i, float f) { (void)i; (void)f; }
void func_800C4F68(int16_t mode, void *m, uint32_t f) { (void)mode; (void)m; (void)f; }
void func_800D5E64(void *m) { (void)m; }

void func_800E543C(int16_t action, int16_t index);

/* The model record's fields Resurrect and multiinit reach (tModelData, N64 offsets = PC offsets). */
#define MF(i, off) ((float *)((uint8_t *)&pc_models[i] + (off)))
#define M16(i, off) (*(int16_t *)((uint8_t *)&pc_models[i] + (off)))
#define SAVE_POS 0x66C     /* resurrect.save_pos: the start point SetResurrectionData gives */
#define SAVE_UVS 0x678     /* resurrect.uvs: its orientation */
#define CRASH_TIME 0x6C0   /* resurrect.crash_time */
#define POLEPOS 0x6D0      /* initin.polepos */
#define POS 0x6D4          /* initin.pos: the point the car starts from */
#define UVS 0x6E0          /* initin.uvs */
#define OFFSET 0x704       /* initin.offset */
#define LAST_CP 0x7E2      /* last_cp_index: the start checkpoint */
#define CAR_LENGTH 14.0f   /* feet: the grid's rows are a car apart and more */

/* Two tables of the test's own: forward-staggered columns, and rows shifted sideways every other row. Rows 6-9 are
 * the humans' respawns, 10-11 read by nothing (a value the code must not use). */
static const float TABLE_A[12][3] = {
    {-7, 0, 0}, {7, 0, -10}, {-7, 0, -25}, {7, 0, -35}, {-7, 0, -50}, {7, 0, -60},
    {-4, 0, 5}, {4, 0, 5}, {-12, 0, 5}, {12, 0, 5}, {999, 999, 999}, {999, 999, 999}};
static const float WANT_A[6][3] = {{-7, 0, -75}, {7, 0, -85}, {-7, 0, -100}, {7, 0, -110}, {-7, 0, -125}, {7, 0, -135}};
static const float TABLE_B[12][3] = {
    {-5, 0, 0}, {10, 0, 0}, {-12, 0, -22}, {3, 0, -22}, {-5, 0, -44}, {10, 0, -44},
    {-2, 0, 0}, {2, 0, 0}, {-15, 0, 0}, {15, 0, 0}, {999, 999, 999}, {999, 999, 999}};
static const float WANT_B[6][3] = {{-12, 0, -66}, {3, 0, -66}, {-5, 0, -88}, {10, 0, -88}, {-12, 0, -110}, {3, 0, -110}};

/* The paths: the start point is index START, the path runs forward (+z) through it in steps of 10 ft. */
#define START 20
static int16_t g_pts[64][3];
static const float g_start[3] = {100.0f, 5.0f, -300.0f};

static void path(int n, int start, int loop_point, int bend_at)
{
    for (int k = 0; k < n; k++) {
        g_pts[k][0] = 100, g_pts[k][1] = 5, g_pts[k][2] = (int16_t)(-300 + (k - start) * 10);
        if (k < bend_at)   /* before the bend the path comes along +x, at the bend's z */
            g_pts[k][0] = (int16_t)(100 - (bend_at - k) * 10), g_pts[k][2] = (int16_t)(-300 + (bend_at - start) * 10);
    }
    D_801407F0.numPts = (uint16_t)n;
    D_801407F0.pts = g_pts;
    memset(D_80151CE8, 0, sizeof D_80151CE8);
    int16_t one = 1, sp = (int16_t)start, lp = (int16_t)loop_point;
    memcpy(D_80151CE8 + 2, &one, 2);                         /* loopPt: checkpoint 1 */
    memcpy(D_80151CE8 + 0xC + 0 * 0x50 + 0x22, &sp, 2);       /* checkpoint 0 (the start's): its path point */
    memcpy(D_80151CE8 + 0xC + 1 * 0x50 + 0x22, &lp, 2);       /* checkpoint 1: the loop's first path point */
}

static float dist(const float *a, const float *b)
{
    float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
    return sqrtf(x * x + y * y + z * z);
}

static float closest(const float (*rows)[3], int n)
{
    float m = 1e30f;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (dist(rows[i], rows[j]) < m)
                m = dist(rows[i], rows[j]);
    return m;
}

static int near(const float *a, float x, float y, float z)
{
    return fabsf(a[0] - x) < 1e-3f && fabsf(a[1] - y) < 1e-3f && fabsf(a[2] - z) < 1e-3f;
}

static void test_rows(const float table[12][3], const float want[6][3])
{
    memcpy(D_801210E8, table, sizeof D_801210E8);
    CHECK(PC_RESPAWN_BASE == 12 && PC_MAX_LINKS == 12);
    for (int p = 0; p < 6; p++)   /* the N64's grid, bit for bit */
        CHECK(memcmp(pc_pole_row(p), D_801210E8[p], 12) == 0);
    for (int p = 6; p < 12; p++)
        CHECK(memcmp(pc_pole_row(p), want[p - 6], 12) == 0);
    for (int s = 0; s < 4; s++)   /* a respawn: the table's own respawn row */
        CHECK(pc_pole_row(PC_RESPAWN_BASE + s) == D_801210E8[6 + s]);
}

/* Init_MDrive for slots 0..n-1: slot 0 the human, at the back (a human's place is the drones' count plus its
 * index), the drones 1..n-1 at places 0..n-2, every car at the start point facing +z (SetResurrectionData's). pos[i]
 * is where car i stands: its start point plus its offset (the orientation is the identity throughout). */
static void place(int n, int action, float pos[][3])
{
    memset(pc_models, 0, sizeof pc_models);
    memset(pc_cars, 0, sizeof pc_cars);
    memset(pc_links, 0, sizeof pc_links);
    memset(g_placed, 0, sizeof g_placed);
    for (int i = 0; i < n; i++) {
        PC_LINK_OWNER(i) = i == 0 ? 6 : 0;
        PC_LINK_PLACE(i) = i == 0 ? n - 1 : i - 1;
        PC_LINK_FLAGS(i) = 0xB0;
        PC_MODEL_SLOT(i) = i;
        *MF(i, CRASH_TIME) = 0.0f;
        memcpy(MF(i, SAVE_POS), g_start, 12);
        for (int k = 0; k < 9; k++)
            MF(i, SAVE_UVS)[k] = k % 4 == 0;
    }
    for (int i = 0; i < n; i++) {
        func_800E543C(action, i);
        for (int k = 0; k < 3; k++)
            pos[i][k] = MF(i, POS)[k] + MF(i, OFFSET)[k];
    }
}

static int place_of(int i, int n) { return i == 0 ? n - 1 : i - 1; }

static void test_straight(void)
{
    float pos[12][3];
    D_8014A110 = 0;   /* a single race */
    D_801174B4 = 0;
    path(40, START, 0, 0);
    for (int t = 0; t < 2; t++) {
        const float (*table)[3] = t ? TABLE_B : TABLE_A, (*want)[3] = t ? WANT_B : WANT_A;
        memcpy(D_801210E8, table, sizeof D_801210E8);
        place(12, 0, pos);
        for (int i = 0; i < 12; i++) {
            int p = place_of(i, 12);
            const float *row = p < 6 ? table[p] : want[p - 6];
            CHECK(g_placed[i] == 1 && M16(i, POLEPOS) == p);
            if (p < 6)   /* the start point, the table's row: the N64's */
                CHECK(memcmp(MF(i, POS), g_start, 12) == 0 && memcmp(MF(i, OFFSET), row, 12) == 0);
            else   /* the path's point the row's distance back, the row's sideways offset */
                CHECK(near(MF(i, POS), g_start[0], g_start[1], g_start[2] + row[2]) && MF(i, OFFSET)[0] == row[0] &&
                      MF(i, OFFSET)[2] == 0.0f);
            CHECK(near(pos[i], g_start[0] + row[0], g_start[1] + row[1], g_start[2] + row[2]));
        }
        float m = closest((const float (*)[3])pos, 12);
        CHECK(m >= closest(table, 6) && m >= CAR_LENGTH);
    }
    /* Six cars: the N64's grid, the human at place 5. */
    memcpy(D_801210E8, TABLE_A, sizeof D_801210E8);
    place(6, 0, pos);
    for (int i = 0; i < 6; i++)
        CHECK(memcmp(MF(i, POS), g_start, 12) == 0 && memcmp(MF(i, OFFSET), TABLE_A[place_of(i, 6)], 12) == 0);
}

static void test_bent(void)
{
    float pos[12][3];
    memcpy(D_801210E8, TABLE_A, sizeof D_801210E8);
    D_8014A110 = 0;
    D_801174B4 = 0;
    /* The path turns 90 degrees 30 ft behind the start: before point 17 it comes along +x. Place 6 is 75 ft back:
     * 30 ft to the bend, 45 along x, halfway between points 13 and 12, facing the segment from 12. */
    path(40, START, 0, START - 3);
    place(12, 0, pos);
    CHECK(near(MF(7, POS), 55.0f, 5.0f, -330.0f) && MF(7, OFFSET)[0] == -7.0f && MF(7, OFFSET)[2] == 0.0f);
    path(40, START, 0, START - 3);
    func_800E543C(0, 7);
    CHECK(g_faced == 12);
    /* Place 11 is 135 ft back: 30 to the bend, 105 along x. */
    CHECK(near(MF(0, POS), -5.0f, 5.0f, -330.0f) && MF(0, OFFSET)[0] == 7.0f);
    for (int i = 0; i < 12; i++)   /* every car on the path's line, its row's sideways offset from it */
        CHECK(MF(i, POS)[1] == 5.0f);
    /* A path that runs out: three points behind the start, 30 ft; place 6's other 45 ft are straight back. */
    path(4, 3, 99, 0);
    place(12, 0, pos);
    CHECK(near(MF(7, POS), 100.0f, 5.0f, -330.0f) && near(MF(7, OFFSET), -7.0f, 0.0f, -45.0f));
    /* Through the loop: the start is the loop's first point, 0; the point before it is the path's last. */
    path(30, 0, 0, 0);
    for (int k = 0; k < 30; k++)   /* the loop comes back along +z to point 0 */
        g_pts[k][2] = (int16_t)(k == 0 ? -300 : -300 - (30 - k) * 10);
    place(12, 0, pos);
    CHECK(near(MF(7, POS), 100.0f, 5.0f, -375.0f));
    path(30, 0, 0, 0);
    for (int k = 0; k < 30; k++)
        g_pts[k][2] = (int16_t)(k == 0 ? -300 : -300 - (30 - k) * 10);
    func_800E543C(0, 7);
    CHECK(g_faced == 22);   /* 75 ft back from point 0: between 22 and 23 (the loop's last, 29, is 10 ft back) */
}

static void test_demo_and_respawns(void)
{
    float pos[12][3];
    memcpy(D_801210E8, TABLE_A, sizeof D_801210E8);
    path(40, START, 0, START - 3);
    /* The attract demo starts each car on its own drone path: straight rows from its start point. (It also reads its
     * drone path's speed at the start checkpoint: gDronePaths[0], tDronePath {u16 numPts; tDronePt *pts;}.) */
    static uint8_t demo_pts[8 * 8];
    void *pp = demo_pts;
    memcpy(D_8012E5E8 + 8, &pp, sizeof pp);
    D_8014A110 = 0;
    D_801174B4 = 8;
    place(12, 0, pos);
    CHECK(memcmp(MF(7, POS), g_start, 12) == 0 && memcmp(MF(7, OFFSET), WANT_A[0], 12) == 0);
    D_801174B4 = 0;
    /* A respawn (Init_MDrive's action 1): index + PC_RESPAWN_BASE from the start point; a human's offset is the
     * table's row index + 6, a drone's none. Slots 0 and 2 human. */
    place(12, 0, pos);
    PC_LINK_OWNER(2) = 6;
    for (int i = 0; i < 4; i++) {
        func_800E543C(1, i);
        CHECK(M16(i, POLEPOS) == PC_RESPAWN_BASE + i && memcmp(MF(i, POS), g_start, 12) == 0);
    }
    CHECK(memcmp(MF(0, OFFSET), TABLE_A[6], 12) == 0 && memcmp(MF(2, OFFSET), TABLE_A[8], 12) == 0);
    CHECK(MF(1, OFFSET)[0] == 0.0f && MF(1, OFFSET)[2] == 0.0f && MF(3, OFFSET)[0] == 0.0f);
    /* A ghost race: every start at place 0, a respawn at PC_RESPAWN_BASE, which is the table's row 6. */
    D_8014A110 = 2;
    func_800E543C(0, 0);
    CHECK(M16(0, POLEPOS) == 0 && memcmp(MF(0, OFFSET), TABLE_A[0], 12) == 0);
    func_800E543C(1, 0);
    CHECK(M16(0, POLEPOS) == PC_RESPAWN_BASE && memcmp(MF(0, OFFSET), TABLE_A[6], 12) == 0);
    /* Battle: no offsets, the start point. */
    D_8014A110 = 6;
    PC_LINK_PLACE(5) = 8;
    func_800E543C(0, 5);
    CHECK(MF(5, OFFSET)[0] == 0.0f && MF(5, OFFSET)[2] == 0.0f && memcmp(MF(5, POS), g_start, 12) == 0);
    D_8014A110 = 0;
}

int main(void)
{
    test_rows(TABLE_A, WANT_A);
    test_rows(TABLE_B, WANT_B);
    test_straight();
    test_bent();
    test_demo_and_respawns();
    return TEST_DONE();
}
