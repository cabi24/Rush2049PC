#include "game/game.h"

VIEW(u8, gTeamPoints);
#define gTeamPoints gTeamPoints_view

extern void *memset(void *s, s32 c, u32 n);

void InitBattlePoints(void) {
    memset(&gBattlePoints, 0, 0x10U);
    memset(&gTeamPoints, 0, 4U);
}
