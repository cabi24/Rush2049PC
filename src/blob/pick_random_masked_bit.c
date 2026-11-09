/* pick_random_masked_bit.c: a random bit number from a mask table. */
#include "game/game.h"

/* A random bit number 0..31 whose bit is set in random_bit_masks[index]: draw Random(32) until one hits the mask
 * (the mask must not be zero, or this never returns). */
u8 pick_random_masked_bit(u8 index)
{
    u8 bit;

    do {
        bit = Random(32.0f);
    } while (!(random_bit_masks[index] & (1 << bit)));
    return bit;
}
