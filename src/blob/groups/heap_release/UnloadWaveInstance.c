/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* UnloadWaveInstance: release audio slot `index`. Waits for DMA idle (inlined static acquire_decompress_lock), frees the slot's
 * heap block through the inlined heap-free helper DeallocateMemory_stub (lock / DeallocateMemoryInternal / unlock; its real
 * definition is audio_sfx_shutdown.c's, the prefer_definition override of the locked stub), clears the allocation and
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
/* PC: gWaveInstance's host layout (waves.c): ptr and lastPtr are pointer-sized, so 0x20 bytes a slot. The
 * {pointer, count} tables are 16 bytes an entry. */
typedef struct Slot20 { u8 pad0[6]; u8 resource; u8 pad7; u32 word8; uintptr_t allocation, word16; } Slot20;
#define WAVE_PAIR 16
extern Slot20 gWaveInstance[64];
extern volatile s16 gRenderInProgress;
extern u8 gRomObjDefPtrs[], gTexDefPtrs[], gPalDefPtrs[];
void *memset(void *, int, u32);
/* DeallocateMemory_stub's manifest owner is the empty deleted-procedure stub; DeallocateMemory (heap_release/group.c)
 * is its body under a name the link keeps (docs/port/PROTOTYPES.md). */
#define DeallocateMemory_stub DeallocateMemory
void DeallocateMemory_stub(u32 address);

/* The free takes the block's 32-bit N64 address: N64A(p) is a window pointer's (pc_prelude.h,
 * docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
static void acquire_decompress_lock(void)
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
    acquire_decompress_lock();
    DeallocateMemory_stub(N64A(slot->allocation));
    if (slot) {}
    slot->allocation = 0;
    memset(gRomObjDefPtrs + index * WAVE_PAIR, 0, WAVE_PAIR);
    memset(gTexDefPtrs + index * WAVE_PAIR, 0, WAVE_PAIR);
    memset(gPalDefPtrs + index * WAVE_PAIR, 0, WAVE_PAIR);
}
