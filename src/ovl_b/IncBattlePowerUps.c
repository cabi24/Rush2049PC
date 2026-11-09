/* IncBattlePowerUps: one more power-up on the arena (MAT3 powerups.c). */
#include "game/game.h"

/* The power-ups' count (gBattlePowerUps.count, its first byte). */

/* Counts one more power-up (a target with the power-up record kind, SetupTarget's count). */
void IncBattlePowerUps(void)
{
    gBattlePowerUps.num++;
}
