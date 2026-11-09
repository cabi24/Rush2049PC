/* seeded_rand: a random number from a seed of its own (the circuits' tracks and drones). */
#include "game/game.h"

/* Steps the linear congruential generator `seed` (the C library's rand constants) and returns its next number,
 * 0 to 32767. A circuit's choices come from its seed so that a circuit code makes the same circuit. */
s32 seeded_rand(u32 *seed) {
    *seed = *seed * 1103515245 + 12345;
    return (*seed >> 16) & 0x7fff;
}
