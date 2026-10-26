/* GENERATED ROM-aligned TU — segment 0xe240 (rom/lib_e240)
 * layout map 574b1d74524f1b7c5783607ded27a53e2b3271af6246554b4bd8061c261a7c23.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

/* PROMOTED 2026-07-11 — __osSpSetStatus
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void __osSpSetStatus(u32 status) {
    SP_STATUS_REG = status;
}

