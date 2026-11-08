/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
/* Display-list segment rebase over [start, end). Quirk: the block-local copy `off = base` only
 * serves to found base's web before the w1 load (base then lands in a3); it is copy-propagated away. */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;

typedef struct Gfx {
    u32 w0;
    u32 w1;
} Gfx;

/* ROM display lists stay in N64 form on PC: 8-byte big-endian commands with 32-bit N64 addresses
 * (docs/port/POINTERS.md). */
#define DLW(w) pc_be32_get(&(w))
#define DLW_SET(w, v) pc_be32_set(&(w), v)
/* The rebased address past 16 MiB of a larger window (game.memory) is the K0 one, not the 24-bit offset; at
 * original, the N64's (pc_rcp_reloc, docs/port/MEMORY.md 3). */
#define REBASE(w, b) pc_rcp_reloc(w, b, -1)
void entity_cull_check(Gfx *start, Gfx *end, s32 base) {
    Gfx *gfx;
    u8 cmd;
    u8 next;
    u32 val;
    s32 off;

    for (gfx = start; gfx < end; gfx++) {
        cmd = (DLW(gfx->w0) & 0xFF000000) >> 24;
        if ((cmd & 0xC0) != 0x40 && (cmd & 0xC0) != 0x80 && (cmd < 9 || cmd >= 0x40) && (cmd < 0xC0 || cmd >= 0xD6)) {
            next = (DLW(gfx[1].w0) & 0xFF000000) >> 24;
            if (cmd == 0xFD) {
                off = base;
                val = REBASE(DLW(gfx->w1), off);
                DLW_SET(gfx->w1, val);
            }
            if (cmd == 0xE1 && next == 4) {
                off = base;
                val = REBASE(DLW(gfx->w1), off);
                DLW_SET(gfx->w1, val);
            }
        }
    }
}
