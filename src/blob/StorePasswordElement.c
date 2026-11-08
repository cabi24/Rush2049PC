/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

extern u8 password_gData[];
extern u16 gNumBytes;
extern u16 gDataBits;
extern u16 gChecksumBits;
extern u16 gNumBits;
extern u8 gInversePasswordAlphabet[];
extern char *gPasswordAlphabet;


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

