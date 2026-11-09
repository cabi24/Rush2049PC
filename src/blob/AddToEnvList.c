/* AddToEnvList.c: put a visual on the list of environment visuals (MAT3 visuals.c, AddToEnvList()).
 *
 * The environment visuals are the track's animated objects (cones, fences, windows, mines, blasts ...): each is a
 * tVisual from the pool gEnvirons (GrabEnvEntry), linked on gEnvList, animated each frame by UpdateEnvirons. */
#include "game/game.h"

/* A visual (tVisual) on the PC: 0x20 bytes, next and func host pointers (docs/port/CARS.md 3). */
typedef struct tVisual {
    struct tVisual *next;
} tVisual;

VIEW(tVisual *, gEnvList);
#define gEnvList gEnvList_view

/* Push visual v on the front of gEnvList. No C calls it; it is in gen_data's function table, so data could
 * (docs/port/LAYOUT.md 6). */
void AddToEnvList(tVisual *v) {
    v->next = gEnvList;
    gEnvList = v;
}
