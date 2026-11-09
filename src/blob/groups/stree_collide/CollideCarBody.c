/* CollideCarBody: a car against the track's walls (MAT3 stree.c). */
#include "game/game.h"

/* A contact with a wall (MAT3 anon_surf_uvs_30). */
typedef struct CCol {
    surftyp *surf;  /* the wall, NULL none */
    f32 uvs[9];     /* its frame */
    s16 carEdge;    /* the car's corner (tireIndex) */
    s16 index;
    f32 dist;       /* how far the car is into it */
} CCol;

/* The model record (MAT3 tModelData): the corners' positions now and last step, and each tyre's last node of the
 * surface tree (TireSurf finds it). */
typedef struct VCar {
    _tCar *car;
    Tune44 *tune;
    u8 p8[620];
    f32 pos[4][3];      /* 0x274 bodyrwr: the corners' positions */
    f32 old[4][3];      /* 0x2A4 bodyrwr2: the same last step */
    u8 p724[204];
    streetyp *lasttp[4];   /* 0x3A0 each tyre's last node of the surface tree */
} VCar;

#define FLOOR(v) (((v) < 0.0f && (f32)(s32)(v) != (v)) ? (v) - 1.0f : (v))

/* Tests model `car`'s body against the walls round its four tyres: the surfaces of the tree's leaves under them
 * (each leaf once) but those already tested this step (flag 0x4000, set while testing), soft ones (0x2000) and
 * removed ones (kind 15); the deepest contact pushes the car back (CarApplyCollision). Then a corner that went down
 * through a half-pipe surface (0x2000) since the last step is pushed back up out of it. */
void CollideCarBody(VCar *car)
{
    streetyp *cells[4];
    s16 cellIdx[4];
    u16 polys[142];
    s32 npoly;
    s32 ncells = 0;
    s32 j;
    s32 i;
    s32 k;
    s16 out;
    u8 cnt;
    CCol col = { 0 };
    s32 unique;
    u32 off;
    u8 *q;
    surftyp *p;
    surftyp *poly;
    f32 bound;
    f32 pt[3];
    f32 w[3];
    s16 outIdx;

    for (j = 0; j < 4; j++) {
        p = (surftyp *)updownleaf(car->lasttp[j], (s32)FLOOR(car->pos[j][0]), (s32)FLOOR(car->pos[j][2]), &out);
        if (p != NULL) {
            unique = 1;
            for (i = ncells - 1; i >= 0; i--) {
                if ((streetyp *)p == cells[i] && out == cellIdx[i]) {
                    unique = 0;
                    break;
                }
            }
            if (unique) {
                cellIdx[ncells] = out;
                cells[ncells] = (streetyp *)p;
                ncells++;
            }
        }
    }
    npoly = 0;
    for (j = 0; j < ncells; j++) {
        off = cells[j]->child[cellIdx[j]];
        if (cells[j]->flags & (16 << cellIdx[j])) {
            off |= 0x10000;
        }
        if (off != 0) {
            q = stree_slist + off;
            cnt = *q++;
            DecodeSlist(q, cnt, &polys[npoly], 0);
            for (i = npoly; i < cnt + npoly; i++) {
                p = &stree_surf[polys[i]];
                if (col.surf == p || (p->flags & 0x4000) || (p->flags & 0x2000) || (p->flags & 0xF) == 15) {
                    continue;
                }
                CollideCarBodyAgainstSurface(car, p, &col);
                p->flags |= 0x4000;
            }
            npoly += cnt;
        }
    }
    if (col.surf != NULL) {
        CarApplyCollision(car, &col);
    }
    for (j = 0; j < npoly; j++) {
        poly = &stree_surf[polys[j]];
        poly->flags &= ~0x4000;
        if (poly->flags & 0x2000) {
            for (i = 0; i < 4; i++) {
                bound = 250.0f;
                if (PointInSurf(poly, car->pos[tireIndex[i]], car->old[tireIndex[i]], pt, &bound, &outIdx, col.uvs, -5.0f) != 0) {
                    fHalfPipeInterp((intptr_t)poly, outIdx, car->pos[i], pt, (f32 (*)[3])col.uvs, 0.0f);
                    bound = pt[1];
                    if (bound < -5.0f || bound >= 0.0f) {
                        continue;
                    }
                    pt[0] = car->pos[tireIndex[i]][0] - car->old[tireIndex[i]][0];
                    pt[1] = car->pos[tireIndex[i]][1] - car->old[tireIndex[i]][1];
                    pt[2] = car->pos[tireIndex[i]][2] - car->old[tireIndex[i]][2];
                    BodyVector(pt, w, col.uvs);
                    if (w[1] >= 0.0f) {
                        continue;
                    }
                    col.surf = poly;
                    col.carEdge = i;
                    col.index = i;
                    col.dist = -bound;
                    CarApplyCollision(car, &col);
                }
            }
        }
    }
    for (k = 0; k < 4; k++) {
        car->old[k][0] = car->pos[k][0];
        car->old[k][1] = car->pos[k][1];
        car->old[k][2] = car->pos[k][2];
    }
}
