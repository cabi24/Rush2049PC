/* libultra's CRCs of the Controller Pak commands (crc.c): of an address and of a data block. */
#include "../rom_tu.h"

/* The 5-bit CRC of a pak address (its 11 bits), sent with a pak read or write. */
u8 __osContAddressCrc(u16 addr) {
    u32 temp = 0;
    u32 i = (1 << 10);

    do {
        temp <<= 1;

        if ((u32)addr & i) {
            if (temp & (1 << 5)) {
                temp ^= 0x15 - 1;
            } else {
                ++temp;
            }
        } else if (temp & (1 << 5)) {
            temp ^= 0x15;
        }

        i >>= 1;
    } while (i != 0);

    i = 5;

    do {
        temp <<= 1;
        if (temp & (1 << 5)) {
            temp ^= 0x15;
        }
    } while (--i != 0);

    return temp & ((1 << 5) - 1);
}

/* The 8-bit CRC of a 32-byte block, which the pak returns with a read or write. */
u8 __osContDataCrc(u8* data) {
    u32 temp = 0;
    u32 i;
    u32 j;

    for (i = 32; i; --i) {
        for (j = (1 << (8 - 1)); j; j >>= 1) {
            temp <<= 1;
            if ((*data & j) != 0) {
                if ((temp & (1 << 8)) != 0) {
                    temp ^= 0x85 - 1;
                } else {
                    ++temp;
                }
            } else if (temp & (1 << 8)) {
                temp ^= 0x85;
            }
        }

        data++;
    }
    do {
        temp <<= 1;
        if (temp & (1 << 8)) {
            temp ^= 0x85;
        }
    } while (++i < 8);

    return temp;
}

