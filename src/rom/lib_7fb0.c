/* GENERATED ROM-aligned TU — segment 0x7fb0 (rom/lib_7fb0)
 * layout map 1a786bb831297a8815b902edffbc3acc97075ef7084ae9a45901e970b0cfa809.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

extern OSTime __osCurrentTime;
extern u32 __osBaseCounter;

/* PROMOTED 2026-10-01 — osGetTime
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
OSTime osGetTime(void) {
    u32 tmptime;
    u32 elapseCount;
    OSTime currentCount;
    register u32 saveMask;


    saveMask = __osDisableInt();
    tmptime = osGetCount();
    elapseCount = tmptime - __osBaseCounter;
    currentCount = __osCurrentTime;
    __osRestoreInt(saveMask);
    return currentCount + elapseCount;
}

