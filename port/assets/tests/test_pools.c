/* port/assets/tests/test_pools.c: the pools more cars a race fill (docs/port/MORECARS.md 8), through the game's own
 * code, with the pools shrunk (CMakeLists.txt: PC_MATRIX_SPILL 4, PC_FRAME_MATRICES 8, PC_GLIST_LEN 32) so that the
 * checks themselves are what is tested. GetMatrix (func_800A7D6C) hands out the N64's 150 persistent matrices at
 * D_8015F740 (index 0 unused), then pc_matrix_spill's, then says it ran out (pc_pool_overflow) and hands out its last
 * one again; world_collision_response (EndGraphics) closes the frame's list in the frame's buffer as pc_game_types.h
 * lays it out and hands the list's and the matrices' ends to pc_frame_pools (port/ultra/pools.c), which says when
 * either ran past its end, or the frame's lookats did, and not when one is exactly full. The handler is this test's
 * (pc_pool_overflow_hook).
 * game: src/blob/func_800A7D6C.c src/blob/world_collision_response.c
 */
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

extern void (*pc_pool_overflow_hook)(const char *pool, int used, int capacity);

/* ---------------------------------------------------------------- what the code under test uses */

/* GetMatrix's */
int32_t D_80156BAC, D_80156BD0;
float D_8015F740[PC_N64_MATRICES][12] __attribute__((aligned(16)));
float pc_matrix_spill[PC_MATRIX_SPILL][12] __attribute__((aligned(16)));   /* data.c's, in the window, in the game */
float D_8011418C[3][3];
static int g_copies;
static const void *g_copied_to;
void math_utility(void *src, void *dst)
{
    if (src == D_8011418C)
        g_copies++;
    g_copied_to = dst;
}
void *func_800A7D6C(void);

/* world_collision_response's */
uint8_t D_80156BE0[2][0xD0] __attribute__((aligned(16)));   /* the frame records (its PC Frame, 0xD0 bytes) */
volatile int8_t D_8015F72D;
void *D_801497C8, *D_80156BA8, *D_8015B250, *D_8015B260;
int32_t D_80156BCC, D_80114730;
intptr_t D_80149438;
uint64_t D_80027DD0[2], D_80027EA0[2], D_80028CC0[2], D_8002DDF0[2], D_8007BA50[2], D_8007BE50[2], D_8007CA50[2],
    D_80084A50[2];
uint32_t D_8002ECF8[6], D_8002E960[6];
uint8_t D_8002E998[16];
int16_t D_8002EB70;
volatile int32_t D_8002EB98;
uint8_t D_8002E8E8[640] __attribute__((aligned(16)));
static uint64_t g_text_dl[2];
static const void *g_sent;
void world_velocity_integrate(int32_t a) { (void)a; }
void *audio_start(void) { return g_text_dl; }
void osStopThread(void *t) { (void)t; }
void osStartThread(void *t) { (void)t; }
int32_t osSendMesg(void *q, void *m, int32_t f) { (void)q; (void)f; g_sent = m; return 0; }
void world_collision_response(void);

/* port/ultra/pools.c's */
int32_t D_801569A8, D_8013C234;
int16_t D_8012E678;
int8_t D_80124F84;   /* the frame's lookats so far */
const char *pc_os_thread_name(void) { return "test"; }
void pc_print_backtrace(int skip) { (void)skip; }

/* The pools' handler: what ran past its end, and by how much. */
static int g_overflows, g_used, g_capacity;
static const char *g_pool;
static void overflow(const char *pool, int used, int capacity)
{
    g_overflows++;
    g_pool = pool, g_used = used, g_capacity = capacity;
}

/* ---------------------------------------------------------------- GetMatrix */

