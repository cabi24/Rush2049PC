#include "game/game.h"

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

