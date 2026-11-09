/* InitEgg: the secret pad sequences ("eggs": the cheat codes) (MAT3 eggs.c InitEgg; CheckForEgg follows the
 * player's presses). */
#include "game/game.h"

/* Start watching for code `egg` (an index of gEggs) from its first press, with no time limit yet. */
void InitEgg(s32 egg)
{
    gEgg = &gEggs[egg];
    gNextTimeOut = 0.0f;
    gEggIdx = 0;
}
