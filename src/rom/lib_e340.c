/* GENERATED ROM-aligned TU — segment 0xe340 (rom/lib_e340)
 * layout map db477fc5620275132aef99c28206313a259a59a9528270c5f3b559f760fe2747.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

/* PROMOTED 2026-07-15 — osDpIsBusy
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
s32 osDpIsBusy(void)
{
    u32 status;

    status = DPC_STATUS_REG;

    if (status & 0x100)
    {
        return 1;
    }
    return 0;
}

