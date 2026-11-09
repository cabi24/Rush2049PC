#include "game/game.h"

extern void *memset(void *s, s32 c, u32 n);

VIEW(s32, gTotalRaceStats);
#define gTotalRaceStats gTotalRaceStats_view

VIEW(s32, gTotalStuntStats);
#define gTotalStuntStats gTotalStuntStats_view

VIEW(s32, gTotalBattleStats);
#define gTotalBattleStats gTotalBattleStats_view

VIEW(s32, gTotalObstacleStats);
#define gTotalObstacleStats gTotalObstacleStats_view

VIEW(s32, gTotalCircuitStats);
#define gTotalCircuitStats gTotalCircuitStats_view

void ClearTotalStats(void) {
    memset(&gTotalRaceStats, 0, 0x480U);
    memset(&gTotalStuntStats, 0, 0x100U);
    memset(&gTotalBattleStats, 0, 0x60U);
    memset(&gTotalObstacleStats, 0, 0x18U);
    memset(&gTotalCircuitStats, 0, 0x70U);
}
