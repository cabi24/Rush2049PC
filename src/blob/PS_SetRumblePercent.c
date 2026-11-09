/* PS_SetRumblePercent: how hard a Rumble Pak should rumble (MAT3 paksysdc.c). */
#include "game/game.h"

/* Asks pak's Rumble Pak for rumble value (0..40 used: 10 and up runs the motor all the time, below that a duty cycle
 * of rumble_duty_masks); the rumble thread brings it about. Nothing calls it: update_model writes the request itself. */
void PS_SetRumblePercent(s32 pak,u32 value) {gPaks[pak].request=value;}
