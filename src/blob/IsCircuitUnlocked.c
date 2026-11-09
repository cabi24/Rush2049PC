/* IsCircuitUnlocked: whether a circuit is unlocked for anyone playing (MAT3 save.c). */
#include "game/game.h"

/* 1 when circuit field is unlocked for any of the gNumPlayers players (gUnlockedCircuits). */
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
