#include "game/game.h"

VIEW(u16, gNumLaps);
#define gNumLaps gNumLaps_view

void InitCPS(void) {
    gEndGame = 0;
    gNumLapsLeft = (s8) (s16) gNumLaps;
}
