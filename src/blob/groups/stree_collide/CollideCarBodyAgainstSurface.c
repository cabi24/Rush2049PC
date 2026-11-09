/* CollideCarBodyAgainstSurface: a car's body against one wall (MAT3 stree.c). */
#include "game/game.h"

float fabsf(float);

/* A contact with a wall (MAT3 anon_surf_uvs_30). */
typedef struct CCol {
    surftyp *surf;  /* the wall, NULL none yet */
    f32 uvs[9];     /* its frame */
    s16 carEdge;    /* the car's corner (tireIndex) */
    s16 index;      /* the wall's edge */
    f32 dist;       /* how far the car is into it */
} CCol;

/* The model record (MAT3 tModelData): the fields this reads and sets. */
typedef struct CCar {
    _tCar *car;
    Tune44 *tune;
    u8 p8[0x26C];
    f32 bodyrwr[4][3];      /* 0x274 the corners' positions */
    f32 bodyrwr2[4][3];     /* 0x2A4 the same last step */
    u8 p724[24];
    f32 uvs[9];             /* 0x2EC the orientation */
    u8 p784[144];
    streetyp *lasttp[4];
    u8 p944[92];
    const s16 *torquecurve;
    u8 p1040[560];
    s8 crashflag;           /* 0x640 */
    u8 p1601[7];
    surftyp *movingSurface;
    u8 p1612[120];
    s16 moving_state;       /* 0x6C4 resurrect.moving_state: -1 unless being put back */
    u8 p1734[7];
    s8 waterAbort;          /* 0x6CD resurrect.waterAbort */
    u8 p1742[248];
    s16 slot;               /* 0x7C6 */
} CCar;

/* Tests model `car`'s body (its corners, BodyVerts) against wall `poly`: finds how deep the car's outline, flattened
 * into the wall's plane, is into the wall's outline and on which edge; keeps the deepest contact of the step in
 * `col` (a wall facing the car's up more than 45 degrees away is no wall). A wall's kind can end the car, unless it
 * is out, crashed or being put back already: kind 4 crashes it (a battle point lost), kind 7 (water) and kind 3
 * abort it (kind 3 with sound 21 for a car driven here). */
void CollideCarBodyAgainstSurface(CCar *car, surftyp *poly, CCol *col) {
    u32 n;
    s32 type;
    u16 idx[16];
    sverttyp *v;
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
    surftyp *old;

    conv = 0;
    n = poly->snd_flags;
    n &= 0xF;
    DecodeVlist(poly->vli + stree_vlist, n, (s16 *) idx);
    i = idx[0];
    v = &stree_svert[i];
    DECODE_SVERT(org, v);
    mats2fcopy(poly->uvs, mtx);
    for (ka = 0; ka < 4; ka++) {
        d[0] = car->bodyrwr[tireIndex[ka]][0] - org[0];
        d[1] = car->bodyrwr[tireIndex[ka]][1] - org[1];
        d[2] = car->bodyrwr[tireIndex[ka]][2] - org[2];
        BodyVector(d, pts[ka], mtx);
    }
    type = poly->flags & 0xF;
    if (type == 5 || type == 6) {
        dz = -3.0f;
    } else if (mtx[3] * car->uvs[3] + mtx[4] * car->uvs[4] + mtx[5] * car->uvs[5] < -0.707f) {
        dz = -4.5f;
    } else {
        dz = -0.5f;
    }
    BodyVector(&car->uvs[3], d, mtx);
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
                    pv[k][0] = (f32) ((v->pos[0] << 5) + ((v->overflow & 0x7C00) >> 10)) * 0.03125f;
                    pv[k][1] = (f32) ((v->pos[2] << 5) + (v->overflow & 0x1F)) * 0.03125f;
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
                        type = poly->flags & 0xF;
                        if (u != 0.0f) {
                            u = (pz * hx - px * hz) / u;
                            if (u > 0.0f && u < t) {
                                t = u;
                            }
                        }
                        if (type == 4) {
                            if (PC_CAR_DEAD(car->slot) == 0 && PC_CAR_CRASH_STATE(car->slot) == 0 && car->moving_state == -1) {
                                AddBattlePoint(car->slot, car->slot, -1);
                                car->crashflag = 1;
                            }
                        } else if (type == 7) {
                            if (PC_CAR_DEAD(car->slot) == 0 && PC_CAR_CRASH_STATE(car->slot) == 0 && car->moving_state == -1) {
                                car->waterAbort = 1;
                                AbortCar(car->slot, 1);
                            }
                        } else if (type == 3) {
                            if (PC_CAR_DEAD(car->slot) == 0 && PC_CAR_CRASH_STATE(car->slot) == 0 && car->moving_state == -1) {
                                car->waterAbort = 1;
                                AbortCar(car->slot, 1);
                                if (PC_LINK_OWNER(car->slot) == 6) {
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
                            org[0] = car->bodyrwr[tireIndex[k]][0] - car->bodyrwr2[tireIndex[k]][0];
                            org[1] = car->bodyrwr[tireIndex[k]][1] - car->bodyrwr2[tireIndex[k]][1];
                            org[2] = car->bodyrwr[tireIndex[k]][2] - car->bodyrwr2[tireIndex[k]][2];
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
        if (col->surf != NULL) {
            if (col->dist < best) {
                CarApplyCollision(car, col);
                col->surf = NULL;
                CollideCarBodyAgainstSurface(car, poly, col);
                return;
            } else {
                old = col->surf;
                col->surf = poly;
                CopyUV(mtx, col->uvs);
                col->carEdge = bestk;
                col->index = beste;
                col->dist = best;
                CarApplyCollision(car, col);
                col->surf = NULL;
                CollideCarBodyAgainstSurface(car, old, col);
                return;
            }
        } else {
            col->surf = poly;
            CopyUV(mtx, col->uvs);
            col->carEdge = bestk;
            col->index = beste;
            col->dist = best;
        }
    }
end:;
}
