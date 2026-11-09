/* InitCPS: the checkpoint state at the start of a race (MAT3 checkpoint.c). */
#include "game/game.h"

/* The race has not ended, and every lap is still to drive. */
void InitCPS(void) {
    gEndGame = 0;
    gNumLapsLeft = (s8) (s16) gNumLaps;
}
