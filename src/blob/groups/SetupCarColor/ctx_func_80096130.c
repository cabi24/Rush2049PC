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
typedef struct Slot20 { u8 pad0[6]; u8 resource; u8 pad7; u32 word8, allocation, word16; } Slot20;
extern Slot20 gWaveInstance[64];
extern volatile s16 gRenderInProgress;
extern u8 gRomObjDefPtrs[], gTexDefPtrs[], gPalDefPtrs[];
void *memset(void *, int, u32);
void DeallocateMemory_stub(u32 address);

static void acquire_decompress_lock(void)
{
    while (gRenderInProgress != 0) {
    }
}

