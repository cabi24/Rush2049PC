/* GrabEnvEntry.c: take a visual from the pool of environment visuals (MAT3 visuals.c, GrabEnvEntry()). */
#include "game/game.h"


/* Unlink the first free visual from gFreeVis, count it in use (gEnvCount, and its high-water mark gEnvCountMax)
 * and return it cleared, with no scene object; NULL when the pool is used up. The caller fills it in and puts it on
 * gEnvList (AddToEnvList). */
tVisual *GrabEnvEntry(void)
{
    tVisual *v;

    if (!gFreeVis) return 0;
    v = gFreeVis;
    gFreeVis = v->next;
    gEnvCount++;
    if (gEnvCount > gEnvCountMax) gEnvCountMax = gEnvCount;
    v->next = 0;
    v->index = 0;
    v->objnum = -1;
    v->slot = 0;
    v->data = 0;
    v->func = 0;
    v->timeStamp = 0.0f;
    return v;
}
