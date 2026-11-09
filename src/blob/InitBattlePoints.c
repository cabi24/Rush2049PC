/* InitBattlePoints: clear the battle's points (MAT3 gamerecords.c). */
#include "game/game.h"

VIEW(u8, gTeamPoints);
#define gTeamPoints gTeamPoints_view

extern void *memset(void *s, s32 c, u32 n);

/* Clears the points each player scored against each other (gBattlePoints, 4 x 4) and the teams' (gTeamPoints). */
void InitBattlePoints(void) {
    memset(&gBattlePoints, 0, 0x10U);
    memset(&gTeamPoints, 0, 4U);
}
