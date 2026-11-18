/* InitSkids.c: empty the skid-mark pool for a new race (MAT3 skids.c, InitSkids()).
 *
 * Removes every mark left from the last race, sets the pool of 100 marks (gSkidArray, listed by gSkidList) up again
 * and clears each tire's skid state (gNewSkid). */
#include "game/game.h"

/* The tires' skid table gNewSkid (tNewSkid[cars][4], 96 bytes each, its mark pointer pointer-sized): all of it, or
 * the last wheel's slot keeps a stale mark pointer after a reset. On the PC it is pc_skids, PC_MAX_LINKS cars, with no
 * N64 address (pc_game_types.h, docs/port/DECOUPLE.md 6). */
#define SKID_TABLE_BYTES sizeof (pc_skids)

#define gNewSkid PC_SKIDS

void *memset(void *, int, unsigned int);


/* Remove every skid mark's polygon and free its element, make gSkidList a doubly linked list of the 100 marks of
 * gSkidArray again, all free, and clear every tire's skid state. (MAT3 also builds each mark's GameCube strip;
 * the N64 draws the polygon alone.) */
void InitSkids(void)
{
    tSkid *s;

    while (gSkidList.head != 0) {
        s = gSkidList.head;
        ZOID_RemovePoly(s->poly);
        FreeStatLLE(&gSkidList, s);
    }
    gSkidList.list = gSkidArray;    /* MAT3 InitStatLL, inlined: the elements, how many, their size, doubly linked */
    gSkidList.numElements = 100;
    gSkidList.elementSize = sizeof(tSkid);
    gSkidList.dbl = 1;
    ClearStatLL(&gSkidList);
    memset(gNewSkid, 0, SKID_TABLE_BYTES);
}
