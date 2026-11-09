/* CheckButtons: the players' buttons during a race (MAT3 game.c has none; the N64's loop does nothing). */
#include "game/game.h"

/* Walks the players and does nothing for each: the body the N64 build compiled out. */
void CheckButtons(void)
{
    s16 i;
    for (i = 0; i < gNumPlayers; i++) {}
}
