#include "game/game.h"

void GetPasswordString(char *out);

/*
 * decode `str` into the bit buffer and verify its checksum.
 * Shaping (all compile-affecting): one u16 counter `i` for both loops, set to 0 before the call to
 * CalcChecksum and again in the for-init (retail keeps it in t1 across the call, which that callee
 * preserves, and still loads the constant 0 for the loop's working copy); the table value is an int
 * local (`s32 v`; with `u8 v` uopt swaps the operands of the 0xFF compare); the checksum loop advances
 * the bit position in the for-header as `pos += 1` (with `pos++` got/pos swap registers).
 */
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
        if (v == 0xFF) {
            gDataBits -= gChecksumBits;
            return 0;
        }
        if (!StorePasswordElement(v, 5)) {
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

/* checksum of the payload bits */
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

/*
 * append the checksum to the bit buffer and encode the whole buffer as password characters
 * (5 bits per character through the gPasswordAlphabet alphabet), NUL-terminated, into `out`
 * . Shaping: `i` is the bit position in both loops; the first loop's
 * counter `j` is its own variable (sharing `n` lets uopt skip re-zeroing `n` on the empty-loop
 * path); `j++, i++, sum >>= 1` in the for-header in that order; `i = 0` stated before the second
 * loop's empty for-init.
 */
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

