/* TB_TextOncePerFrame: advance the flashing text colour (MAT3 TB_text.c). */
#include "game/game.h"

/* Called once a frame (HAL_InstallOncePerFrameFunc, from the first flashing text): advances the flash clock by the
 * frame's time and makes gColorCycle a triangle wave from 0 up to 1 and back, twice a second. */
void TB_TextOncePerFrame(void) {
    f32 cycle;
    f32 phase;

    gColorCycleTime += gFrameSeconds;
    cycle = gColorCycleTime;
    gColorCycle = cycle + cycle;
    phase = gColorCycle;
    gColorCycle = phase - (s32) phase;
    phase = gColorCycle;
    if (phase > 0.5f) {
        gColorCycle = 1.0f - phase;
        phase = gColorCycle;
    }
    gColorCycle = phase + phase;
}
