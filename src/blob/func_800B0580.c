/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Reset the pool D_80155220: release every node still on its active list
 * (func_8008D0C0 on the node's data, then func_800AFA84 unlinks it), set it
 * up again as 100 records of 36 bytes in D_80155B30 with flag 1, rebuild the
 * free list and clear the 2208-byte table D_80155290. N64 code, no arcade
 * ancestor found. Same-shape sibling of physics_collision_test (found by
 * searching for pool_linked_list_init callers).
 *
 * Real source form: struct_fields_init(&D_80155220, D_80155B30, 36, 100, 1),
 * the kept pool-init function, inlined by umerge in the unit (the whole-program unit
 *Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10.
 * one-line POOL_SETUP macro gives the same single-.loc store order.
 * The loop re-reads the list head as `while (pool.head) { n = pool.head; ...}`
 * (retail tests v0 and copies it to s0).
 */
typedef unsigned char u8;

#ifdef TARGET_PC
/* PC: the skid-mark effects (skid_marks' EffectObj) of this doubly linked list: the two links are pointer-sized, so
 * the element is 56 bytes here (36 on the N64), and so are the array and the element size (docs/port/POINTERS.md 9). */
typedef struct Node {
    void *next, *prev;
    int pad8;
    void *data;     /* 12 */
} Node;
typedef struct { void *next, *prev; float f8; void *dl; float pos[3]; u8 color[4]; int f20; } PcEffectObj;
#define ELEMENT_SIZE(n64, type) sizeof(type)
/* The skid table D_80155290 (skid_marks' WheelSlot[6][4]: 0x5C bytes on the N64, 0x60 here, its effect pointer being
 * pointer-sized): all of it, or the last wheel's slot keeps a stale effect pointer after a reset. On the PC it is
 * pc_skids, PC_MAX_LINKS cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md 6). */
#define SKID_TABLE_BYTES sizeof (pc_skids)
#else
typedef struct Node {
    int pad0, pad4, pad8;
    void *data;     /* 12 */
} Node;
#define ELEMENT_SIZE(n64, type) n64
#define SKID_TABLE_BYTES 2208
#endif

typedef struct Pool {
    u8 flag;
    int count;
    int size;
    void *mem;
    Node *head;     /* 16 */
#ifdef TARGET_PC
    Node *store;    /* the whole StatLL header (shots.c), as every view has it on the PC */
#endif
} Pool;

extern Pool D_80155220;
#ifdef TARGET_PC
extern PcEffectObj D_80155B30[100];
#else
extern char D_80155B30[];
#endif
#ifdef TARGET_PC
#define D_80155290 PC_SKIDS
#else
extern char D_80155290[];
#endif
void pool_linked_list_init(Pool *pool);
void func_8008D0C0(void *data);
void func_800AFA84(Pool *pool, Node *node);
void *memset(void *, int, unsigned int);

/* one-line multi-statement macro: stands in for the inlined kept struct_fields_init (one .loc) */
#define POOL_SETUP(pool, m, sz, n, f) { (pool).mem = (m); (pool).count = (n); (pool).size = (sz); (pool).flag = (f); }

void func_800B0580(void)
{
    Node *n;

    while (D_80155220.head != 0) {
        n = D_80155220.head;
        func_8008D0C0(n->data);
        func_800AFA84(&D_80155220, n);
    }
    POOL_SETUP(D_80155220, D_80155B30, ELEMENT_SIZE(36, PcEffectObj), 100, 1);
    pool_linked_list_init(&D_80155220);
    memset(D_80155290, 0, SKID_TABLE_BYTES);
}
