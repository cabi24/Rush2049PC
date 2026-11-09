/* zoid_reset_lights.c: go back to the scene's default lights (the N64's; ZOID_Reset does the same). */
#include "game/game.h"

/* No lights of the track's: the scene is lit by default_lights. */
void zoid_reset_lights(void) {
    gNumLights = 0;
    gLights = (struct Lights2 *) &default_lights;
}
