/* port/assets/tests/test_pool.c: the game's own StatLL list code on the PC layouts (docs/port/POINTERS.md 9): the
 * header (40 bytes: doubly flag, count, size, element array, active list, free list), InitStatLL (struct_fields_init)
 * and its free list (pool_linked_list_init), GetStatLLE and FreeStatLLE on a singly and a doubly linked list, and
 * the initializers that give overlay B's trail and fade lists (func_8038D1A8) and the skid-mark effects
 * (func_800B0580) their arrays and host element sizes. Elements are linked through their first pointer, and the
 * doubly linked ones back through their second. The skid-mark reset clears the whole skid table at its PC size, and
 * StopSkid (func_800AF844) writes a skid's midpoint and its done flag at the effect's PC offsets, past its links and
 * its display handle (docs/port/BRINGUP.md 5).
 * game: src/blob/struct_fields_init.c src/blob/pool_linked_list_init.c src/blob/func_8008E3C0.c
 * game: src/blob/func_800AFA84.c src/ovl_b/func_8038D1A8.c src/blob/func_800B0580.c src/blob/func_800AF844.c
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

StatLL D_80395EE8, D_80396B80, D_80155220;
Trail D_80395F00[50];
Fade D_80396B98[150];
Effect D_80155B30[100];
_Static_assert(sizeof(WheelSlot) == sizeof(struct pc_skid), "the PC declaration's size");
struct pc_skid pc_skids[PC_MAX_LINKS][4];   /* the skid table (data.c's in the game), PC_MAX_LINKS cars */
#define D_80155290 (*(WheelSlot (*)[PC_MAX_LINKS][4])pc_skids)
uint8_t after_skids[16];   /* what follows the skid table: the reset must not reach it */

void struct_fields_init(void *head, void *array, int32_t size, int32_t count, uint8_t dbl);
void *func_8008E3C0(void *list);             /* GetStatLLE */
void func_800AFA84(void *list, void *e);     /* FreeStatLLE */
void func_8038D1A8(void);
void func_800B0580(void);
void func_800AF844(WheelSlot *slot);         /* StopSkid */
static int g_freed_dl;
void func_8008D0C0(void *dl) { g_freed_dl++; }
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

    memset(&D_80395EE8, 0xA5, sizeof(D_80395EE8));
    func_8038D1A8();
    CHECK(D_80395EE8.list == D_80395F00 && D_80395EE8.numElements == 50);
    CHECK(D_80395EE8.elementSize == (int32_t)sizeof(Trail) && D_80395EE8.dbl == 0);
    CHECK(D_80395EE8.head == NULL && D_80395EE8.store == D_80395F00);
    CHECK(D_80396B80.list == D_80396B98 && D_80396B80.elementSize == (int32_t)sizeof(Fade));
    for (i = 0; i < 49; i++)
        CHECK(D_80395F00[i].next == &D_80395F00[i + 1]);
    CHECK(D_80395F00[49].next == NULL && count_free(&D_80396B80) == 150);

    /* Singly linked: the newest element is the head; FreeStatLLE unlinks it from wherever it is. */
    t0 = func_8008E3C0(&D_80395EE8);
    t1 = func_8008E3C0(&D_80395EE8);
    t2 = func_8008E3C0(&D_80395EE8);
    CHECK(t0 == &D_80395F00[0] && t1 == &D_80395F00[1] && t2 == &D_80395F00[2]);
    CHECK(D_80395EE8.head == t2 && t2->next == t1 && t1->next == t0 && t0->next == NULL);
    t1->flags = 0x1234;
    func_800AFA84(&D_80395EE8, t1);
    CHECK(D_80395EE8.head == t2 && t2->next == t0 && D_80395EE8.store == t1 && count_free(&D_80395EE8) == 48);
    CHECK(func_8008E3C0(&D_80395EE8) == t1 && t1->flags == 0x1234);   /* reused, fields kept */
    for (i = 0; i < 47; i++)
        CHECK(func_8008E3C0(&D_80395EE8) != NULL);
    CHECK(func_8008E3C0(&D_80395EE8) == NULL);   /* all 50 out */

    /* Doubly linked (the skid-mark effects): re-initialising frees what is active first, and clears all of the skid
     * table (PC_MAX_LINKS cars of four slots of 0x60 bytes here), the last car's last wheel too. */
    memset(D_80155290, 0xA5, sizeof D_80155290);
    memset(after_skids, 0x5A, sizeof after_skids);
    func_800B0580();
    for (i = 0; i < (int)sizeof D_80155290; i++)
        if (((uint8_t *)D_80155290)[i] != 0)
            break;
    CHECK(i == (int)sizeof D_80155290 && D_80155290[PC_MAX_LINKS - 1][3].obj == NULL);
    CHECK(after_skids[0] == 0x5A && after_skids[15] == 0x5A);
    CHECK(D_80155220.dbl == 1 && D_80155220.list == D_80155B30 && D_80155220.elementSize == (int32_t)sizeof(Effect));
    e0 = func_8008E3C0(&D_80155220);
    e1 = func_8008E3C0(&D_80155220);
    e2 = func_8008E3C0(&D_80155220);
    CHECK(D_80155220.head == e2 && e2->next == e1 && e1->next == e0 && e1->prev == e2 && e2->prev == NULL);
    e1->dl = &g_freed_dl;
    func_800AFA84(&D_80155220, e1);
    CHECK(e2->next == e0 && e0->prev == e2 && D_80155220.store == e1);
    func_800AFA84(&D_80155220, e2);   /* the head */
    CHECK(D_80155220.head == e0 && e0->prev == NULL);
    /* StopSkid: the effect gets the midpoint of the skid's start and end and its done flag; its links, its time and
     * its display handle stay. */
    e0->f8 = 2.5f;
    e0->dl = &g_freed_dl;
    D_80155290[2][1].obj = e0;
    for (i = 0; i < 3; i++)
        D_80155290[2][1].start[i] = (float)i, D_80155290[2][1].pos[i] = 10.0f + i;
    func_800AF844(&D_80155290[2][1]);
    CHECK(D_80155290[2][1].obj == NULL && e0->f20 == 1);
    CHECK(e0->pos[0] == 5.0f && e0->pos[1] == 6.0f && e0->pos[2] == 7.0f);
    CHECK(e0->f8 == 2.5f && e0->dl == &g_freed_dl && e0->next == NULL && e0->prev == NULL);
    e0->dl = NULL;
    g_freed_dl = 0;
    func_800B0580();
    CHECK(g_freed_dl == 1 && D_80155220.head == NULL && count_free(&D_80155220) == 100);
    return TEST_DONE();
}
