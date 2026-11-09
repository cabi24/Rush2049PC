/* UnloadFrontEndWaves: let the front end's waves go (MAT3 front.c). */
#include "game/game.h"

/* Unlocks the three asset waves the front end keeps loaded (waves 0x36, 0x3A and 0x3B), so a race can use their
 * memory. */
void UnloadFrontEndWaves(void) {
    UnlockWaveInstance(FindWaveInstanceInternal(0x36, -1));
    UnlockWaveInstance(FindWaveInstanceInternal(0x3A, -1));
    UnlockWaveInstance(FindWaveInstanceInternal(0x3B, -1));
}
