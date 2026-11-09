/* ReleaseEnviron.c: give an environment visual back to the pool (MAT3 visuals.c, ReleaseEnviron()). */
#include "game/game.h"

/* A visual (tVisual) on the PC: 0x28 bytes, next, data and func pointer-sized (struct pc_visual, docs/port/CARS.md 3). */
typedef struct tVisual {
    struct tVisual *next;
    u16 index;
    s16 objnum;                 /* its scene object, -1 none */
    s16 slot;
    uintptr_t data;
    f32 timeStamp;
    intptr_t func;              /* its animator */
} tVisual;

/* Hide visual v's scene object, clear its animator and, if `unlink`, take it off gEnvList (a caller walking the list
 * itself passes 0), then put it on the free list. gEnvCount drops by one.
 * Original game: the high-water mark gEnvCountMax is updated here too, after the count went down, where it can't
 * grow (MAT3 has the same test). */
void ReleaseEnviron(tVisual *v, int unlink) {
    tVisual *previous, *current;
    if (v->objnum >= 0) MBOX_HideObject(v->objnum, 1, 15);
    v->func = 0;
    v->objnum = -1;
    if (unlink) {
        current = gEnvList;
        if (v == current) gEnvList = current->next;
        else {
            previous = current;
            while (current && ((current = current->next), v != current)) {
                previous = current;
            }
            if (previous && previous->next == v) previous->next = v->next;
        }
    }
    v->next = gFreeVis;
    gEnvCount--;
    gFreeVis = v;
    if (gEnvCountMax < gEnvCount) gEnvCountMax = gEnvCount;
}
