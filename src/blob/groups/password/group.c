/* SetPasswordString, CalcChecksum and GetPasswordString: the cheat passwords (MAT3 password.c). A password is
 * gDataBits of data and gChecksumBits of checksum (NewPassword), five bits a letter of gPasswordAlphabet. */
#include "game/game.h"

#define PASSWORD_LETTER_BITS 5
#define NOT_A_LETTER 0xFF   /* gInversePasswordAlphabet of a character not in the alphabet */

void GetPasswordString(char *out);

/* Reads the password str into the password's data (password_gData) and checks its checksum. Returns 1 when every
 * letter is in the alphabet and the checksum agrees, the data then ready to be read from the start
 * (GetPasswordElement); else 0. */
s32 SetPasswordString(u8 *str)
{
    u16 i;
    u8 c;
    s32 v;
    s32 sum;
    s32 got;
    u16 pos;

    for (i = 0; i < gNumBytes; i++) {
        password_gData[i] = 0;
    }
    gNumBits = 0;
    gDataBits += gChecksumBits;
    while ((c = *str) != 0) {
        v = gInversePasswordAlphabet[c];
        str++;
        if (v == NOT_A_LETTER) {
            gDataBits -= gChecksumBits;
            return 0;
        }
        if (!StorePasswordElement(v, PASSWORD_LETTER_BITS)) {
            gDataBits -= gChecksumBits;
            return 0;
        }
    }
    gDataBits -= gChecksumBits;
    i = 0;
    sum = CalcChecksum() & ((1 << gChecksumBits) - 1);
    got = 0;
    pos = gDataBits;
    for (i = 0; i < gChecksumBits; i++, pos += 1) {
        got |= ((password_gData[pos >> 3] >> (pos & 7)) & 1) << i;
    }
    if (sum != got) {
        return 0;
    }
    gNumBits = 0;
    return 1;
}

/* The checksum of the password's data bits: the sum of each set bit's value folded below 1 << gChecksumBits (0 when
 * the password has no checksum). */
s32 CalcChecksum(void)
{
    u32 bit;
    u32 i;
    s32 sum;

    if (gChecksumBits == 0) {
        return 0;
    }
    i = 0;
    sum = 0;
    while (i < gDataBits) {
        bit = password_gData[i >> 3] & (1 << (i & 7));
        i++;
        while (bit >= (1 << gChecksumBits)) {
            bit >>= gChecksumBits;
        }
        sum += bit;
    }
    return sum;
}

/* Appends the checksum to the password's data and writes the whole as letters of gPasswordAlphabet, five bits a
 * letter (the last one padded with zeros), ended by a zero, to out. */
void GetPasswordString(char *out)
{
    u32 sum;
    u16 i;
    u16 j;
    u16 n;
    u16 total;
    u16 k;
    u8 acc;
    u8 *p;
    s32 b;

    sum = CalcChecksum();
    i = gDataBits;
    for (j = 0; j < gChecksumBits; j++, i++, sum >>= 1) {
        p = &password_gData[i >> 3];
        b = i & 7;
        *p &= ~(1 << b);
        *p |= (sum & 1) << b;
    }
    total = gDataBits + gChecksumBits;
    n = 0;
    i = 0;
    k = 0;
    acc = 0;
    for (; i < total; i++) {
        acc |= ((password_gData[i >> 3] >> (i & 7)) & 1) << k;
        if (k == 4) {
            out[n++] = gPasswordAlphabet[acc];
            acc = 0;
            k = 0;
        } else {
            k++;
        }
    }
    if (k != 0) {
        out[n++] = gPasswordAlphabet[acc];
    }
    out[n] = 0;
}

