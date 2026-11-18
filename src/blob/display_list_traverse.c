/* display_list_traverse: a walk over a display list and the lists it calls, for MirrorWaveInstance (which flips the
 * triangles' winding through it); N64 only (MAT3's models have no display lists). The lists are the WAD converter's
 * records (port/assets/wad.c): a call, a branch and a GFX_OP_BRANCH_Z hold the list they name. */
#include "game/game.h"

/* flags */
#define DLT_ENTER_BRANCH 1      /* walk into a GFX_OP_BRANCH_Z's list and come back */
#define DLT_FOLLOW_BRANCH 2     /* go on at a GFX_OP_BRANCH_Z's list */
#define DLT_SKIP_CALLS 4        /* don't walk into the lists a call names */
#define DLT_REPLACE 8           /* a call or branch to match goes to replace instead */

/* Walks the display list at dl to its end. A call is walked into (unless DLT_SKIP_CALLS), a branch followed; with
 * DLT_REPLACE a call or branch whose list is match gets replace instead (the walk still goes into match). A
 * GFX_OP_BRANCH_Z is entered or followed as flags say. Every other record goes to cb, which returns how many records
 * to step; with no cb they are stepped over one at a time.
 * PC: a call or a branch that names no list (where the N64's word named no place in the file: the WAD converter said
 * so) is not followed: a call is stepped over, a branch ends the walk, where the N64 went into whatever lay there. */
void display_list_traverse(Gfx *dl, Gfx *match, Gfx *replace, s32 flags, s32 (*cb)(Gfx *)) {
    Gfx *target;

    while (dl->op != GFX_OP_END_LIST) {
        if (dl->op == GFX_OP_CALL_LIST || dl->op == GFX_OP_BRANCH_LIST) {
            target = dl->list.list;
            if ((flags & DLT_REPLACE) && target == match) {
                dl->list.list = replace;
            }
            if (dl->op == GFX_OP_CALL_LIST) {
                dl++;
                if (!(flags & DLT_SKIP_CALLS) && target != NULL) {
                    display_list_traverse(target, match, replace, flags, cb);
                }
            } else if (target == NULL) {
                return;
            } else {
                dl = target;
            }
        } else if (dl->op == GFX_OP_BRANCH_Z) {
            target = dl->branch_z.list;
            dl++;
            if (flags & DLT_FOLLOW_BRANCH) {
                if (target == NULL) {
                    return;
                }
                dl = target;
            } else if ((flags & DLT_ENTER_BRANCH) && target != NULL) {
                display_list_traverse(target, match, replace, flags, cb);
            }
        } else if (cb != 0) {
            dl += cb(dl);
        } else {
            dl++;
        }
    }
}
