#include "game/game.h"

typedef struct Pool { u8 b0; u8 n64; u8 pad2[2]; s32 count; s32 size; u8 *mem; intptr_t f16; u8 *free; } Pool;
typedef struct { intptr_t w[5]; } Rec20;   /* MineSlots (AnimateMine): four target pointers and a count */
/* PC: the mine list's elements (AnimateMine's Link, CheckTargetCollision's Node: next and the target) are 16 bytes here,
 * 8 on the N64: their heap block and element size are the host ones (docs/port/POINTERS.md 9). */
typedef struct { void *next, *target; } PcLink;
#define ELEMENT_SIZE(n64, type) sizeof(type)
VIEW(Pool, gTargetList);
#define gTargetList gTargetList_view
VIEW(u8[130 * 112], gTargetArray);
#define gTargetArray gTargetArray_view

VIEW(u8[8], gTargetStructCount);
#define gTargetStructCount gTargetStructCount_view

VIEW(Rec20[4], gPlayerMines);
#define gPlayerMines gPlayerMines_view

VIEW(Pool, gAlwaysColList);
#define gAlwaysColList gAlwaysColList_view

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
