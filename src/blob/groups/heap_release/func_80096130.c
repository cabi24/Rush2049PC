/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* func_80096130: release audio slot `index`. Waits for DMA idle (inlined static dma_wait), frees the slot's
 * heap block through the inlined heap-free helper func_800960CC (lock / audio_reverb_update / unlock; its real
 * definition is wheel_params_set.c's, the prefer_definition override of the locked stub), clears the allocation and
 * the slot's three 8-byte per-slot records. No arcade ancestor (N64 audio heap code).
 * Shaping (disclosed): one compiled-out read `if (slot) {}` before the store. It is a code-free extra use that keeps
 * the slot address web unsplit (totalsave 4 > caller-save cost 3), so it is coloured v1 ahead of the allocation
 * (a3) as in retail. The early `return` (instead of nested if) is the source lever for the tail: with the
 * return arm as block 0's fall-through, ugen closes v1's `.noalias $3,$sp` region there, so as1 keeps
 * `sw zero,12(v1)` ahead of the `lw v0,72(sp)` index reload (retail order). No unused locals, no pad, no stand-ins. */
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
#ifdef TARGET_PC
/* PC: gWaveInstance's host layout (waves.c): ptr and lastPtr are pointer-sized, so 0x20 bytes a slot. The
 * {pointer, count} tables are 16 bytes an entry. */
typedef struct Slot20 { u8 pad0[6]; u8 resource; u8 pad7; u32 word8; uintptr_t allocation, word16; } Slot20;
#define WAVE_PAIR 16
#else
typedef struct Slot20 { u8 pad0[6]; u8 resource; u8 pad7; u32 word8, allocation, word16; } Slot20;
#define WAVE_PAIR 8
#endif
extern Slot20 gWaveInstance[64];
extern volatile s16 D_8002EB70;
extern u8 gRomObjDefPtrs[], gTexDefPtrs[], gPalDefPtrs[];
void *memset(void *, int, u32);
#ifdef TARGET_PC
/* func_800960CC's manifest owner is the empty deleted-procedure stub; audio_effect_process (heap_release/group.c)
 * is its body under a name the link keeps (docs/port/PROTOTYPES.md). */
#define func_800960CC audio_effect_process
#endif
void func_800960CC(u32 address);

#ifndef TARGET_PC
#define PC_SPIN(site)   /* PC: the busy-wait lets the writer run (pc_prelude.h) */
#endif

static void dma_wait(void)
{
    while (D_8002EB70 != 0) { PC_SPIN("func_80096130")
    }
}

void func_80096130(s32 index)
{
    Slot20 *slot = &gWaveInstance[index];

    if (slot->allocation == 0) {
        return;
    }
    dma_wait();
    func_800960CC(slot->allocation);
    if (slot) {}
    slot->allocation = 0;
    memset(gRomObjDefPtrs + index * WAVE_PAIR, 0, WAVE_PAIR);
    memset(gTexDefPtrs + index * WAVE_PAIR, 0, WAVE_PAIR);
    memset(gPalDefPtrs + index * WAVE_PAIR, 0, WAVE_PAIR);
}
