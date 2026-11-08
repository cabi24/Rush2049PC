/* port/assets/tests/test_pools.c: the pools more cars a race fill (docs/port/MORECARS.md 8), through the game's own
 * code, with the pools shrunk (CMakeLists.txt: PC_MATRIX_SPILL 4, PC_FRAME_MATRICES 8, PC_GLIST_LEN 32) so that the
 * checks themselves are what is tested. GetMatrix (ZOID_NewMatrix) hands out the N64's 150 persistent matrices at
 * gMatrices (index 0 unused), then pc_matrix_spill's, then says it ran out (pc_pool_overflow) and hands out its last
 * one again; EndGraphics closes the frame's list in the frame's buffer as pc_game_types.h
 * lays it out and hands the list's and the matrices' ends to pc_frame_pools (port/ultra/pools.c), which says when
 * either ran past its end, or the frame's lookats did, and not when one is exactly full. The scene nodes' allocator
 * (ZOID_AddObject) hands out free nodes, then grows the table to its 600, then says it ran out and hands out the last
 * one again, where the N64 writes past the table. "Says" is this test's handler (test_pool_overflow, which
 * support.c's pc_pool_overflow calls).
 * game: src/blob/ZOID_NewMatrix.c src/blob/EndGraphics.c src/blob/ZOID_AddObject.c
 */
#include <stddef.h>
#include <string.h>

#include "pc_prelude.h"
#include "support.h"

/* ---------------------------------------------------------------- what the code under test uses */

/* GetMatrix's */
int32_t gNumMatrices, gMaxMatrices;
float gMatrices[PC_N64_MATRICES][12] __attribute__((aligned(16)));
float pc_matrix_spill[PC_MATRIX_SPILL][12] __attribute__((aligned(16)));   /* data.c's, in the window, in the game */
float unitmatrix[3][3];
static int g_copies;
static const void *g_copied_to;
void CopyUV(void *src, void *dst)
{
    if (src == unitmatrix)
        g_copies++;
    g_copied_to = dst;
}
void *ZOID_NewMatrix(void);

/* EndGraphics's */
uint8_t frame_records[2][0xD0] __attribute__((aligned(16)));   /* the frame records (its PC Frame, 0xD0 bytes) */
volatile int8_t frame_record_index;
void *frame_dl, *last_frame_dl_end, *frame_dynamic, *frame_matrix_cursor;
int32_t last_frame_text_dl_end, gfx_tasks_pending;
intptr_t blit_dl;
uint64_t rspbootTextStart[2], rspbootTextEnd[2], gspF3DEX2_NoN_fifoTextStart[2], gspF3DEX2_NoN_fifoDataStart[2], rsp_dram_stack[2], rsp_yield_buffer[2], rdp_fifo_buffer[2],
    rdp_fifo_buffer_end[2];
uint32_t main_thread_queue[6], scheduler_task_queue[6], scheduler_interrupt_queue[6];   /* the scheduler's queues: done, tasks, commands */
uint8_t scheduler_thread[16];
int16_t gRenderInProgress;
volatile int32_t gLastFrameCount;
uint8_t gScheduler[640] __attribute__((aligned(16)));
static uint64_t g_text_dl[2];
static const void *g_sent;
void HAL_FinishText(int32_t a) { (void)a; }
void *BlitFinish(void) { return g_text_dl; }
void osStopThread(void *t) { (void)t; }
void osStartThread(void *t) { (void)t; }
int32_t osSendMesg(void *q, void *m, int32_t f) { (void)q; (void)f; g_sent = m; return 0; }
void EndGraphics(void);

/* The scene nodes' (ZOID_AddObject: the host record, 0x68 bytes, its id at +0x18) */
uint8_t ObjInstanceList[PC_SCENE_NODES][0x68] __attribute__((aligned(16)));
int32_t gNumObjects;
void ZOID_InsertObject(int16_t index, int16_t parent) { (void)index; (void)parent; }
int32_t ZOID_AddObject(int32_t a, void *b, int16_t parent, int32_t d);

