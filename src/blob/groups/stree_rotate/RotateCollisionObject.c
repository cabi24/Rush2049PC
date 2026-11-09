#include "game/game.h"


#define vecadd(a,b,r)	{r[0]=a[0]+b[0]; r[1]=a[1]+b[1]; r[2]=a[2]+b[2];}
typedef struct Item {
    /* 0x00 */ u16 tag;
    /* 0x02 */ u16 matrix;
    /* 0x04 */ s16 rot[9];
    /* 0x16 */ u16 point;
    /* 0x18 */ s16 pos[3];
    /* 0x1E */ u16 frac;
} Item;

typedef struct OutMatrix {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s16 rot[9];
    /* 0x16 */ s16 pad;
} OutMatrix;

typedef struct OutPoint {
    /* 0x00 */ s16 pos[3];
    /* 0x06 */ u16 frac;
} OutPoint;

VIEW(Item *, stree_save);
#define stree_save stree_save_view
VIEW(OutMatrix *, stree_surf);
#define stree_surf stree_surf_view
VIEW(OutPoint *, stree_svert);
#define stree_svert stree_svert_view

void RotateCollisionObject(s32 tag, f32 *origin, f32 m1[3][3], f32 m2[3][3]) {
    s32 unused0;
    s32 ipos[3];
    s32 unused1;
    s32 unused2;
    f32 pos[3];
    f32 rel[3];
    f32 mat[3][3];
    f32 tmp[3][3];
    Item *item;
    s16 *out;
    OutPoint *pt;
    u32 i;
    s32 count;

    count = 0;
    item = stree_save;
    for (i = 0; i < stree_savenum; i++, item++) {
        if (tag == item->tag) {
            count++;
            mats2fcopy(item->rot, (f32 *) mat);
            MulUVS(m1, mat, tmp);
            MulUVS(m2, tmp, mat);
            out = stree_surf[item->matrix].rot;
            out[0] = mat[0][0] * 16384.0f;
            out[1] = mat[0][1] * 16384.0f;
            out[2] = mat[0][2] * 16384.0f;
            out[3] = mat[1][0] * 16384.0f;
            out[4] = mat[1][1] * 16384.0f;
            out[5] = mat[1][2] * 16384.0f;
            out[6] = mat[2][0] * 16384.0f;
            out[7] = mat[2][1] * 16384.0f;
            out[8] = mat[2][2] * 16384.0f;
            pos[0] = ((item->pos[0] << 5) + ((item->frac & 0x7C00) >> 10)) * 0.03125f;
            pos[1] = ((item->pos[1] << 5) + ((item->frac & 0x3E0) >> 5)) * 0.03125f;
            pos[2] = ((item->pos[2] << 5) + (item->frac & 0x1F)) * 0.03125f;
            rel[0] = pos[0] - origin[0];
            rel[1] = pos[1] - origin[1];
            rel[2] = pos[2] - origin[2];
            WorldVector(rel, pos, (f32 *) m1);
            WorldVector(pos, rel, (f32 *) m2);
            vecadd(rel, origin, pos);
            ipos[0] = pos[0] * 32.0f;
            ipos[1] = pos[1] * 32.0f;
            ipos[2] = pos[2] * 32.0f;
            pt = &stree_svert[item->point];
            pt->frac = (((ipos[0] & 0x1F) << 10) + ((ipos[1] & 0x1F) << 5)) + (ipos[2] & 0x1F);
            pt->pos[0] = ipos[0] >> 5;
            pt->pos[1] = ipos[1] >> 5;
            pt->pos[2] = ipos[2] >> 5;
        }
    }
    if (count == 0) {
    }
}

/* stand-in: retail mats2fcopy has callers outside this group (PointInSurf, CylinderInSurf);
 * this kept caller only keeps it out of line here.  Not a claimed member. */
