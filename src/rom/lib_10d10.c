/* GENERATED ROM-aligned TU — segment 0x10d10 (rom/lib_10d10)
 * layout map 4985898c120df7fc71cee8cdd7d8996c92f2e065308c067697df582ac9e5ba8a.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

/* PROMOTED 2026-10-11 — osYieldThread
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
#define OS_STATE_RUNNABLE 2
extern u32 __osDisableInt(void);
extern void __osRestoreInt(u32 flags);
extern void __osEnqueueAndYield(OSThread **queue);
extern OSThread *__osRunningThread;
extern OSThread *__osActiveQueue;
void osYieldThread(void) {
    register u32 saveMask = __osDisableInt();

    __osRunningThread->state = OS_STATE_RUNNABLE;
    __osEnqueueAndYield(&__osActiveQueue);
    __osRestoreInt(saveMask);
}

