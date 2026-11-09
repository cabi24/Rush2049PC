#include "game/game.h"

VIEW(s8[][4], gBattlePoints);
#define gBattlePoints gBattlePoints_view
void RecordBattlePoints(s8 row,s8 column,s8 delta) {
 gBattlePoints[row][column]+=delta;
}
