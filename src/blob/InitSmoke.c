/* InitSmoke.c: set the smoke up for a race (MAT3 smoke.c, InitSmoke()).
 *
 * Empties the pool of 100 smoke puffs (gSmokeObjs, listed by gSmokeList), clears every car's smoke latches and
 * finds the fire textures the blasts' flames use. */
#include "game/game.h"

/* On the PC the tire-effect latches are pc_tire_fx, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md 6),
 * and the reset clears all PC_MAX_LINKS of them: on the N64 the 6 it has. */
#define gCarSmokeDir pc_tire_fx

/* Make gSmokeList a singly linked list of the 100 puffs of gSmokeObjs (88 bytes each on the N64, PcPuff's size
 * here), all free; clear every car's smoke latches (SMOKE_* in cars.h); and look the six fire textures up by name
 * (FireTexNames, into gFireTexs) in every loaded model, warning if one is missing, for StartSmoke's flames. With
 * 4 MB on the first six tracks there is no fire (StartNewBlast starts none), so the N64 skips the lookup. */
void InitSmoke(void)
{
    int i;

    gSmokeList.list = gSmokeObjs;   /* MAT3 InitStatLL, inlined: the elements, how many, their size, singly linked */
    gSmokeList.numElements = 100;
    gSmokeList.elementSize = sizeof(PcPuff);
    gSmokeList.dbl = 0;
    ClearStatLL(&gSmokeList);
    for (i = 0; i < PC_MAX_LINKS; i++) {
        gCarSmokeDir[i] = 0;
    }
    if (expansion_pak_present || gTrackNum >= 6) {
        for (i = 0; i < 6; i++) {
            MBOX_FindTexture_WORK(FireTexNames[i], &gFireTexs[i], 0, (s8)(gNumModels[0] - 1), 1);
        }
    }
}
