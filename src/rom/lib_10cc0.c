/* GENERATED ROM-aligned TU — segment 0x10cc0 (rom/lib_10cc0)
 * layout map de94582e73efc8ed1a1a72eb83814fc99e592ac67fa47da94b9ebbad4fe324c2.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

/* PROMOTED 2026-10-11 — __osSetGlobalIntMask
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef u32 OSHWIntr;
extern u32 __osDisableInt(void);
extern void __osRestoreInt(u32 flags);
extern u32 __osGlobalIntMask;
void __osSetGlobalIntMask(OSHWIntr mask) {
    register u32 saveMask = __osDisableInt();

    __osGlobalIntMask |= mask;
    __osRestoreInt(saveMask);
}

