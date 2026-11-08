/* port/ultra/pools.c: the game's fixed pools that more cars a race fill (docs/port/MORECARS.md 8). The N64 game checks
 * none of them: GetMatrix (ZOID_NewMatrix) and the scene nodes (ZOID_AddObject) have no limit, and the SDK's
 * display-list check is an empty `if` (EndGraphics.c). On the PC each has room for PC_MAX_LINKS cars
 * (port/include/pc_game_types.h), and the frame's three are checked here at the end of each frame's list; a pool run
 * past its end goes to pc_pool_overflow (port/ultra/pool_overflow.c). At the end of the run the high-water marks are
 * logged (pc_pools_report). */
#include <stdint.h>
#include <stdio.h>

#include "pc_game_types.h"
#include "ultra.h"

/* The game's own high-water marks, never reset: GetMatrix's highest index (ZOID_NewMatrix), the scene nodes in use
 * (ZOID_AddObject: ObjInstanceList, PC_SCENE_NODES, checked on the PC), the environment visuals (GrabEnvEntry:
 * gEnvirons, 100, none left gives NULL) and the sprites (ZOID_AddBlit: gBlitInstanceList, 200, none left gives -1). */
extern int32_t gMaxMatrices, gMaxObjects, gMaxBlitInsts;
extern int16_t gEnvCountMax;
/* The frame's lookats so far: the scene draw gives each environment-mapped node of each view one of the frame buffer's
 * 32 (particle_knot's GfxBuf.la[frame_lookat_count++], reset at the frame's start), a car's body among them. */
extern int8_t frame_lookat_count;

static long g_glist_max, g_matrices_max, g_lookats_max;

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
    if (frame_lookat_count > g_lookats_max)
        g_lookats_max = frame_lookat_count;
    if (frame_lookat_count > PC_FRAME_LOOKATS) {
        pc_pool_overflow("the frame's lookats", frame_lookat_count, PC_FRAME_LOOKATS);
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
                    "commands (high-water marks)\n", (int)gMaxMatrices, PC_MATRICES - 1, (int)gMaxObjects, PC_SCENE_NODES,
            (int)gEnvCountMax, (int)gMaxBlitInsts, g_lookats_max, PC_FRAME_LOOKATS, g_matrices_max, PC_FRAME_MATRICES,
            g_glist_max, PC_GLIST_LEN);
}
