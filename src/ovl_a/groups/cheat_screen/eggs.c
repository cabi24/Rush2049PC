#include "game/game.h"

VIEW(Egg *, gEgg);
#define gEgg gEgg_view

/* MAT3 eggs.c InitEgg: the cheat code `egg` the player is entering. */
void InitEgg(s32 egg)
{
    gEgg = &gEggs[egg];
    gNextTimeOut = 0.0f;
    gEggIdx = 0;
}
