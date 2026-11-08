/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * NewPassword (0x800DC628, 248 B): open the bit-packing buffer used by the
 * StorePasswordElement/GetPasswordElement bit writer/reader (32-symbol code alphabet).
 * On the first call it builds the reverse lookup gInversePasswordAlphabet[alphabet[i]] = i
 * for the 32 symbols of *gPasswordAlphabet.  Then: byte count gNumBytes =
 * (bits + extra + 7) / 8, refuses more than 32 bytes or extra > 32, clears
 * password_gData[0..bytes), records the bit capacity (gDataBits), the extra
 * count (gChecksumBits) and resets the bit cursor (gNumBits).
 *
 * Quirk: the one-shot flag at 0x801170F8 is a function-local static
 * (initialised to 1, own .data verified by the scorer). As an extern s32 IDO
 * shares one lui/addiu base between the load and the store (10 words off);
 * as a local static the load and store each get their own lui like retail.
 */
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
