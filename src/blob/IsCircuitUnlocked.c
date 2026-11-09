#include "game/game.h"

s32 IsCircuitUnlocked(s32 field)
{
    s32 found = 0;
    s32 i;

    for (i = 0; i < gNumPlayers; i++) {
        if (gUnlockedCircuits[i][field]) {
            found = 1;
        }
    }
    return found;
}
