/* GENERATED ROM-aligned TU — segment 0xe310 (rom/lib_e310)
 * layout map f4db079f8b036cf17dadd303f7df62980b03a7c3ee665006696b2cdcc0abb113.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

/* PROMOTED 2026-07-11 — __osSpDeviceBusy
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
s32 __osSpDeviceBusy(void) {
    u32 status;

    status = SP_STATUS_REG;

    /* Check bits 2-4 (DMA_BUSY, DMA_FULL, IO_FULL) */
    if (status & 0x1C) {
        return 1;
    }
    return 0;
}

