/* port/assets/tests/test_pool.c: the game's own StatLL list code on the PC layouts (docs/port/POINTERS.md 9): the
 * header (40 bytes: doubly flag, count, size, element array, active list, free list), InitStatLL
 * and its free list (ClearStatLL), GetStatLLE and FreeStatLLE on a singly and a doubly linked list, and
 * the initializers that give overlay B's trail and fade lists (InitStreaks) and the skid-mark effects
 * (InitSkids) their arrays and host element sizes. Elements are linked through their first pointer, and the
 * doubly linked ones back through their second. The skid-mark reset clears the whole skid table at its PC size, and
 * StopSkid writes a skid's midpoint and its done flag at the effect's PC offsets, past its links and
 * its display handle (docs/port/BRINGUP.md 5).
 * game: src/blob/InitStatLL.c src/blob/ClearStatLL.c src/blob/GetStatLLE.c
 * game: src/blob/FreeStatLLE.c src/ovl_b/InitStreaks.c src/blob/InitSkids.c src/blob/StopSkid.c
 */
#include <string.h>

#include "pc_game_types.h"
#include "support.h"

typedef struct {
    int8_t dbl;
    int32_t numElements, elementSize;
    void *list, *head, *store;
} StatLL;
_Static_assert(sizeof(StatLL) == 40, "the header's host size");
typedef struct Trail {
    struct Trail *next;
    uint32_t unknown04, flags;
    float lifetime, origin[3], previous[3], lower[3], upper[3];
} Trail;   /* runtime_b_trail's */
typedef struct Fade {
    struct Fade *next;
    uint32_t unknown04;
    void *object;
    float lifetime, interval;
    uint8_t color[4];
    Trail *source;
} Fade;
typedef struct Effect {
    struct Effect *next, *prev;
    float f8;
    void *dl;
    float pos[3];
    uint8_t color[4];
    int32_t f20;
} Effect;  /* skid_marks' EffectObj */
typedef struct {
    Effect *obj;
    float start[3], pos[3], lenSq, dir[3], quad[6], left[3], right[3];
} WheelSlot;   /* skid_marks' (N64 0x5C bytes) */
_Static_assert(sizeof(WheelSlot) == 0x60, "a skid slot's host size");

StatLL gStreakList, gStreakPolyList, gSkidList;
Trail gStreakArray[50];
Fade gStreakPolyArray[150];
Effect gSkidArray[100];
_Static_assert(sizeof(WheelSlot) == sizeof(struct pc_skid), "the PC declaration's size");
struct pc_skid pc_skids[PC_MAX_LINKS][4];   /* the skid table (data.c's in the game), PC_MAX_LINKS cars */
#define gNewSkid (*(WheelSlot (*)[PC_MAX_LINKS][4])pc_skids)
uint8_t after_skids[16];   /* what follows the skid table: the reset must not reach it */

void InitStatLL(void *head, void *array, int32_t size, int32_t count, uint8_t dbl);
void *GetStatLLE(void *list);
void FreeStatLLE(void *list, void *e);
void InitStreaks(void);
void InitSkids(void);
void StopSkid(WheelSlot *slot);
static int g_freed_dl;
void ZOID_RemovePoly(void *dl) { g_freed_dl++; }
void *n64_memset(void *p, int c, uint32_t n) { return memset(p, c, n); }

static int count_free(StatLL *l)
{
    int n = 0;
    void **e;
    for (e = l->store; e; e = *e)
        n++;
    return n;
}

