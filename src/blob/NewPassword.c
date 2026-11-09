#include "game/game.h"

s32 NewPassword(u32 a, u32 b)
{
    static s32 first = 1;
    s32 i;
    if (first) {
        first = 0;
        for (i = 0; i < 32; i++) {
            gInversePasswordAlphabet[gPasswordAlphabet[i]] = i;
        }
    }
    gNumBytes = (a + b + 7) >> 3;
    if (gNumBytes > 32) return 0;
    if (b > 32) return 0;
    for (i = 0; i < gNumBytes; i++) {
        password_gData[i] = 0;
    }
    gDataBits = a;
    gChecksumBits = b;
    gNumBits = 0;
    return 1;
}
