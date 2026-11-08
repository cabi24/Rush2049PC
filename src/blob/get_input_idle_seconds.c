/* flags: -g0 -O3 -mips2 -G 0 -non_shared  (also MATCH at -O2) */
/*
 * Elapsed time in seconds since the mark gLastKeyTime: (scheduler frame count
 * at gScheduler+0x27C minus the mark), as unsigned, times the seconds-per-
 * frame constant gRetraceTime.  N64-only.
 *
 * What mattered: the scheduler's frame counter is `volatile` (it is advanced
 * by the scheduler thread).  A volatile field is not folded into the symbol
 * (`lui/addiu base; lw 636(base)` instead of `lw %lo(sym+636)`).  The same
 * unfolded form appears in SetPakError, UpdateControls, CreatePlayer,
 * ShowPakError, EndGraphics, process_scheduler_message,
 * emitter_update, GameLoop and animDot.
 */
typedef unsigned char u8;
typedef int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct {
    /* 0x000 */ u8 pad0[0x27C];
    /* 0x27C */ volatile u32 frameCount;
} Sched;

extern Sched gScheduler;
extern u32 gLastKeyTime;
extern f32 gRetraceTime;

f32 get_input_idle_seconds(void) {
    return (f32)(u32)(gScheduler.frameCount - gLastKeyTime) * gRetraceTime;
}
