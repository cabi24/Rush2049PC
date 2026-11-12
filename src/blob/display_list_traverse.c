/* display_list_traverse: a walk over a display list and the lists it calls, for MirrorWaveInstance (which flips the
 * triangles' winding through it); N64 only (MAT3's models have no display lists). The lists are the PC's own form,
 * the WAD converter's (port/assets/wad.c): a command's w1 holds a pointer where it names a list. */
#include "game/game.h"

/* The F3DEX2 commands the walk follows (a command's first word, its first byte the opcode). */
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
 * one that branches is followed; with DLT_REPLACE a G_DL whose target is match gets replace instead (the walk still
 * goes into match). A G_BRANCH_Z is entered or followed as flags say; a G_RDPHALF_1 without a G_BRANCH_Z after it is
 * stepped over with the command after it. Every other command goes to cb, which returns how many commands to step;
 * with no cb they are stepped over one at a time. */
void display_list_traverse(Gfx *dl, Gfx *match, Gfx *replace, s32 flags, s32 (*cb)(Gfx *)) {
    u32 w0;
    Gfx *target;

    while (dl->words.w0 != G_ENDDL_WORD) {
        w0 = dl->words.w0;
        if ((w0 & 0xFF000000) == G_DL_OP) {
            target = (Gfx *)dl->words.w1;
            if ((flags & DLT_REPLACE) && target == match) {
                dl->words.w1 = (uintptr_t)replace;
            }
            dl++;
            if (!(w0 & 0x00FF0000)) {
                if (!(flags & DLT_SKIP_CALLS)) {
                    display_list_traverse(target, match, replace, flags, cb);
                }
            } else {
                dl = target;
            }
        } else if ((w0 & 0xFF000000) == G_RDPHALF_1_OP) {
            target = (Gfx *)dl->words.w1;
            dl++;
            if ((dl->words.w0 & 0xFF000000) == G_BRANCH_Z_OP) {
                dl++;
                if (flags & DLT_FOLLOW_BRANCH) {
                    dl = target;
                } else if (flags & DLT_ENTER_BRANCH) {
                    display_list_traverse(target, match, replace, flags, cb);
                }
            } else {
                dl++;
            }
        } else if (cb != 0) {
            dl += cb(dl);
        } else {
            dl++;
        }
    }
}
