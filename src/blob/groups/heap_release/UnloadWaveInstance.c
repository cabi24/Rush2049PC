/* UnloadWaveInstance (MAT3 waves.c): frees a wave instance's heap block and forgets the model, texture and palette
 * tables SetupWaveModel made from it. The wave loader is waves/; gWaveInstance's slots are anon_loaded_ready_14
 * (include/game/types.h). */
#include "game/game.h"

void *memset(void *, int, u32);

/* Waits until the RSP has finished the frame being drawn (gRenderInProgress), which may still read the wave. */
static void wait_for_render(void)
{
    while (gRenderInProgress != 0) { pc_os_spin_yield("UnloadWaveInstance");
    }
}

/* Frees wave instance index, if it holds a block: waits for the frame being drawn, frees the block, and clears the
 * instance's entries in the model, texture and palette tables. */
void UnloadWaveInstance(s32 index)
{
    anon_loaded_ready_14 *slot = &gWaveInstance[index];

    if (slot->ptr == 0) {
        return;
    }
    wait_for_render();
    DeallocateMemory(slot->ptr);
    slot->ptr = 0;
    memset(&gRomObjDefPtrs[index], 0, sizeof(gRomObjDefPtrs[index]));
    memset(&gTexDefPtrs[index], 0, sizeof(gTexDefPtrs[index]));
    memset(&gPalDefPtrs[index], 0, sizeof(gPalDefPtrs[index]));
}
