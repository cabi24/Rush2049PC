#include "game/game.h"

VIEW(s8, gBattlePowerUps);
#define gBattlePowerUps gBattlePowerUps_view

void IncBattlePowerUps(void)
{
    gBattlePowerUps++;
}
