#include "game/game.h"

VIEW(intptr_t, gLights);
#define gLights gLights_view

void zoid_reset_lights(void) {
    gNumLights = 0;
    gLights = (intptr_t) &default_lights;
}
