/* InitCPS: the checkpoint state at the start of a race (MAT3 checkpoint.c). */
#include "game/game.h"

/* gNumLaps as MAT3 has it, unsigned. */
VIEW(u16, gNumLaps);
#define gNumLaps gNumLaps_view

/* The race has not ended, and every lap is still to drive. */
void InitCPS(void) {
    gEndGame = 0;
    gNumLapsLeft = (s8) (s16) gNumLaps;
}
