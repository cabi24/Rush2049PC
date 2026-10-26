/* GENERATED ROM-aligned TU — segment 0x8330 (rom/lib_8330)
 * layout map 90c062168a4e29eeb44d9521ab322e206373d920a766a0ba15c0fbd3ceaf252d.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

/* PROMOTED 2026-10-01 — osSpTaskYielded
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
OSYieldResult osSpTaskYielded(OSTask* tp) {
    u32 status;
    OSYieldResult result;

    status = bzero_alt();
    result = (status & SP_STATUS_YIELDED) ? OS_TASK_YIELDED : 0;

    if (status & SP_STATUS_YIELD) {
        tp->t.flags |= result;
        tp->t.flags &= ~(OS_TASK_DP_WAIT);
    }

    return result;
}

/* PROMOTED 2026-10-01 — osViGetCurrentFramebuffer
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void* osViGetCurrentFramebuffer(void) {
    register u32 saveMask;
    void* framep;

#ifdef _DEBUG
    if (!__osViDevMgr.active) {
        __osError(ERR_OSVIGETCURRENTFRAMEBUFFER, 0);
        return NULL;
    }
#endif

    saveMask = __osDisableInt();
    framep = ((__OSViContext *)__osViModeInfo)->framep;
    __osRestoreInt(saveMask);
    return framep;
}

