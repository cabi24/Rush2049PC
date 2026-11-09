#include "game/game.h"

void AddBattlePoint(s8 arg0, s8 arg1, s8 arg2) {
    if ((gRace == 6) || (gRace == 4)) {
        AddBattlePointInternal(arg0, arg1, arg2);
    }
}
