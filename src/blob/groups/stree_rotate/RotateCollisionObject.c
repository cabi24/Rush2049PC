/* RotateCollisionObject: turns a collision object's surfaces with its target (MAT3 stree.c). */
#include "game/game.h"

#define vecadd(a,b,r)	{r[0]=a[0]+b[0]; r[1]=a[1]+b[1]; r[2]=a[2]+b[2];}

/* A surface (MAT3 surftyp, stree_surf[]): its orientation. */
typedef struct surftyp {
    u16 flags;      /* 0x00 */
    u16 snd_flags;  /* 0x02 */
    s16 uvs[9];     /* 0x04 its orientation, 1.0 = 0x4000 (MAT3 s16[3][3]) */
    u16 vli;        /* 0x16 */
} surftyp;

VIEW(savetyp *, stree_save);
#define stree_save stree_save_view
VIEW(surftyp *, stree_surf);
#define stree_surf stree_surf_view
VIEW(sverttyp *, stree_svert);
#define stree_svert stree_svert_view

/* Turns collision object `num` about `origin`: each saved surface's orientation (as the track had it) is turned
 * by m1 then m2 (MulUVS), and its vertex's position by m1 then m2 about the origin (WorldVector), truncated to 1/32
 * foot. A target path turns its collision object so, m1 its first point's orientation transposed (undoing it) and
 * m2 its present one (PlacePathTargetQuat, StartPathTarget). */
void RotateCollisionObject(s32 num, f32 *origin, f32 m1[3][3], f32 m2[3][3]) {
    s32 unused0;
    s32 ipos[3];
    s32 unused1;
    s32 unused2;
    f32 pos[3];
    f32 rel[3];
    f32 mat[3][3];
    f32 tmp[3][3];
    savetyp *s;
    s16 *out;
    sverttyp *pt;
    u32 i;
    s32 count;

    count = 0;
    s = stree_save;
    for (i = 0; i < stree_savenum; i++, s++) {
        if (num == s->object) {
            count++;
            mats2fcopy(s->uvs, (f32 *) mat);
            MulUVS(m1, mat, tmp);
            MulUVS(m2, tmp, mat);
            out = stree_surf[s->snum].uvs;
            out[0] = mat[0][0] * 16384.0f;
            out[1] = mat[0][1] * 16384.0f;
            out[2] = mat[0][2] * 16384.0f;
            out[3] = mat[1][0] * 16384.0f;
            out[4] = mat[1][1] * 16384.0f;
            out[5] = mat[1][2] * 16384.0f;
            out[6] = mat[2][0] * 16384.0f;
            out[7] = mat[2][1] * 16384.0f;
            out[8] = mat[2][2] * 16384.0f;
            pos[0] = ((s->pos[0] << 5) + ((s->overflow & 0x7C00) >> 10)) * 0.03125f;
            pos[1] = ((s->pos[1] << 5) + ((s->overflow & 0x3E0) >> 5)) * 0.03125f;
            pos[2] = ((s->pos[2] << 5) + (s->overflow & 0x1F)) * 0.03125f;
            rel[0] = pos[0] - origin[0];
            rel[1] = pos[1] - origin[1];
            rel[2] = pos[2] - origin[2];
            WorldVector(rel, pos, (f32 *) m1);
            WorldVector(pos, rel, (f32 *) m2);
            vecadd(rel, origin, pos);
            ipos[0] = pos[0] * 32.0f;
            ipos[1] = pos[1] * 32.0f;
            ipos[2] = pos[2] * 32.0f;
            pt = &stree_svert[s->vnum];
            pt->overflow = (((ipos[0] & 0x1F) << 10) + ((ipos[1] & 0x1F) << 5)) + (ipos[2] & 0x1F);
            pt->pos[0] = ipos[0] >> 5;
            pt->pos[1] = ipos[1] >> 5;
            pt->pos[2] = ipos[2] >> 5;
        }
    }
    if (count == 0) {
    }
}
