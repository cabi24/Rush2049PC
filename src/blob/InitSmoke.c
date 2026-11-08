/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * Effect/collision pool reset (historical label InitSmoke;
 * N64 code, no arcade ancestor found). Sets up the pool gSmokeList
 * (100 records of 88 bytes in gSmokeObjs, flag cleared) and builds its free
 * list (ClearStatLL), zeroes the six words gCarSmokeDir[], and, when
 * expansion_pak_present is set or gTrackNum >= 6, calls MBOX_FindTexture_WORK for each of six
 * entries (FireTexNames[i], &gFireTexs[i], 0, (s8)(gNumModels - 1), 1).
 *
 * Real source form: the pool set-up is a call of the kept function
 * InitStatLL(&gSmokeList, gSmokeObjs, 88, 100, 0) (= pool_init:
 * mem, size, count, flag, then ClearStatLL), which umerge inlines
 * in the whole-program unit; its statements then carry the call's .loc, so
 * as1 orders the four stores as one source line (size/count/mem/flag).
 * the whole-program unit score is EQUAL with that call
 * Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10.
 * the one-line POOL_SETUP macro below reproduces the same single .loc; on
 * four separate lines (or a static helper, which keeps its own lines) the
 * store order is mem/count/size (4 words off).
 * gNumModels is u8 (lbu), re-read per iteration. Also matches at -O2.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;

/* PC: a StatLL list header's host layout, as every view has it (docs/port/POINTERS.md 9): doubly flag, element
 * count and size, then the element array, the active list and the free list pointer-sized (40 bytes). */
typedef struct Pool {
    u8 flag;
    int count;
    int size;
    void *mem;
    void *head, *store;
} Pool;
/* The puffs (UpdateSmoke's Puff: 88 bytes on the N64, 96 here, linked through their first pointer): the array and the
 * element size are the host ones. */
typedef struct { void *next; float matrix[3][3], position[3]; int id; u8 pad56[4]; float life, timer, velocity[3];
    unsigned int flags; s16 car, frame; } PcPuff;
extern Pool gSmokeList;
extern PcPuff gSmokeObjs[100];
#define ELEMENT_SIZE(n64, type) sizeof(type)
/* On the PC the tire-effect latches are pc_tire_fx, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md 6),
 * and the reset clears all PC_MAX_LINKS of them: on the N64 the 6 it has. */
#define gCarSmokeDir pc_tire_fx
extern s8 expansion_pak_present;
extern s8 gTrackNum;
extern char *FireTexNames[6];   /* the six model names: N64 pointers in the data, host pointers once loaded */
extern s16 gFireTexs[6];
extern u8 gNumModels;
void ClearStatLL(Pool *pool);
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
