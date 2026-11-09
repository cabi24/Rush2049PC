#include "game/game.h"

/* PC: gWaveInstance's host layout (waves.c): ptr and lastPtr are pointer-sized, so 0x20 bytes a slot. The
 * {pointer, count} tables are 16 bytes an entry. */
typedef struct Slot20 { u8 pad0[6]; u8 resource; u8 pad7; u32 word8; uintptr_t allocation, word16; } Slot20;
#define WAVE_PAIR 16
VIEW(Slot20[64], gWaveInstance);
#define gWaveInstance gWaveInstance_view
VIEW(volatile s16, gRenderInProgress);
#define gRenderInProgress gRenderInProgress_view
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
static void wait_for_render(void)
{
    while (gRenderInProgress != 0) { pc_os_spin_yield("UnloadWaveInstance");
    }
}

void UnloadWaveInstance(s32 index)
{
    Slot20 *slot = &gWaveInstance[index];

    if (slot->allocation == 0) {
        return;
    }
    wait_for_render();
    DeallocateMemory(N64A(slot->allocation));
    if (slot) {}
    slot->allocation = 0;
    memset(gRomObjDefPtrs + index * WAVE_PAIR, 0, WAVE_PAIR);
    memset(gTexDefPtrs + index * WAVE_PAIR, 0, WAVE_PAIR);
    memset(gPalDefPtrs + index * WAVE_PAIR, 0, WAVE_PAIR);
}
