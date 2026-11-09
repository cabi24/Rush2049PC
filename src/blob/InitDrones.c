/* InitDrones: the drones' catch-up at a race's start (MAT3 drones.c). */
#include "game/game.h"

/* Every car's last catch-up scale and boost (DoDrones' smoothing) back to 1, for the six car slots the N64 has. */
void InitDrones(void) {
    s32 i;
    for (i = 0; i < 6; i++) {
        old_drone_scale[i] = 1.0f;
        old_boost[i] = 1.0f;
    }
}
