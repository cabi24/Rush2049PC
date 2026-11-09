/* relocate_texture_loads.c: point a wave's texture load display lists at its loaded images.
 *
 * When a wave (an asset bundle) is loaded (waves.c), its texture load display lists (TXLD) still hold image
 * addresses relative to the wave's image chunk (IMAG); this adds the chunk's address to them. */
#include "game/game.h"
#define Gfx N64Gfx   /* a display list command in the N64's form, two 32-bit words (include/game's Gfx is the PC's) */

typedef struct Gfx {
    u32 w0;
    u32 w1;
} Gfx;

/* The rebased address past 16 MiB of a larger window (game.memory) is the K0 one, not the 24-bit offset; at
 * original, the N64's (pc_rcp_reloc, docs/port/MEMORY.md 3). */
#define REBASE(w, b) pc_rcp_reloc(w, b, -1)

/* Rebase the address (the second word) of every G_SETTIMG, and of every G_RDPHALF_1 followed by a G_BRANCH_Z, in
 * the commands from start to end, by the image chunk's address base. */
void relocate_texture_loads(Gfx *start, Gfx *end, s32 base) {
    Gfx *gfx;
    u8 cmd;
    u8 next;
    u32 val;
    s32 off;

    for (gfx = start; gfx < end; gfx++) {
        cmd = (DLW(gfx->w0) & 0xFF000000) >> 24;
        if ((cmd & 0xC0) != 0x40 && (cmd & 0xC0) != 0x80 && (cmd < 9 || cmd >= 0x40) && (cmd < 0xC0 || cmd >= 0xD6)) {
            next = (DLW(gfx[1].w0) & 0xFF000000) >> 24;
            if (cmd == G_SETTIMG) {
                off = base;
                val = REBASE(DLW(gfx->w1), off);
                DLW_SET(gfx->w1, val);
            }
            if (cmd == G_RDPHALF_1 && next == G_BRANCH_Z) {
                off = base;
                val = REBASE(DLW(gfx->w1), off);
                DLW_SET(gfx->w1, val);
            }
        }
    }
}
