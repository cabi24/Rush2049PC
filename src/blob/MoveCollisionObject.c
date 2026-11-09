/* MoveCollisionObject: moves a collision object's surfaces with its target (MAT3 stree.c). */
#include "game/game.h"

VIEW(savetyp *, stree_save);
#define stree_save stree_save_view
VIEW(sverttyp *, stree_svert);
#define stree_svert stree_svert_view

/* Moves the vertices of collision object `num`'s saved surfaces by `trans` (feet), keeping 1/32-foot positions
 * rounded to nearest. A target path moves its collision object so (PlacePathTargetPos). */
void MoveCollisionObject(s32 num, f32 *trans) {
    s32 ipos[4];
    f32 pos[4];
    savetyp *s;
    sverttyp *vp;
    s32 n;
    u32 i;

    n = 0;
    s = stree_save;
    for (i = 0; i < stree_savenum; i++, s++) {
        if (num == s->object) {
            n++;
            vp = &stree_svert[s->vnum];
            pos[0] = ((vp->pos[0] << 5) + ((vp->overflow & 0x7C00) >> 10)) * 0.03125f;
            pos[1] = ((vp->pos[1] << 5) + ((vp->overflow & 0x3E0) >> 5)) * 0.03125f;
            pos[2] = ((vp->pos[2] << 5) + (vp->overflow & 0x1F)) * 0.03125f;
            pos[0] = pos[0] + trans[0];
            pos[1] = pos[1] + trans[1];
            pos[2] = pos[2] + trans[2];
            if (pos[0] * 32.0f < 0.0f) {
                ipos[0] = pos[0] * 32.0f - 0.5f;
            } else {
                ipos[0] = pos[0] * 32.0f + 0.5f;
            }
            if (pos[1] * 32.0f < 0.0f) {
                ipos[1] = pos[1] * 32.0f - 0.5f;
            } else {
                ipos[1] = pos[1] * 32.0f + 0.5f;
            }
            if (pos[2] * 32.0f < 0.0f) {
                ipos[2] = pos[2] * 32.0f - 0.5f;
            } else {
                ipos[2] = pos[2] * 32.0f + 0.5f;
            }
            vp->pos[0] = ipos[0] >> 5;
            vp->pos[1] = ipos[1] >> 5;
            vp->pos[2] = ipos[2] >> 5;
            vp->overflow = ((ipos[0] << 10) & 0x7C00) | ((ipos[1] << 5) & 0x3E0) | (ipos[2] & 0x1F);
        }
    }
    if (n == 0) {
        return;
    }
}
