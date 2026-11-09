/* display_list_traverse: a walk over an N64 display list and the lists it calls, for MirrorWaveInstance (which
 * flips the triangles' winding through it); N64 only (MAT3's models have no display lists). */
#include "game/game.h"

/* ROM display lists stay in N64 form on PC: 8-byte big-endian commands with 32-bit N64 addresses
 * (docs/port/POINTERS.md). */
#define DLW(w) pc_be32_get(&(w))
#define DLW_SET(w, v) pc_be32_set(&(w), v)
#define DL_AT(addr) ((u32 *)pc_n64_ptr(addr))
/* A K0 address past 16 MiB of a larger window (game.memory: what relocate_model_dl's relocation gives there) is no
 * segmented one, whatever its bits 24-27 (docs/port/MEMORY.md 3); at original none reaches 16 MiB. */
#define SEGMENTED(a) ((a & 0x0F000000) && !pc_rdram_k0_high(a))
/* The F3DEX2 commands the walk follows (the first word of a command, its first byte the opcode). */
#define G_ENDDL_WORD 0xDF000000
#define G_DL_OP 0xDE000000
#define G_RDPHALF_1_OP 0xE1000000
#define G_BRANCH_Z_OP 0x04000000
/* flags */
#define DLT_ENTER_BRANCH 1      /* walk into a G_BRANCH_Z's target and come back */
#define DLT_FOLLOW_BRANCH 2     /* go on at a G_BRANCH_Z's target */
#define DLT_SKIP_CALLS 4        /* don't walk into the lists a G_DL calls */
#define DLT_REPLACE 8           /* replace the G_DL target match by replace */

/* Walks the display list at dl to its G_ENDDL. A G_DL that calls a list is walked into (unless DLT_SKIP_CALLS),
 * one that branches is followed; with DLT_REPLACE a G_DL whose target is match gets replace instead. A G_BRANCH_Z
 * is entered or followed as flags say. Every other command goes to cb, which returns how many words to step (two a
 * command); with no cb they are stepped over. A segmented address is made a plain one (the segment bits dropped),
 * a physical one a K0 address. */
void display_list_traverse(u32 *dl, u32 match, u32 replace, s32 flags, s32 (*cb)(u32 *)) {
    u32 w0;
    u32 addr;

    while (DLW(*dl) != G_ENDDL_WORD) {
        if ((DLW(*dl) & 0xFF000000) == G_DL_OP) {
            w0 = DLW(*dl++);
            addr = DLW(*dl);
            if SEGMENTED(addr) {
                addr &= 0xF0FFFFFF;
            } else if (!(addr & 0xFF000000)) {
                addr += 0x80000000;
            }
            if ((flags & DLT_REPLACE) && addr == match) {
                DLW_SET(*dl, replace);
            }
            dl++;
            if (!(w0 & 0x00FF0000)) {
                if (!(flags & DLT_SKIP_CALLS)) {
                    display_list_traverse(DL_AT(addr), match, replace, flags, cb);
                }
            } else {
                dl = DL_AT(addr);
            }
        } else if ((DLW(*dl) & 0xFF000000) == G_RDPHALF_1_OP) {
            addr = DLW(dl[1]);
            dl += 2;
            if SEGMENTED(addr) {
                addr &= 0xF0FFFFFF;
            } else if (!(addr & 0xFF000000)) {
                addr += 0x80000000;
            }
            if ((DLW(*dl) & 0xFF000000) == G_BRANCH_Z_OP) {
                dl += 2;
                if (flags & DLT_FOLLOW_BRANCH) {
                    dl = DL_AT(addr);
                } else if (flags & DLT_ENTER_BRANCH) {
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
