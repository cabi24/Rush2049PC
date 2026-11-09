/* get_input_idle_seconds: how long since a player last pressed anything (the N64's idle warning). */
#include "game/game.h"

/* Returns the seconds since the last key press (gLastKeyTime, a retrace count). */
f32 get_input_idle_seconds(void) {
    return (f32)(u32)(gScheduler.retraceCount - gLastKeyTime) * gRetraceTime;
}
