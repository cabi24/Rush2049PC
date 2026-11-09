/* playerBeingDeleted: is another window deleting a profile? (MAT3 playerscr.c playerBeingDeleted) */
#include "game/game.h"

/* 1 if a player other than `skip` is deleting a profile (kPlayerStateDeletePlayer), else 0. The first argument is
 * not used: the count is always gNumPlayers. */
s32 playerBeingDeleted(s32 count, s32 skip)
{
    s8 i;
    s32 found = 0;

    count = gNumPlayers;
    for (i = 0; i < count; i++) {
        if (i != skip && gPlayerState[i] == kPlayerStateDeletePlayer) {
            found = 1;
        }
    }
    return found;
}
