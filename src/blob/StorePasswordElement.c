/* StorePasswordElement: add bits to the password being built (MAT3 password.c). */
#include "game/game.h"

/* Appends the low nbits bits of value, lowest first, to the password's data (password_gData, gNumBits used so far).
 * Returns 0 without storing anything when nbits is over 32 or the data would exceed gDataBits, else 1. */
s32 StorePasswordElement(u32 value, u32 nbits)
{
    s32 i;
    if (nbits > 32) return 0;
    if (gDataBits < gNumBits + nbits) return 0;
    for (i = 0; i < nbits; gNumBits++, value >>= 1, i++) {
        password_gData[gNumBits >> 3] |= (value & 1) << (gNumBits & 7);

    }
    return 1;
}
