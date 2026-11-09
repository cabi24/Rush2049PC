/* GrabEnvEntry.c: take a visual from the pool of environment visuals (MAT3 visuals.c, GrabEnvEntry()). */
#include "game/game.h"

/* A visual (tVisual) on the PC: 0x20 bytes, next and func host pointers, data a u32 (docs/port/CARS.md 3). */
typedef struct tVisual {
    struct tVisual *next;
    s16 index;
    s16 objnum;                 /* its scene object, -1 none */
    s16 slot;
    u8 padA[2];
    u32 data;
    f32 timeStamp;
    void *func;                 /* its animator */
} tVisual;

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
