/* game_rand.c: the game's random number generator (MAT3: the C library's rand()).
 *
 * Every random choice in the game (Random, the drones, the effects) comes from this one sequence, so its state
 * rand_seed decides replays and the synthetic traces. */
#include "game/game.h"

/* The next number of the classic C library linear congruential sequence (seed * 1103515245 + 12345), 0 to 32767:
 * bits 16-30 of the new seed. */
int game_rand(void) {
    rand_seed = rand_seed * 1103515245 + 12345;
    return (rand_seed >> 16) & 0x7fff;
}