static void test_matrices(void)
{
    CHECK(PC_N64_MATRICES == 150 && PC_MATRIX_SPILL == 4 && PC_MATRICES == 154);   /* this test's build */
    memset(D_8015F740, 0x7F, sizeof D_8015F740);
    memset(pc_matrix_spill, 0x7F, sizeof pc_matrix_spill);
    D_80156BAC = 0;
    D_80156BD0 = 0;
    g_overflows = g_copies = 0;
    /* The N64's: indexes 1-149 at their N64 places, each cleared of its translation and set to the identity. */
    for (int i = 1; i < PC_N64_MATRICES; i++) {
        float *e = func_800A7D6C();
        CHECK(e == D_8015F740[i] && g_copied_to == e);
        CHECK(e[9] == 0.0f && e[10] == 0.0f && e[11] == 0.0f && e[8] != 0.0f);
    }
    CHECK(D_80156BAC == 149 && D_80156BD0 == 149 && g_copies == 149 && g_overflows == 0);
    CHECK(D_8015F740[0][0] != 0.0f);   /* index 0 is never handed out */
    /* Then the spill, in order. */
    for (int i = 0; i < PC_MATRIX_SPILL; i++) {
        float *e = func_800A7D6C();
        CHECK(e == pc_matrix_spill[i] && e[9] == 0.0f && e[11] == 0.0f);
    }
    CHECK(D_80156BAC == 153 && D_80156BD0 == 153 && g_overflows == 0);
    /* Past it: said, and the last one handed out again. */
    float *e = func_800A7D6C();
    CHECK(g_overflows == 1 && g_pool && strstr(g_pool, "GetMatrix") && g_used == 154 && g_capacity == 153);
    CHECK(e == pc_matrix_spill[PC_MATRIX_SPILL - 1]);
    CHECK(D_80156BD0 == 154);
    /* The frame's reset starts again at the N64's first. */
    D_80156BAC = 0;
    CHECK(func_800A7D6C() == (void *)D_8015F740[1] && g_overflows == 1);
}

/* ---------------------------------------------------------------- the frame's buffer */

static uint8_t g_frame[PC_FRAME_BYTES] __attribute__((aligned(16)));

/* Closes a frame whose list holds `commands` and whose matrices took `matrices`; returns the handler's calls. */
static int end_frame(int commands, int matrices)
{
    D_8015F72D = 1;
    D_8015B250 = g_frame;
    D_801497C8 = g_frame + PC_GLIST_OFFSET + commands * 16;
    D_8015B260 = g_frame + PC_FRAME_MATRIX_OFFSET + matrices * 64;
    g_overflows = 0;
    g_pool = NULL;
    g_sent = NULL;
    world_collision_response();
    return g_overflows;
}

static void test_frame(void)
{
    CHECK(PC_FRAME_MATRIX_OFFSET == 1728 && PC_FRAME_MATRICES == 8 && PC_GLIST_LEN == 32);
    CHECK(PC_GLIST_OFFSET == 1728 + 8 * 64 && PC_FRAME_BYTES == PC_GLIST_OFFSET + 32 * 16);
    memset(g_frame, 0, sizeof g_frame);
    /* A frame well inside both: its list gets the text, the full sync and the end, after the 10 commands. */
    CHECK(end_frame(10, 3) == 0);
    CHECK(D_801497C8 == g_frame + PC_GLIST_OFFSET + 13 * 16 && g_sent == D_80156BE0[1]);
    uint32_t w0[3];
    for (int k = 0; k < 3; k++)
        memcpy(&w0[k], g_frame + PC_GLIST_OFFSET + (10 + k) * 16, 4);
    CHECK(w0[0] == 0xDE000000u && w0[1] == 0xE9000000u && w0[2] == 0xDF000000u);
    /* Exactly full, both: nothing said (the SDK's check is glistp > &glist[GLIST_LEN]). */
    CHECK(end_frame(PC_GLIST_LEN - 3, PC_FRAME_MATRICES) == 0);
    /* One command past the list's end. */
    CHECK(end_frame(PC_GLIST_LEN - 2, 0) == 1 && g_pool && strstr(g_pool, "display list"));
    CHECK(g_used == PC_GLIST_LEN + 1 && g_capacity == PC_GLIST_LEN);
    /* One matrix past the matrices' end. */
    CHECK(end_frame(0, PC_FRAME_MATRICES + 1) == 1 && g_pool && strstr(g_pool, "matrices"));
    CHECK(g_used == PC_FRAME_MATRICES + 1 && g_capacity == PC_FRAME_MATRICES);
    /* Both. */
    CHECK(end_frame(PC_GLIST_LEN, PC_FRAME_MATRICES + 2) == 2);
    /* The lookats: the GfxBuf's 32, full, then one past. */
    D_80124F84 = PC_FRAME_LOOKATS;
    CHECK(end_frame(0, 0) == 0);
    D_80124F84 = PC_FRAME_LOOKATS + 1;
    CHECK(end_frame(0, 0) == 1 && g_pool && strstr(g_pool, "lookats") && g_used == 33 && g_capacity == 32);
    D_80124F84 = 0;
}

int main(void)
{
    pc_pool_overflow_hook = overflow;
    test_matrices();
    test_frame();
    return TEST_DONE();
}
