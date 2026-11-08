/* GENERATED ROM-aligned TU — segment 0x1f50 (rom/lib_1050)
 * layout map c8f0857aebf4ea1ed90dde16bb4db378a8281234d10bd7a742394c75635cc457.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */

#include "rom_tu.h"


/* PROMOTED 2026-10-12 — InitializeTimer
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern volatile int scheduler_retrace_count;
extern volatile int gLastFrameCounter;
extern f32 gRetraceTime;
extern f32 gFrameSeconds;
extern f32 gSeconds;
extern u32 osSetIntMask(u32);
void InitializeTimer(void) {
    u32 mask;
    register f32 t;

    mask = osSetIntMask(1);
    gLastFrameCounter = scheduler_retrace_count;
    gFrameSeconds = t = 2.0f * gRetraceTime;
    gSeconds = t;
    osSetIntMask(mask);
}

/* PROMOTED 2026-10-12 — PauseTimer
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int gTimerPaused;
void PauseTimer(void) {
    gTimerPaused = 1;
}

/* PROMOTED 2026-10-12 — ResumeTimer
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void ResumeTimer(void) {
    gTimerPaused = 0;
}

/* PROMOTED 2026-10-12 — UpdateFrameTimer
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern int gLastFrameCount;
extern int gCountdownStart;
extern int gCountdownEnd;
void UpdateFrameTimer(void) {
    gLastFrameCount = scheduler_retrace_count - gLastFrameCounter;
    gLastFrameCounter = scheduler_retrace_count;
    if (gTimerPaused) {
        gCountdownStart += gLastFrameCount;
        gCountdownEnd += gLastFrameCount;
    } else {
        gSeconds = (scheduler_retrace_count - gCountdownStart) * gRetraceTime;
    }
    if (gLastFrameCount > 5) {
        gLastFrameCount = 5;
    }
    gFrameSeconds = gLastFrameCount * gRetraceTime; pc_game_frame_note(gLastFrameCount);
}

/* PROMOTED 2026-10-12 — SetCountdownTimer
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
extern f32 gRetraceRate;
void SetCountdownTimer(f32 seconds) {
    gCountdownStart = scheduler_retrace_count;
    gCountdownEnd = (int)(seconds * gRetraceRate) + scheduler_retrace_count;
}

/* PROMOTED 2026-10-12 — AddCountdownTime
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void AddCountdownTime(f32 seconds) {
    gCountdownEnd += (int)(seconds * gRetraceRate);
}

/* PROMOTED 2026-10-12 — GetCountdownTimeRemaining
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g1 -O1 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
f32 GetCountdownTimeRemaining(void) {
    return (gCountdownEnd - scheduler_retrace_count) * gRetraceTime;
}


/* PROMOTED 2026-10-01 — CountdownTimeOut
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags: -g1 -O1 -mips2 -G 0 -non_shared -Wab,-r4300_mul (segment flagset; re-locked
 *           against the 48-B target that absorbs the former 20-B "viStub" slot; was -g0 -O2)
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
s32 CountdownTimeOut(void) {
    return (gCountdownEnd - scheduler_retrace_count) < 1;
}

