/* GetNextCheckpoint: the checkpoint after another (MAT3 checkpoint.c). */
#include "game/game.h"

/* Returns the checkpoint that follows checkpoint `index` round the lap: the next one, or after the last one the
 * checkpoint a lap starts from (gCheckpoints.loopPt). */
s32 GetNextCheckpoint(s32 index) {
    if (index + 1 == gCheckpoints.numPoints) {
        return gCheckpoints.loopPt;
    }
    return index + 1;
}
