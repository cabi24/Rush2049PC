#include "game/game.h"

typedef struct PV { s16 x, y, z; u16 w; } PV;
VIEW(PV *, stree_svert);
#define stree_svert stree_svert_view
VIEW(intptr_t, stree_vlist);
#define stree_vlist stree_vlist_view
VIEW(s8[16], BodyVerts);
#define BodyVerts BodyVerts_view

float fabsf(float);
#pragma intrinsic (fabsf)

typedef struct Poly {
    u16 type;
    u16 cnt;
    u8 body[0x12];
    u16 off;
} Poly;
typedef struct CCol {
    Poly *poly;
    f32 m[9];
    s16 idx;
    s16 edge;
    f32 k;
} CCol;
typedef struct CCar {
    u8 p0[628];
    f32 B628[4][3];
    f32 C676[4][3];
    u8 p724[24];
    f32 mat[9];
    u8 p784[816];
    s8 b1600;
    u8 p1601[131];
    s16 f6C4;
    u8 p1734[7];
    s8 b1741;
    u8 p1742[248];
    s16 pidx;
} CCar;

#define DECODE(v, e) \
    (v)[0] = (f32) (((e)->x << 5) + (((e)->w & 0x7C00) >> 10)) * 0.03125f; \
    (v)[1] = (f32) (((e)->y << 5) + (((e)->w & 0x3E0) >> 5)) * 0.03125f; \
    (v)[2] = (f32) (((e)->z << 5) + ((e)->w & 0x1F)) * 0.03125f

