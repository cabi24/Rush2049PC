/* GENERATED ROM-aligned TU — segment 0xd260 (rom/lib_d260)
 * layout map f8e7304fe78f87eaf0b01132b0c5140f473ace785bdf961a8141e94d2f0ccccb.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

/* PROMOTED 2026-10-01 — osGetActiveQueue
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
s32 osGetActiveQueue(void) {
    return __osViModeInfo;
}

