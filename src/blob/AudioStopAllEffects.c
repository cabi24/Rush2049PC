/* AudioStopAllEffects: the sound server's stop of every effect (MAT3 audiosfx.c). */
#include "game/game.h"
#include "game/audiomgr.h"

/* Marks every effect instance in use to be stopped; AudioSfxServer then stops them over the next retraces (a
 * quarter of them at most per retrace on the N64). */
void AudioStopAllEffects(void)
{
    s32 i;

    for (i = 0; i < gNumInstances; i++) {
        if (gSfxInstances[i].used) {
            gSfxInstances[i].stopAll = 1;
        }
    }
    gStopAllEffects = 1;
}
