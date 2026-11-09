#include "game/game.h"

/* PC: a StatLL list header's host layout, as every view has it (docs/port/POINTERS.md 9): doubly flag, element
 * count and size, then the element array, the active list and the free list pointer-sized (40 bytes). */
typedef struct Pool {
    u8 flag;
    int count;
    int size;
    void *mem;
    void *head, *store;
} Pool;

VIEW(Pool, gSmokeList);
#define gSmokeList gSmokeList_view
VIEW(PcPuff[100], gSmokeObjs);
#define gSmokeObjs gSmokeObjs_view
#define ELEMENT_SIZE(n64, type) sizeof(type)
/* On the PC the tire-effect latches are pc_tire_fx, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md 6),
 * and the reset clears all PC_MAX_LINKS of them: on the N64 the 6 it has. */
#define gCarSmokeDir pc_tire_fx

VIEW(char *[6], FireTexNames);
#define FireTexNames FireTexNames_view
VIEW(s16[6], gFireTexs);
#define gFireTexs gFireTexs_view
VIEW(u8, gNumModels);
#define gNumModels gNumModels_view

void MBOX_FindTexture_WORK(char *a, s16 *b, int c, int d, int e);

/* one-line multi-statement macro: stands in for the inlined kept InitStatLL (one .loc) */
#define POOL_SETUP(pool, m, n, sz) { (pool).mem = (m); (pool).count = (n); (pool).size = (sz); (pool).flag = 0; }

void InitSmoke(void)
{
    int i;

    POOL_SETUP(gSmokeList, gSmokeObjs, 100, ELEMENT_SIZE(88, PcPuff));
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
