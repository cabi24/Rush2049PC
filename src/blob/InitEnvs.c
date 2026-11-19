/* InitEnvs.c: the pool of environment visuals and its start-up (MAT3 visuals.c, gEnvirons and InitEnvs()).
 *
 * gEnvirons holds the 100 visuals the track's animated objects use (GrabEnvEntry takes one, ReleaseEnviron gives it
 * back); the free ones are linked on gFreeVis, the ones in use on gEnvList. */
#include "game/game.h"


tVisual gEnvirons[100];

/* Empty the pool: every visual free (linked in order on gFreeVis, no scene object), none in use. */
void InitEnvs(void)
{
    int i;

    gFreeVis = gEnvirons;
    for (i = 0; i < 99; i++) {
        gEnvirons[i].next = &gEnvirons[i + 1];
        gEnvirons[i].objnum = -1;
    }
    gEnvirons[i].next = 0;
    gEnvirons[i].objnum = -1;
    gEnvList = 0;
    gEnvCount = 0;
}
