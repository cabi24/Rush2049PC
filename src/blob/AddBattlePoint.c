/* AddBattlePoint: a battle point was scored (MAT3 hudextra.c's _AddBattlePoint, called from the race). */
#include "game/game.h"

/* In a battle or stunt race, shows a point p flying from player from to player to (AddBattlePointInternal, in
 * runtime image B). */
void AddBattlePoint(s8 to, s8 from, s8 p) {
    if ((gRace == kBattle) || (gRace == kStunt)) {
        AddBattlePointInternal(to, from, p);
    }
}