int main(void)
{
    Trail *t0, *t1, *t2;
    Effect *e0, *e1, *e2;
    int i;

    memset(&gStreakList, 0xA5, sizeof(gStreakList));
    InitStreaks();
    CHECK(gStreakList.list == gStreakArray && gStreakList.numElements == 50);
    CHECK(gStreakList.elementSize == (int32_t)sizeof(Trail) && gStreakList.dbl == 0);
    CHECK(gStreakList.head == NULL && gStreakList.store == gStreakArray);
    CHECK(gStreakPolyList.list == gStreakPolyArray && gStreakPolyList.elementSize == (int32_t)sizeof(Fade));
    for (i = 0; i < 49; i++)
        CHECK(gStreakArray[i].next == &gStreakArray[i + 1]);
    CHECK(gStreakArray[49].next == NULL && count_free(&gStreakPolyList) == 150);

    /* Singly linked: the newest element is the head; FreeStatLLE unlinks it from wherever it is. */
    t0 = GetStatLLE(&gStreakList);
    t1 = GetStatLLE(&gStreakList);
    t2 = GetStatLLE(&gStreakList);
    CHECK(t0 == &gStreakArray[0] && t1 == &gStreakArray[1] && t2 == &gStreakArray[2]);
    CHECK(gStreakList.head == t2 && t2->next == t1 && t1->next == t0 && t0->next == NULL);
    t1->flags = 0x1234;
    FreeStatLLE(&gStreakList, t1);
    CHECK(gStreakList.head == t2 && t2->next == t0 && gStreakList.store == t1 && count_free(&gStreakList) == 48);
    CHECK(GetStatLLE(&gStreakList) == t1 && t1->flags == 0x1234);   /* reused, fields kept */
    for (i = 0; i < 47; i++)
        CHECK(GetStatLLE(&gStreakList) != NULL);
    CHECK(GetStatLLE(&gStreakList) == NULL);   /* all 50 out */

    /* Doubly linked (the skid-mark effects): re-initialising frees what is active first, and clears all of the skid
     * table (PC_MAX_LINKS cars of four slots of 0x60 bytes here), the last car's last wheel too. */
    memset(gNewSkid, 0xA5, sizeof gNewSkid);
    memset(after_skids, 0x5A, sizeof after_skids);
    InitSkids();
    for (i = 0; i < (int)sizeof gNewSkid; i++)
        if (((uint8_t *)gNewSkid)[i] != 0)
            break;
    CHECK(i == (int)sizeof gNewSkid && gNewSkid[PC_MAX_LINKS - 1][3].obj == NULL);
    CHECK(after_skids[0] == 0x5A && after_skids[15] == 0x5A);
    CHECK(gSkidList.dbl == 1 && gSkidList.list == gSkidArray && gSkidList.elementSize == (int32_t)sizeof(Effect));
    e0 = GetStatLLE(&gSkidList);
    e1 = GetStatLLE(&gSkidList);
    e2 = GetStatLLE(&gSkidList);
    CHECK(gSkidList.head == e2 && e2->next == e1 && e1->next == e0 && e1->prev == e2 && e2->prev == NULL);
    e1->dl = &g_freed_dl;
    FreeStatLLE(&gSkidList, e1);
    CHECK(e2->next == e0 && e0->prev == e2 && gSkidList.store == e1);
    FreeStatLLE(&gSkidList, e2);   /* the head */
    CHECK(gSkidList.head == e0 && e0->prev == NULL);
    /* StopSkid: the effect gets the midpoint of the skid's start and end and its done flag; its links, its time and
     * its display handle stay. */
    e0->f8 = 2.5f;
    e0->dl = &g_freed_dl;
    gNewSkid[2][1].obj = e0;
    for (i = 0; i < 3; i++)
        gNewSkid[2][1].start[i] = (float)i, gNewSkid[2][1].pos[i] = 10.0f + i;
    StopSkid(&gNewSkid[2][1]);
    CHECK(gNewSkid[2][1].obj == NULL && e0->f20 == 1);
    CHECK(e0->pos[0] == 5.0f && e0->pos[1] == 6.0f && e0->pos[2] == 7.0f);
    CHECK(e0->f8 == 2.5f && e0->dl == &g_freed_dl && e0->next == NULL && e0->prev == NULL);
    e0->dl = NULL;
    g_freed_dl = 0;
    InitSkids();
    CHECK(g_freed_dl == 1 && gSkidList.head == NULL && count_free(&gSkidList) == 100);
    return TEST_DONE();
}
