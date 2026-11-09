#include "game/game.h"

void UnloadFrontEndWaves(void) {
    UnlockWaveInstance(FindWaveInstanceInternal(0x36, -1));
    UnlockWaveInstance(FindWaveInstanceInternal(0x3A, -1));
    UnlockWaveInstance(FindWaveInstanceInternal(0x3B, -1));
}
