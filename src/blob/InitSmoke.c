/* InitSmoke.c: set the smoke up for a race (MAT3 smoke.c, InitSmoke()).
 *
 * Empties the pool of 100 smoke puffs (gSmokeObjs, listed by gSmokeList), clears every car's smoke latches and
 * finds the fire textures the blasts' flames use. */
#include "game/game.h"

/* PC: a StatLL list header's host layout, as every view has it (docs/port/POINTERS.md 9): doubly flag, element
 * count and size, then the element array, the active list and the free list pointer-sized (40 bytes). */
typedef struct SmokeList {
    u8 dbl;
    int numElements;
    int elementSize;
    void *list;
    void *head, *store;
} SmokeList;

VIEW(SmokeList, gSmokeList);
#define gSmokeList gSmokeList_view
VIEW(PcPuff[100], gSmokeObjs);
#define gSmokeObjs gSmokeObjs_view
/* On the PC the tire-effect latches are pc_tire_fx, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md 6),
 * and the reset clears all PC_MAX_LINKS of them: on the N64 the 6 it has. */
#define gCarSmokeDir pc_tire_fx

VIEW(char *[6], FireTexNames);
#define FireTexNames FireTexNames_view
VIEW(s16[6], gFireTexs);
#define gFireTexs gFireTexs_view
VIEW(u8, gNumModels);
#define gNumModels gNumModels_view

/* one-line multi-statement macro: stands in for the inlined kept InitStatLL (one .loc) */
#define POOL_SETUP(pool, m, n, sz) { (pool).list = (m); (pool).numElements = (n); (pool).elementSize = (sz); (pool).dbl = 0; }

/* Make gSmokeList a singly linked list of the 100 puffs of gSmokeObjs (88 bytes each on the N64, PcPuff's size
 * here), all free; clear every car's smoke latches (SMOKE_* in cars.h); and look the six fire textures up by name
 * (FireTexNames, into gFireTexs) in every loaded model, warning if one is missing, for StartSmoke's flames. With
 * 4 MB on the first six tracks there is no fire (StartNewBlast starts none), so the N64 skips the lookup. */
void InitSmoke(void)
{
    int i;

    POOL_SETUP(gSmokeList, gSmokeObjs, 100, sizeof(PcPuff));
    ClearStatLL(&gSmokeList);
    for (i = 0; i < PC_MAX_LINKS; i++) {
        gCarSmokeDir[i] = 0;
    }
    if (expansion_pak_present || gTrackNum >= 6) {
        for (i = 0; i < 6; i++) {
            MBOX_FindTexture_WORK(FireTexNames[i], &gFireTexs[i], 0, (s8)(gNumModels - 1), 1);
        }
    }
}
