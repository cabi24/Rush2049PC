#include "game/game.h"

/* The heap's API takes N64 addresses: N64A(p) is a window pointer's (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
void *musyx_alloc(s32 arg0, M2C_UNK arg1) {
    void *temp_v0;

    temp_v0 = AllocateMemoryHeap(0, arg0);
    PurgeableMemory(N64A(temp_v0), 0);
    return temp_v0;
}

