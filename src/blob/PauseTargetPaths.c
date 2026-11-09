#include "game/game.h"

void PauseTargetPaths(void *arg0) {
    M2C_FIELD(arg0, s8 *, 0x64) = 2;
    M2C_FIELD(arg0, s32 *, 0x60) = -1;
}
