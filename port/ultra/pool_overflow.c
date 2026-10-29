/* port/ultra/pool_overflow.c: a game pool run past its end (pc_game_types.h, docs/port/MORECARS.md 8): GetMatrix's
 * matrices, the scene nodes, and the frame's lookats, matrices and display list (port/ultra/pools.c). It says which,
 * once a pool, and a Debug build stops with a backtrace. A Release build goes on (GetMatrix and the scene nodes hand
 * out their last one again; the frame's list is already written), so a player sees a wrong picture and the log says
 * why. On its own so the asset tests can link port/ultra/pools.c with a handler of theirs (port/assets/tests/support.c). */
#include <stdio.h>
#include <stdlib.h>

#include "pc_game_types.h"
#include "ultra.h"

void pc_pool_overflow(const char *pool, int used, int capacity)
{
    static const char *said[8];
    int i;
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
