/* PS_RumbleGo: allow rumbling (MAT3 paksysdc.c). */
#include "game/game.h"

/* Lets the rumble thread run the Rumble Paks' motors again (after PS_RumbleStop: the pause screen). */
void PS_RumbleGo(void) {
    gRumble = 1;
}
