/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Reset the pool gSkidList: release every node still on its active list
 * (ZOID_RemovePoly on the node's data, then FreeStatLLE unlinks it), set it
 * up again as 100 records of 36 bytes in gSkidArray with flag 1, rebuild the
 * free list and clear the 2208-byte table gNewSkid. N64 code, no arcade
 * ancestor found. Same-shape sibling of InitSmoke (found by
 * searching for ClearStatLL callers).
 *
 * Real source form: InitStatLL(&gSkidList, gSkidArray, 36, 100, 1),
 * the kept pool-init function, inlined by umerge in the unit (the whole-program unit
 *Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10.
 * one-line POOL_SETUP macro gives the same single-.loc store order.
 * The loop re-reads the list head as `while (pool.head) { n = pool.head; ...}`
 * (retail tests v0 and copies it to s0).
 */
typedef unsigned char u8;

/* PC: the skid-mark effects (skid_marks' EffectObj) of this doubly linked list: the two links are pointer-sized, so
 * the element is 56 bytes here (36 on the N64), and so are the array and the element size (docs/port/POINTERS.md 9). */
typedef struct Node {
    void *next, *prev;
    int pad8;
    void *data;     /* 12 */
} Node;
typedef struct { void *next, *prev; float f8; void *dl; float pos[3]; u8 color[4]; int f20; } PcEffectObj;
#define ELEMENT_SIZE(n64, type) sizeof(type)
/* The skid table gNewSkid (skid_marks' WheelSlot[6][4]: 0x5C bytes on the N64, 0x60 here, its effect pointer being
 * pointer-sized): all of it, or the last wheel's slot keeps a stale effect pointer after a reset. On the PC it is
 * pc_skids, PC_MAX_LINKS cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md 6). */
#define SKID_TABLE_BYTES sizeof (pc_skids)

typedef struct Pool {
    u8 flag;
    int count;
    int size;
    void *mem;
    Node *head;     /* 16 */
    Node *store;    /* the whole StatLL header (shots.c), as every view has it on the PC */
} Pool;

extern Pool gSkidList;
extern PcEffectObj gSkidArray[100];
#define gNewSkid PC_SKIDS
void ClearStatLL(Pool *pool);
void ZOID_RemovePoly(void *data);
void FreeStatLLE(Pool *pool, Node *node);
void *memset(void *, int, unsigned int);

/* one-line multi-statement macro: stands in for the inlined kept InitStatLL (one .loc) */
#define POOL_SETUP(pool, m, sz, n, f) { (pool).mem = (m); (pool).count = (n); (pool).size = (sz); (pool).flag = (f); }

void InitSkids(void)
{
    Node *n;

    while (gSkidList.head != 0) {
        n = gSkidList.head;
        ZOID_RemovePoly(n->data);
        FreeStatLLE(&gSkidList, n);
    }
    POOL_SETUP(gSkidList, gSkidArray, ELEMENT_SIZE(36, PcEffectObj), 100, 1);
    ClearStatLL(&gSkidList);
    memset(gNewSkid, 0, SKID_TABLE_BYTES);
}
