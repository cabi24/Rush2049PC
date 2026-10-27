/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
/* Display-list walker (0xDF end, 0xDE call/branch, 0xE1+0x04 branch-z); no quirks. */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;

#ifdef TARGET_PC
/* ROM display lists stay in N64 form on PC: 8-byte big-endian commands with 32-bit N64 addresses
 * (docs/port/POINTERS.md). */
#define DLW(w) pc_be32_get(&(w))
#define DLW_SET(w, v) pc_be32_set(&(w), v)
#define DL_AT(addr) ((u32 *)pc_n64_ptr(addr))
#else
#define DLW(w) w
#define DLW_SET(w, v) w = v
#define DL_AT(addr) (u32 *) addr
#endif
void display_list_traverse(u32 *dl, u32 match, u32 replace, s32 flags, s32 (*cb)(u32 *)) {
    u32 w0;
    u32 addr;

    while (DLW(*dl) != 0xDF000000) {
        if ((DLW(*dl) & 0xFF000000) == 0xDE000000) {
            w0 = DLW(*dl++);
            addr = DLW(*dl);
            if (addr & 0x0F000000) {
                addr &= 0xF0FFFFFF;
            } else if (!(addr & 0xFF000000)) {
                addr += 0x80000000;
            }
            if ((flags & 8) && addr == match) {
                DLW_SET(*dl, replace);
            }
            dl++;
            if (!(w0 & 0x00FF0000)) {
                if (!(flags & 4)) {
                    display_list_traverse(DL_AT(addr), match, replace, flags, cb);
                }
            } else {
                dl = DL_AT(addr);
            }
        } else if ((DLW(*dl) & 0xFF000000) == 0xE1000000) {
            addr = DLW(dl[1]);
            dl += 2;
            if (addr & 0x0F000000) {
                addr &= 0xF0FFFFFF;
            } else if (!(addr & 0xFF000000)) {
                addr += 0x80000000;
            }
            if ((DLW(*dl) & 0xFF000000) == 0x04000000) {
                dl += 2;
                if (flags & 2) {
                    dl = DL_AT(addr);
                } else if (flags & 1) {
                    display_list_traverse(DL_AT(addr), match, replace, flags, cb);
                }
            } else {
                dl += 2;
            }
        } else if (cb != 0) {
            dl += cb(dl);
        } else {
            dl += 2;
        }
    }
}
