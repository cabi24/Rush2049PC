/* ClearTotalStats: clear the totals over all profiles (MAT3 gamerecords.c). */
#include "game/game.h"

extern void *memset(void *s, s32 c, u32 n);

/* Clears the race, stunt, battle, obstacle and circuit totals (gTotalRaceStats ...), before the profiles are added
 * up again. */
void ClearTotalStats(void) {
    memset(&gTotalRaceStats, 0, 0x480U);
    memset(&gTotalStuntStats, 0, 0x100U);
    memset(&gTotalBattleStats, 0, 0x60U);
    memset(&gTotalObstacleStats, 0, 0x18U);
    memset(&gTotalCircuitStats, 0, 0x70U);
}
