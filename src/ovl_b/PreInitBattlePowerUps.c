/* PreInitBattlePowerUps: the power-ups' count before a battle's targets are set up (MAT3 powerups.c). */
#include "game/game.h"

/* The power-ups' count (gBattlePowerUps.count, its first byte). */

/* No power-ups counted yet (initTargetStructs, before SetupTarget counts them). */
void PreInitBattlePowerUps(void)
{
    gBattlePowerUps.num = 0;
}
