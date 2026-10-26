/* GENERATED ROM-aligned TU — segment 0x79a0 (rom/lib_79a0)
 * layout map 56ce1eb055ef7258ce5f136180fd89777ccf4df1dece2ce41df7852ba3d3ef68.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

/* PROMOTED 2026-10-01 — osViBlack (formerly labelled osSetIntMask)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void osViBlack(s32 mask) {
    u32 temp_a0;

    temp_a0 = __osDisableInt();
    if ((u8) mask != 0) {
        __osViContext->state |= 0x20;
    } else {
        __osViContext->state &= 0xFFDF;
    }
    __osRestoreInt(temp_a0);
}

