#include "game/game.h"

VIEW(volatile f32, gColorCycle);
#define gColorCycle gColorCycle_view

void TB_TextOncePerFrame(void) {
    f32 t;
    f32 u;

    gColorCycleTime += *(f32 *) (intptr_t) &gFrameSeconds;
    t = gColorCycleTime;
    gColorCycle = t + t;
    u = gColorCycle;
    gColorCycle = u - (s32) u;
    u = gColorCycle;
    if (u > 0.5f) {
        gColorCycle = 1.0f - u;
        u = gColorCycle;
    }
    gColorCycle = u + u;
}