void CollideCarBodyAgainstSurface(CCar *car, Poly *poly, CCol *col) {
    u32 n;
    s32 type;
    u16 idx[16];
    PV *v;
    f32 org[3];
    f32 d[3];
    f32 pts[8][3];
    f32 pv[16][2];
    f32 mtx[9];
    s32 conv;
    s32 ka;
    s32 kb;
    s32 k;
    s32 e;
    f32 best;
    f32 da;
    f32 db;
    f32 dy;
    f32 dx;
    f32 dz;
    f32 t;
    f32 u;
    f32 px;
    s32 bestk;
    s32 beste;
    f32 pz;
    f32 ex;
    f32 hx;
    f32 hz;
    f32 ez;
    s32 i;
    Poly *old;

    conv = 0;
    n = poly->cnt;
    n &= 0xF;
    DecodeVlist((u8 *) (poly->off + stree_vlist), n, (s16 *) idx);
    i = idx[0];
    v = &stree_svert[i];
    DECODE(org, v);
    mats2fcopy(poly->body, mtx);
    for (ka = 0; ka < 4; ka++) {
        d[0] = car->B628[tireIndex[ka]][0] - org[0];
        d[1] = car->B628[tireIndex[ka]][1] - org[1];
        d[2] = car->B628[tireIndex[ka]][2] - org[2];
        BodyVector(d, pts[ka], mtx);
    }
    type = poly->type & 0xF;
    if (type == 5 || type == 6) {
        dz = -3.0f;
    } else if (mtx[3] * car->mat[3] + mtx[4] * car->mat[4] + mtx[5] * car->mat[5] < -0.707f) {
        dz = -4.5f;
    } else {
        dz = -0.5f;
    }
    BodyVector(&car->mat[3], d, mtx);
    for (ka = 0; ka < 4; ka++) {
        pts[ka + 4][0] = pts[ka][0] + d[0] * dz;
        pts[ka + 4][1] = pts[ka][1] + d[1] * dz;
        pts[ka + 4][2] = pts[ka][2] + d[2] * dz;
    }
    best = 0.0f;
    bestk = -1;
    for (e = 0; e < 8; e++) {
        ka = BodyVerts[e];
        kb = BodyVerts[e + 8];
        da = pts[ka][1];
        db = pts[kb][1];
        if (da * db < 0.0f) {
            if (conv == 0) {
                for (k = n - 1; k > 0; k--) {
                    v = &stree_svert[idx[k]];
                    pv[k][0] = (f32) ((v->x << 5) + ((v->w & 0x7C00) >> 10)) * 0.03125f;
                    pv[k][1] = (f32) ((v->z << 5) + (v->w & 0x1F)) * 0.03125f;
                }
                conv = 1;
            }
            dx = pts[kb][0] - pts[ka][0];
            dz = pts[kb][2] - pts[ka][2];
            dy = db - da;
            if (dy != 0.0f) {
                u = fabsf(da / dy);
                hx = pts[ka][0] + dx * u;
                hz = pts[ka][2] + dz * u;
                if (dy > 0.0f) {
                    t = u;
                    dx = -dx;
                    dz = -dz;
                } else {
                    t = 1.0f - u;
                }
                px = pv[1][0];
                pz = 0.0f;
                if (!(-px * hz > 0.0f)) {
                    if (dz != 0.f) {
                        u = -hz / dz;
                        if (u > 0.0f && u < t) {
                            t = u;
                        }
                    }
                    for (k = 2; k < n; k++) {
                        ez = pv[k][1] - pz;
                        ex = pv[k][0] - px;
                        u = (hx - px) * ez;
                        if (ex * 1.0f * (hz - pz) < u) {
                            goto next;
                        }
                        u = ex * dz - ez * dx;
                        if (u != 0.0f) {
                            u = ((hx - px) * ez - ex * 1.0f * (hz - pz)) / u;
                            if (u > 0.0f && u < t) {
                                t = u;
                            }
                        }
                        px = pv[k][0];
                        pz = pv[k][1];
                    }
                    if (!(pz * hx < px * hz)) {
                        u = px * dz - pz * dx;
                        type = poly->type & 0xF;
                        if (u != 0.0f) {
                            u = (pz * hx - px * hz) / u;
                            if (u > 0.0f && u < t) {
                                t = u;
                            }
                        }
                        if (type == 4) {
                            if (PC_CAR_STATE(car->pidx) == 0 && PC_CAR_B358(car->pidx) == 0 && car->f6C4 == -1) {
                                AddBattlePoint(car->pidx, car->pidx, -1);
                                car->b1600 = 1;
                            }
                        } else if (type == 7) {
                            if (PC_CAR_STATE(car->pidx) == 0 && PC_CAR_B358(car->pidx) == 0 && car->f6C4 == -1) {
                                car->b1741 = 1;
                                AbortCar(car->pidx, 1);
                            }
                        } else if (type == 3) {
                            if (PC_CAR_STATE(car->pidx) == 0 && PC_CAR_B358(car->pidx) == 0 && car->f6C4 == -1) {
                                car->b1741 = 1;
                                AbortCar(car->pidx, 1);
                                if (PC_LINK_OWNER(car->pidx) == 6) {
                                    PlayGameSound(21, 0, 1, 2);
                                }
                            }
                            goto end;
                        }
                        t *= dy;
                        if (t < 0.f) {
                            t = -t;
                        }
                        if (best < t) {
                            k = (-da / db < 1.0f) ? ka : kb;
                            org[0] = car->B628[tireIndex[k]][0] - car->C676[tireIndex[k]][0];
                            org[1] = car->B628[tireIndex[k]][1] - car->C676[tireIndex[k]][1];
                            org[2] = car->B628[tireIndex[k]][2] - car->C676[tireIndex[k]][2];
                            BodyVector(org, d, mtx);
                            if (!(d[1] >= 0.0f)) {
                                best = t;
                                beste = e;
                                bestk = k;
                            }
                        }
                    }
                }
            }
        }
next:;
    }
    if (bestk >= 0) {
        if (col->poly != NULL) {
            if (col->k < best) {
                CarApplyCollision(car, col);
                col->poly = NULL;
                CollideCarBodyAgainstSurface(car, poly, col);
                return;
            } else {
                old = col->poly;
                col->poly = poly;
                CopyUV(mtx, col->m);
                col->idx = bestk;
                col->edge = beste;
                col->k = best;
                CarApplyCollision(car, col);
                col->poly = NULL;
                CollideCarBodyAgainstSurface(car, old, col);
                return;
            }
        } else {
            col->poly = poly;
            CopyUV(mtx, col->m);
            col->idx = bestk;
            col->edge = beste;
            col->k = best;
        }
    }
end:;
}
