/* GENERATED ROM-aligned TU — segment 0xc990 (rom/lib_c990)
 * layout map 3d7225dbf25a7b232ed775c14d1337f15392518bd99b81ab2c8ecdb8fdd031aa.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

/* PROMOTED 2026-10-01 — osStopThread (formerly labelled osPfsChecker_full)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void osStopThread(OSThread* t) {
    register u32 saveMask = __osDisableInt();
    register u16 state;

    state = (t == NULL) ? OS_STATE_RUNNING: t->state;

    switch (state) {
        case OS_STATE_RUNNING:
            __osRunningThread->state = OS_STATE_STOPPED;
            __osEnqueueAndYield(NULL);
            break;
        case OS_STATE_RUNNABLE:
        case OS_STATE_WAITING:
            t->state = OS_STATE_STOPPED;
            __osDequeueThread(t->queue, t);
            break;
    }

    __osRestoreInt(saveMask);
}

/* PROMOTED 2026-10-01 — osGetThreadId (formerly labelled osPfsReAllocate)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
OSId osGetThreadId(OSThread* thread) {
    if (thread == NULL) {
        thread = __osRunningThread;
    }

    return thread->id;
}

