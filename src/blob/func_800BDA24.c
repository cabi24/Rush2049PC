/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef unsigned int u32;
typedef unsigned char u8;
#ifdef TARGET_PC
/* PC: the WAD's TXLD lists stay in N64 form, 8-byte big-endian commands (port/assets/wad.c). */
#define DLW(w) pc_be32_get(&(w))
#define DLW_SET(w, v) pc_be32_set(&(w), v)
#else
#define DLW(w) w
#define DLW_SET(w, v) w = v
#endif
u32 func_800BDA24(u32 *dl) {
    u8 op;
    for (;;dl+=2) {
        op=(DLW(dl[0])&0xFF000000)>>24;
        if ((op&0xC0)==0x40 || (op&0xC0)==0x80 || (op>=9 && op<0x40) || (op>=0xC0 && op<0xD6)) {
            continue;
        } else if (op==0xFD) {
            return DLW(dl[1]);
        } else if (op==0xDF) {
            return;
        } else {
            continue;
        }
    }
}
