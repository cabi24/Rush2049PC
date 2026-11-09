/* InitSkids.c: empty the skid-mark pool for a new race (MAT3 skids.c, InitSkids()).
 *
 * Removes every mark left from the last race, sets the pool of 100 marks (gSkidArray, listed by gSkidList) up again
 * and clears each tire's skid state (gNewSkid). */
#include "game/game.h"

/* PC: the skid marks (skid_marks' EffectObj, MAT3 tSkid) of this doubly linked list: the two links are
 * pointer-sized, so the element is 56 bytes here (36 on the N64), and so are the array and the element size
 * (docs/port/POINTERS.md 9). */
typedef struct tSkid {
    void *next, *prev;
    f32 lastTime;
    void *poly;     /* the mark's polygon */
} tSkid;

/* The skid table gNewSkid (skid_marks' WheelSlot[6][4]: 0x5C bytes on the N64, 0x60 here, its effect pointer being
 * pointer-sized): all of it, or the last wheel's slot keeps a stale effect pointer after a reset. On the PC it is
 * pc_skids, PC_MAX_LINKS cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md 6). */
#define SKID_TABLE_BYTES sizeof (pc_skids)

/* The list header (tStatList), with the active list typed. */
typedef struct SkidList {
    u8 dbl;
    int numElements;
    int elementSize;
    void *list;
    tSkid *head;     /* the marks in use */
    tSkid *store;    /* the whole StatLL header (shots.c), as every view has it on the PC */
} SkidList;

VIEW(SkidList, gSkidList);
#define gSkidList gSkidList_view
VIEW(PcEffectObj[100], gSkidArray);
#define gSkidArray gSkidArray_view
#define gNewSkid PC_SKIDS

void *memset(void *, int, unsigned int);

/* one-line multi-statement macro: stands in for the inlined kept InitStatLL (one .loc) */
#define POOL_SETUP(pool, m, sz, n, f) { (pool).list = (m); (pool).numElements = (n); (pool).elementSize = (sz); (pool).dbl = (f); }

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
    POOL_SETUP(gSkidList, gSkidArray, sizeof(PcEffectObj), 100, 1);
    ClearStatLL(&gSkidList);
    memset(gNewSkid, 0, SKID_TABLE_BYTES);
}
