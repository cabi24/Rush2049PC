/* GENERATED ROM-aligned TU — segment 0x1f50 (rom/lib_1050)
 * layout map c8f0857aebf4ea1ed90dde16bb4db378a8281234d10bd7a742394c75635cc457.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */

#include "rom_tu.h"


/* PROMOTED 2026-10-12 — viTickStart
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern volatile int gViTickCounter;
extern volatile int gViLastTick;
extern f32 gScaleSecondsPerTick;
extern f32 __osScDeltaTime;
extern f32 __osScElapsedTime;
extern u32 osSetIntMask(u32);
void viTickStart(void) {
    u32 mask;
    register f32 t;

    mask = osSetIntMask(1);
    gViLastTick = gViTickCounter;
    __osScDeltaTime = t = 2.0f * gScaleSecondsPerTick;
    __osScElapsedTime = t;
    osSetIntMask(mask);
}

/* PROMOTED 2026-10-12 — viEnableAccum
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int gScTimeEnabled;
void viEnableAccum(void) {
    gScTimeEnabled = 1;
}

/* PROMOTED 2026-10-12 — viDisableAccum
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void viDisableAccum(void) {
    gScTimeEnabled = 0;
}

/* PROMOTED 2026-10-12 — viUpdateTime
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int gViFrameDelta;
extern int gViAccumTotal;
extern int gViAccumTime;
void viUpdateTime(void) {
    gViFrameDelta = gViTickCounter - gViLastTick;
    gViLastTick = gViTickCounter;
    if (gScTimeEnabled) {
        gViAccumTotal += gViFrameDelta;
        gViAccumTime += gViFrameDelta;
    } else {
        __osScElapsedTime = (gViTickCounter - gViAccumTotal) * gScaleSecondsPerTick;
    }
    if (gViFrameDelta > 5) {
        gViFrameDelta = 5;
    }
    __osScDeltaTime = gViFrameDelta * gScaleSecondsPerTick;
}

/* PROMOTED 2026-10-12 — viScheduleTick
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern f32 gScaleTicksPerSecond;
void viScheduleTick(f32 seconds) {
    gViAccumTotal = gViTickCounter;
    gViAccumTime = (int)(seconds * gScaleTicksPerSecond) + gViTickCounter;
}

/* PROMOTED 2026-10-12 — viAddTicks
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void viAddTicks(f32 seconds) {
    gViAccumTime += (int)(seconds * gScaleTicksPerSecond);
}

/* PROMOTED 2026-10-12 — viGetTimeToDeadline
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
f32 viGetTimeToDeadline(void) {
    return (gViAccumTime - gViTickCounter) * gScaleSecondsPerTick;
}


/* PROMOTED 2026-10-01 — viDeadlinePassed
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags: -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul (segment flagset; re-locked
 *           against the 48-B target that absorbs the former 20-B "viStub" slot; was -g0 -O2)
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
s32 viDeadlinePassed(void) {
    return (gViAccumTime - gViTickCounter) < 1;
}

