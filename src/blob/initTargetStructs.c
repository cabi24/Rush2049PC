/* initTargetStructs: the targets' lists at a track's set-up (MAT3 targets.c). */
#include "game/game.h"

/* InitStatLL, as the N64 code has it here: `count` elements of `size` bytes at `mem`, none in use, singly linked. */
static void pool_init(tStatList *p, void *mem, s32 count, s32 size) {
    p->list = mem;
    p->numElements = count;
    p->elementSize = size;
    p->dbl = 0;
    ClearStatLL(p);
}

/* Sets up the targets for a track: gTargetList's 130 nodes, pc_target_nodes (MAT3 gTargetArray: on the N64 0x70
 * bytes a node in the game's BSS, linked by 32-bit addresses; on the PC tTarget's host layout, PC data, linked by
 * pointers like every StatLL: docs/port/NATIVE.md step 3), no path targets or kinds' records counted yet, no player
 * mines; in battle the list of targets every car is tested against (15 of them, allocated once, a restart keeping
 * it; the PC's elements are 16 bytes, the N64's 8) and the power-ups' records (PreInitBattlePowerUps). No boxes, no
 * coins. `unused` is not read. */
void initTargetStructs(s32 unused) {
    s16 i;

    pool_init(&gTargetList, pc_target_nodes, PC_TARGET_NODES, sizeof(tTarget));
    Restarting = gRestartGame;
    InitCollisionTargetPtrs();
    gPathCount = 0;
    for (i = 0; i < 8; i++) {
        gTargetStructCount[i] = 0;
    }
    gBattleMineCnt = 0;
    for (i = 0; i < 4; i++) {
        gPlayerMines[i].target[0] = 0;
        gPlayerMines[i].target[1] = 0;
        gPlayerMines[i].target[2] = 0;
        gPlayerMines[i].target[3] = 0;
        gPlayerMines[i].count = 0;
    }
    if (gRace == kBattle) {
        if (Restarting == 0) {
            gAlwaysCol = AllocateMemoryHeap(0, sizeof(tAlwaysCol[15]));
        }
        gTargetStructCount[5] = 16;
        PreInitBattlePowerUps();
        pool_init(&gAlwaysColList, gAlwaysCol, 15, sizeof(tAlwaysCol));
    }
    gBBoxModelCount = 0;
    gSilverCoinCnt = 0;
    gGoldCoinCnt = 0;
    gWhichSilverCoin = 0;
    gWhichGoldCoin = 0;
}
