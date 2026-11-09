/* CountBits: the set bits of a word (MAT3 save.c). */
#include "game/game.h"

/* The number of bits set in the lowest n bits of x (the coins found, the unlocked parts). */
s32 CountBits(u32 x, u8 n)
{
    s32 count = 0;

    while (n != 0 && x != 0) {
        if (x & 1) {
            count++;
        }
        n--;
        x >>= 1;
    }
    return count;
}
