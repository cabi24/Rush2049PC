/* NewPassword: start a password (MAT3 password.c). */
#include "game/game.h"

/* Starts building or reading a password of dataBits bits of data and checksumBits bits of checksum: its data cleared
 * (at most 32 bytes), no bits used. The first call also builds gInversePasswordAlphabet, each of the 32 letters'
 * value. Returns 0 when the password is too long or the checksum over 32 bits. */
s32 NewPassword(u32 dataBits, u32 checksumBits)
{
    static s32 first = 1;
    s32 i;
    if (first) {
        first = 0;
        for (i = 0; i < 32; i++) {
            gInversePasswordAlphabet[gPasswordAlphabet[i]] = i;
        }
    }
    gNumBytes = (dataBits + checksumBits + 7) >> 3;
    if (gNumBytes > 32) return 0;
    if (checksumBits > 32) return 0;
    for (i = 0; i < gNumBytes; i++) {
        password_gData[i] = 0;
    }
    gDataBits = dataBits;
    gChecksumBits = checksumBits;
    gNumBits = 0;
    return 1;
}
