/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * initTargetStructs (historical label): resets the level-object system before objects are
 * instantiated. Initialises the node pool gTargetList (130 nodes of 112 bytes at gTargetArray; the
 * pool type is ClearStatLL's), copies gRestartGame to Restarting, clears the CollisionTargetPtrs
 * table (InitCollisionTargetPtrs), the per-category counts gTargetStructCount[8], gPathCount, gBattleMineCnt and four
 * 20-byte records at gPlayerMines; when gRace == 6 it allocates (audio_dma_sync, historical label;
 * only when Restarting is clear) a 120-byte block for a second pool (15 x 8 bytes, gAlwaysColList), sets
 * category 5's count to 16 and calls the overlay function 0x803914A8; finally it zeroes the counters
 * gBBoxModelCount, gSilverCoinCnt, gGoldCoinCnt, gWhichSilverCoin, gWhichGoldCoin (incremented by SetupTarget).
 * No arcade ancestor identified. The argument is unused (retail still stores a0 to its home slot).
 * Shaping quirks: the two pool set-ups are one inlined static helper (pool_init; argument order
 * mem, count, size sets the t6/t7/t8 order, and its pointer parameter puts the second pool's memory in
 * v0); gSilverCoinCnt and gWhichSilverCoin are cleared with `*= 0` (read-modify-write folded by uopt, which
 * leaves the address in v0/v1 as retail does; `= 0`, `&= 0`, `-= x`, volatile all differ).
 */
typedef signed char s8; typedef unsigned char u8; typedef short s16; typedef int s32;
/* PC: the node pool's elements (the level-object nodes, 112 bytes) keep their N64 layout in the arena block
 * gTargetArray, so their links are 32-bit window addresses: the header's n64 flag (docs/port/POINTERS.md 10). */
typedef struct { u8 b0; u8 n64; u8 pad2[2]; s32 count; s32 size; u8 *mem; intptr_t f16; u8 *free; } Pool;
typedef struct { intptr_t w[5]; } Rec20;   /* MineSlots (AnimateMine): four target pointers and a count */
/* PC: the mine list's elements (AnimateMine's Link, CheckTargetCollision's Node: next and the target) are 16 bytes here,
 * 8 on the N64: their heap block and element size are the host ones (docs/port/POINTERS.md 9). */
typedef struct { void *next, *target; } PcLink;
#define ELEMENT_SIZE(n64, type) sizeof(type)
extern Pool gTargetList;
extern u8 gTargetArray[130 * 112];
extern s8 gRestartGame;
extern s8 Restarting;
extern s32 gPathCount;
extern u8 gTargetStructCount[8];
extern s32 gBattleMineCnt;
extern Rec20 gPlayerMines[4];
extern s32 gRace;
extern void *gAlwaysCol;
extern Pool gAlwaysColList;
extern s32 gBBoxModelCount;
extern s32 gSilverCoinCnt;
extern s32 gGoldCoinCnt;
extern s32 gWhichSilverCoin;
extern s32 gWhichGoldCoin;
void ClearStatLL(Pool *p);
void InitCollisionTargetPtrs(void);
void *AllocateMemoryHeap(s32 arg0, s32 arg1);
void PreInitBattlePowerUps(void);

static void pool_init(Pool *p, void *mem, s32 count, s32 size) {
    p->mem = mem;
    p->count = count;
    p->size = size;
    p->b0 = 0;
    ClearStatLL(p);
}

void initTargetStructs(s32 arg0) {
    s16 i;

    gTargetList.n64 = 1; pool_init(&gTargetList, gTargetArray, 130, 112);
    Restarting = gRestartGame;
    InitCollisionTargetPtrs();
    gPathCount = 0;
    for (i = 0; i < 8; i++) {
        gTargetStructCount[i] = 0;
    }
    gBattleMineCnt = 0;
    for (i = 0; i < 4; i++) {
        gPlayerMines[i].w[0] = 0;
        gPlayerMines[i].w[1] = 0;
        gPlayerMines[i].w[2] = 0;
        gPlayerMines[i].w[3] = 0;
        gPlayerMines[i].w[4] = 0;
    }
    if (gRace == 6) {
        if (Restarting == 0) {
            gAlwaysCol = AllocateMemoryHeap(0, ELEMENT_SIZE(120, PcLink[15]));
        }
        gTargetStructCount[5] = 16;
        PreInitBattlePowerUps();
        pool_init(&gAlwaysColList, gAlwaysCol, 15, ELEMENT_SIZE(8, PcLink));
    }
    gBBoxModelCount = 0;
    gSilverCoinCnt *= 0;
    gGoldCoinCnt = 0;
    gWhichSilverCoin *= 0;
    gWhichGoldCoin = 0;
}
