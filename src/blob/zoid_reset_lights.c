/* zoid_reset_lights.c: go back to the scene's default lights (the N64's; ZOID_Reset does the same). */
#include "game/game.h"

VIEW(intptr_t, gLights);
#define gLights gLights_view

/* No lights of the track's: the scene is lit by default_lights. */
void zoid_reset_lights(void) {
    gNumLights = 0;
    gLights = (intptr_t) &default_lights;
}
