#include "game/game.h"

s32 playerBeingDeleted(s32 count, s32 skip)
{
    s8 i;
    s32 found = 0;

    count = gNumPlayers;
    for (i = 0; i < count; i++) {
        if (i != skip && gPlayerState[i] == 4) {
            found = 1;
        }
    }
    return found;
}