/* port/ultra/pools.c's */
int32_t gMaxObjects, gMaxBlitInsts;
int16_t gEnvCountMax;
int8_t frame_lookat_count;   /* the frame's lookats so far */

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
    memset(gMatrices, 0x7F, sizeof gMatrices);
    memset(pc_matrix_spill, 0x7F, sizeof pc_matrix_spill);
    gNumMatrices = 0;
    gMaxMatrices = 0;
    g_overflows = g_copies = 0;
    /* The N64's: indexes 1-149 at their N64 places, each cleared of its translation and set to the identity. */
    for (int i = 1; i < PC_N64_MATRICES; i++) {
        float *e = ZOID_NewMatrix();
        CHECK(e == gMatrices[i] && g_copied_to == e);
        CHECK(e[9] == 0.0f && e[10] == 0.0f && e[11] == 0.0f && e[8] != 0.0f);
    }
    CHECK(gNumMatrices == 149 && gMaxMatrices == 149 && g_copies == 149 && g_overflows == 0);
    CHECK(gMatrices[0][0] != 0.0f);   /* index 0 is never handed out */
    /* Then the spill, in order. */
    for (int i = 0; i < PC_MATRIX_SPILL; i++) {
        float *e = ZOID_NewMatrix();
        CHECK(e == pc_matrix_spill[i] && e[9] == 0.0f && e[11] == 0.0f);
    }
    CHECK(gNumMatrices == 153 && gMaxMatrices == 153 && g_overflows == 0);
    /* Past it: said, and the last one handed out again. */
    float *e = ZOID_NewMatrix();
    CHECK(g_overflows == 1 && g_pool && strstr(g_pool, "GetMatrix") && g_used == 154 && g_capacity == 153);
    CHECK(e == pc_matrix_spill[PC_MATRIX_SPILL - 1]);
    CHECK(gMaxMatrices == 154);
    /* The frame's reset starts again at the N64's first. */
    gNumMatrices = 0;
    CHECK(ZOID_NewMatrix() == (void *)gMatrices[1] && g_overflows == 1);
}

/* ---------------------------------------------------------------- the frame's buffer */

/* The frame's buffer, with room after it for the commands a frame past its end writes there (the test's own: the
 * game's list ends where its buffer does). */
static uint8_t g_frame[PC_FRAME_BYTES + 8 * 16] __attribute__((aligned(16)));

/* Closes a frame whose list holds `commands` and whose matrices took `matrices`; returns the handler's calls. */
static int end_frame(int commands, int matrices)
{
    frame_record_index = 1;
    frame_dynamic = g_frame;
    frame_dl = g_frame + PC_GLIST_OFFSET + commands * 16;
    frame_matrix_cursor = g_frame + PC_FRAME_MATRIX_OFFSET + matrices * 64;
    g_overflows = 0;
    g_pool = NULL;
    g_sent = NULL;
    EndGraphics();
    return g_overflows;
}

static void test_frame(void)
{
    CHECK(PC_FRAME_MATRIX_OFFSET == 1728 && PC_FRAME_MATRICES == 8 && PC_GLIST_LEN == 32);
    CHECK(PC_GLIST_OFFSET == 1728 + 8 * 64 && PC_FRAME_BYTES == PC_GLIST_OFFSET + 32 * 16);
    memset(g_frame, 0, sizeof g_frame);
    /* A frame well inside both: its list gets the text, the full sync and the end, after the 10 commands. */
    CHECK(end_frame(10, 3) == 0);
    CHECK(frame_dl == g_frame + PC_GLIST_OFFSET + 13 * 16 && g_sent == frame_records[1]);
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
    frame_lookat_count = PC_FRAME_LOOKATS;
    CHECK(end_frame(0, 0) == 0);
    frame_lookat_count = PC_FRAME_LOOKATS + 1;
    CHECK(end_frame(0, 0) == 1 && g_pool && strstr(g_pool, "lookats") && g_used == 33 && g_capacity == 32);
    frame_lookat_count = 0;
}

/* ---------------------------------------------------------------- the scene nodes */

static uint16_t node_id(int i)
{
    uint16_t v;
    memcpy(&v, ObjInstanceList[i] + 0x18, 2);
    return v;
}

static void test_nodes(void)
{
    static int owner;
    CHECK(PC_SCENE_NODES == 600);
    memset(ObjInstanceList, 0, sizeof ObjInstanceList);
    gNumObjects = gMaxObjects = 0;
    g_overflows = 0;
    for (int i = 0; i < PC_SCENE_NODES; i++)
        CHECK(ZOID_AddObject(5, &owner, -1, 0) == i && node_id(i) == 5);
    CHECK(gNumObjects == PC_SCENE_NODES && gMaxObjects == PC_SCENE_NODES && g_overflows == 0);
    /* None free: said, the last one handed out again, the table not grown past its end. */
    CHECK(ZOID_AddObject(6, &owner, -1, 0) == PC_SCENE_NODES - 1 && node_id(PC_SCENE_NODES - 1) == 6);
    CHECK(g_overflows == 1 && g_pool && strstr(g_pool, "scene nodes") && g_used == 601 && g_capacity == 600);
    CHECK(gNumObjects == PC_SCENE_NODES);
    /* A node freed (its id 0xFFFF) is the next one handed out. */
    memset(ObjInstanceList[7] + 0x18, 0xFF, 2);
    CHECK(ZOID_AddObject(8, &owner, -1, 0) == 7 && node_id(7) == 8 && g_overflows == 1);
}

int main(void)
{
    test_pool_overflow = overflow;
    test_matrices();
    test_frame();
    test_nodes();
    return TEST_DONE();
}
