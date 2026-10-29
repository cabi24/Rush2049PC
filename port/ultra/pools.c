/* port/ultra/pools.c: the game's fixed pools that more cars a race fill (docs/port/MORECARS.md 8). The N64 game checks
 * none of them: GetMatrix (func_800A7D6C) has no limit, and the SDK's display-list check is an empty `if`
 * (world_collision_response.c). On the PC each has room for PC_MAX_LINKS cars (port/include/pc_game_types.h), and a
 * pool run past its end comes here: it says which, once a pool, and a Debug build stops with a backtrace. A Release
 * build goes on (GetMatrix hands out its last matrix again; the frame's list is already written), so a player sees
 * a wrong picture and the log says why. At the end of the run the high-water marks are logged (pc_pools_report). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "pc_game_types.h"
#include "ultra.h"

/* The game's own high-water marks, never reset: GetMatrix's highest index (func_800A7D6C), the scene nodes in use
 * (func_8008E26C: D_8012E700, PC_SCENE_NODES, checked on the PC), the environment visuals (func_80090284:
 * D_80138880, 100, none left gives NULL) and the sprites (func_800A79F4: D_80140BF0, 200, none left gives -1). */
extern int32_t D_80156BD0, D_801569A8, D_8013C234;
extern int16_t D_8012E678;
/* The frame's lookats so far: the scene draw gives each environment-mapped node of each view one of the frame buffer's
 * 32 (particle_knot's GfxBuf.la[D_80124F84++], reset at the frame's start), a car's body among them. */
extern int8_t D_80124F84;

static long g_glist_max, g_matrices_max, g_lookats_max;

/* A test's own handler (port/assets/tests/test_pools.c): when set, it is called instead of the log and the stop. */
void (*pc_pool_overflow_hook)(const char *pool, int used, int capacity);

void pc_pool_overflow(const char *pool, int used, int capacity)
{
    static const char *said[8];
    int i;
    if (pc_pool_overflow_hook != NULL) {
        pc_pool_overflow_hook(pool, used, capacity);
        return;
    }
    for (i = 0; i < 8 && said[i] != NULL; i++)
        if (said[i] == pool)
            return;
    if (i < 8)
        said[i] = pool;
    fprintf(stderr, "rush2049pc: %s ran past its end: %d of %d (N64 thread %s; docs/port/MORECARS.md 8)\n", pool,
            used, capacity, pc_os_thread_name());
#ifndef NDEBUG
    pc_print_backtrace(1);
    fflush(NULL);
    abort();
#endif
}

int pc_frame_pools(const void *frame, const void *glist_end, const void *matrices_end)
{
    const uint8_t *f = (const uint8_t *)frame;
    long glist = (long)((const uint8_t *)glist_end - (f + PC_GLIST_OFFSET)) / 16;
    long matrices = (long)((const uint8_t *)matrices_end - (f + PC_FRAME_MATRIX_OFFSET)) / 64;
    int over = 0;
    if (glist > g_glist_max)
        g_glist_max = glist;
    if (matrices > g_matrices_max)
        g_matrices_max = matrices;
    if (D_80124F84 > g_lookats_max)
        g_lookats_max = D_80124F84;
    if (D_80124F84 > PC_FRAME_LOOKATS) {
        pc_pool_overflow("the frame's lookats", D_80124F84, PC_FRAME_LOOKATS);
        over = 1;
    }
    if (matrices > PC_FRAME_MATRICES) {
        pc_pool_overflow("the frame's matrices", (int)matrices, PC_FRAME_MATRICES);
        over = 1;
    }
    if (glist > PC_GLIST_LEN) {
        pc_pool_overflow("the frame's display list", (int)glist, PC_GLIST_LEN);
        over = 1;
    }
    return over;
}

void pc_pools_report(void)
{
    fprintf(stderr, "rush2049pc: pools: persistent matrices %d of %d, scene nodes %d of %d, environment visuals %d of "
                    "100, sprites %d of 200, frame lookats %ld of %d, frame matrices %ld of %d, display list %ld of %d "
                    "commands (high-water marks)\n", (int)D_80156BD0, PC_MATRICES - 1, (int)D_801569A8, PC_SCENE_NODES,
            (int)D_8012E678, (int)D_8013C234, g_lookats_max, PC_FRAME_LOOKATS, g_matrices_max, PC_FRAME_MATRICES,
            g_glist_max, PC_GLIST_LEN);
}
