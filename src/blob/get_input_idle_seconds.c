#include "game/game.h"

typedef struct {
    /* 0x000 */ u8 pad0[0x27C];
    /* 0x27C */ volatile u32 frameCount;
} Sched;

VIEW(Sched, gScheduler);
#define gScheduler gScheduler_view
VIEW(u32, gLastKeyTime);
#define gLastKeyTime gLastKeyTime_view

f32 get_input_idle_seconds(void) {
    return (f32)(u32)(gScheduler.frameCount - gLastKeyTime) * gRetraceTime;
}
