/* Random.c: a random float (MAT3 fmath.c, Random()). */
#include "game/game.h"

/* A random number in [0, max): game_rand's low 15 bits as a fraction of 32768, times max. */
f32 Random(f32 max)
{
    f32 rannum;

    rannum = ((f32)(game_rand() & 0x07FFF) * max) / 32768.0f;
    return rannum;
}
