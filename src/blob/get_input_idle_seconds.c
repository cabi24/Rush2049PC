/* get_input_idle_seconds: how long since a player last pressed anything (the N64's idle warning). */
#include "game/game.h"

/* The scheduler (gScheduler): its retrace count. */
typedef struct {
    /* 0x000 */ u8 pad0[0x27C];
    /* 0x27C */ volatile u32 frameCount;
} Sched;

VIEW(Sched, gScheduler);
#define gScheduler gScheduler_view
VIEW(u32, gLastKeyTime);
#define gLastKeyTime gLastKeyTime_view

/* Returns the seconds since the last key press (gLastKeyTime, a retrace count). */
f32 get_input_idle_seconds(void) {
    return (f32)(u32)(gScheduler.frameCount - gLastKeyTime) * gRetraceTime;
}
