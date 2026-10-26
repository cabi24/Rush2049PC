/* GENERATED ROM-aligned TU — segment 0xe1c0 (rom/lib_e1c0)
 * layout map 37445b1e948a966a13f7db9de016ba6979d0da712ce5d86d094da37edf500de5.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

/* PROMOTED 2026-10-01 — osVirtualToPhysical
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
u32 osVirtualToPhysical(void* addr) {
    if (IS_KSEG0(addr)) {
        return K0_TO_PHYS(addr);
    } else if (IS_KSEG1(addr)) {
        return K1_TO_PHYS(addr);
    } else {
        return __osTLBLookup(addr);
    }
}

