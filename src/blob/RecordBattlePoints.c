/* RecordBattlePoints: count a battle point (MAT3 gamerecords.c). */
#include "game/game.h"

/* Adds delta to the points player row scored against player column in this battle (gBattlePoints, 4 x 4). */
void RecordBattlePoints(s8 row,s8 column,s8 delta) {
 gBattlePoints[row][column]+=delta;
}
