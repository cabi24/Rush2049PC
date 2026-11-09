#include "game/game.h"

void InitDrones(void) {
    s32 i;
    for (i = 0; i < 6; i++) {
        old_drone_scale[i] = 1.0f;
        old_boost[i] = 1.0f;
    }
}
