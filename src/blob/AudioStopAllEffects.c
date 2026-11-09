#include "game/game.h"

typedef struct Entry {
    s8 active;      /* 0 */
    u8 pad1[3];
    u8 flag;        /* 4 */
    u8 pad5[19];
} Entry;            /* 24 */

VIEW(Entry *, gSfxInstances);
#define gSfxInstances gSfxInstances_view

VIEW(volatile u8, gStopAllEffects);
#define gStopAllEffects gStopAllEffects_view

void AudioStopAllEffects(void)
{
    s32 i;

    for (i = 0; i < gNumInstances; i++) {
        if (gSfxInstances[i].active) {
            gSfxInstances[i].flag = 1;
        }
    }
    gStopAllEffects = 1;
}
