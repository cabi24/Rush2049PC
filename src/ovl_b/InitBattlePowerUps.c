/* InitBattlePowerUps: the battle arena's power-ups at a battle's start (MAT3 powerups.c). */
#include "game/game.h"

/* Finds the arena's power-ups: the targets in use flagged 0x20 (gTargetList's list), each in the list
 * gBattlePowerUps.tS (allocated unless `restart`: a restart keeps it; a PC entry holds a host pointer, 16 bytes).
 * Then the times until each kind of power-up appears: health in 5 to 15 seconds, invisibility and shield in 15 to
 * 60, one of the two 45 later. */
void InitBattlePowerUps(s32 restart)
{
    tTarget *t;
    s32 n;
    if (restart == 0) {
        gBattlePowerUps.tS = AllocateMemoryHeap(0, gBattlePowerUps.num * (s32) sizeof(tBattlePowerUpT));
    }
    n = 0;
    for (t = gTargetList.head; t != NULL; t = t->next) {
        if (t->Flags & 0x20) {
            gBattlePowerUps.tS[n].t = t;
            gBattlePowerUps.tS[n].last = 1;
            gBattlePowerUps.tS[n].type = 1;
            n++;
        }
    }
    gBattlePowerUps.healthTLeft = Random(10.0f) + 5.0f;
    gBattlePowerUps.invsTLeft = Random(45.0f) + 15.0f;
    gBattlePowerUps.shieldTLeft = Random(45.0f) + 15.0f;
    if (Random(1.0f) > 0.5f) {
        gBattlePowerUps.shieldTLeft += 45.0f;
    } else {
        gBattlePowerUps.invsTLeft += 45.0f;
    }
}
