/* UnloadWaveInstance (MAT3 waves.c): frees a wave instance's heap block and forgets the model, texture and palette
 * tables SetupWaveModel made from it. The wave loader is waves/; gWaveInstance's slots are anon_loaded_ready_14
 * (include/game/types.h). */
#include "game/game.h"

/* The {pointer, count} tables SetupWaveModel fills per wave instance: 16 bytes an entry on the PC. */
#define WAVE_TABLE_ENTRY 16
VIEW(u8[], gRomObjDefPtrs);
#define gRomObjDefPtrs gRomObjDefPtrs_view
VIEW(u8[], gTexDefPtrs);
#define gTexDefPtrs gTexDefPtrs_view
VIEW(u8[], gPalDefPtrs);
#define gPalDefPtrs gPalDefPtrs_view
void *memset(void *, int, u32);

/* The free takes the block's 32-bit N64 address: N64A(p) is a window pointer's (pc_prelude.h,
 * docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)

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
    DeallocateMemory(N64A(slot->ptr));
    slot->ptr = 0;
    memset(gRomObjDefPtrs + index * WAVE_TABLE_ENTRY, 0, WAVE_TABLE_ENTRY);
    memset(gTexDefPtrs + index * WAVE_TABLE_ENTRY, 0, WAVE_TABLE_ENTRY);
    memset(gPalDefPtrs + index * WAVE_TABLE_ENTRY, 0, WAVE_TABLE_ENTRY);
}
