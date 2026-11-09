#include "game/game.h"

typedef struct {
    /* 0x00 */ u16 id;
    /* 0x02 */ char pad02[0x14];
    /* 0x16 */ u16 vtx;
    /* 0x18 */ char pad18[8];
} Rec20;

typedef struct {
    /* 0x00 */ s16 pos[3];
    /* 0x06 */ u16 frac;
} PVtx;

VIEW(Rec20 *, stree_save);
#define stree_save stree_save_view
VIEW(PVtx *, stree_svert);
#define stree_svert stree_svert_view

void MoveCollisionObject(s32 id, f32 *off) {
    s32 ipos[4];
    f32 pos[4];
    Rec20 *r;
    PVtx *v;
    s32 n;
    u32 i;

    n = 0;
    r = stree_save;
    for (i = 0; i < stree_savenum; i++, r++) {
        if (id == r->id) {
            n++;
            v = &stree_svert[r->vtx];
            pos[0] = ((v->pos[0] << 5) + ((v->frac & 0x7C00) >> 10)) * 0.03125f;
            pos[1] = ((v->pos[1] << 5) + ((v->frac & 0x3E0) >> 5)) * 0.03125f;
            pos[2] = ((v->pos[2] << 5) + (v->frac & 0x1F)) * 0.03125f;
            pos[0] = pos[0] + off[0];
            pos[1] = pos[1] + off[1];
            pos[2] = pos[2] + off[2];
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
            v->pos[0] = ipos[0] >> 5;
            v->pos[1] = ipos[1] >> 5;
            v->pos[2] = ipos[2] >> 5;
            v->frac = ((ipos[0] << 10) & 0x7C00) | ((ipos[1] << 5) & 0x3E0) | (ipos[2] & 0x1F);
        }
    }
    if (n == 0) {
        return;
    }
}
