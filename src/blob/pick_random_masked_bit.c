#include "game/game.h"

u8 pick_random_masked_bit(u8 index)
{
    u8 bit;

    do {
        bit = Random(32.0f);
    } while (!(random_bit_masks[index] & (1 << bit)));
    return bit;
}
