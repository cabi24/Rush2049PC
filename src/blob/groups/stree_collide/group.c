/* The surface tree's tests (MAT3 stree.c): whether a point lies over a surface (PointInSurf), whether a segment
 * passes through one (CylinderInSurf), the nearest point of a surface's edge (VectorToSurfEdge), the leaf of the
 * tree under a point (updownleaf), and a car pushed off a wall (CarApplyCollision). The surfaces, vertices and
 * nodes are the collision WAD's (InitSTree). */
#include "game/game.h"

VIEW(f32[][3], unitmatrix);
#define unitmatrix unitmatrix_view

VIEW(sverttyp *, stree_svert);
#define stree_svert stree_svert_view

VIEW(intptr_t, stree_vlist);
#define stree_vlist stree_vlist_view

/* A surface (MAT3 surftyp, stree_surf[]). */
typedef struct surftyp {
    /* 0x00 */ u16 flags;     /* low nibble: the surface's kind (5 and 6 walls the cars bounce softly off,
                               * PointInSurf's test of q); 0x2000 soft too */
    /* 0x02 */ u16 snd_flags; /* low nibble: its vertices */
    /* 0x04 */ s16 uvs[9];    /* its orientation, 1.0 = 0x4000 (MAT3 s16[3][3]) */
    /* 0x16 */ u16 vli;       /* its vertex list, a byte offset in stree_vlist */
} surftyp;

/* The squared length of `v` in x and z. */
f32 lsq(f32 *v) { return (v[0] * v[0]) + (v[2] * v[2]); }

/* The nearest point to `p` of the edge from the origin to `a`, in x and z: sets `out` to it less p (out[1] 0; before
 * the edge's start, the origin, unitmatrix[3]) and returns 1 if that is shorter than sqrt(`rad2`). */
s32 VectorToSurfEdge(f32 *a, f32 *p, f32 *out, f32 rad2) {
    f32 t;
    t = ((a[2] * p[2]) + (p[0] * a[0])) / lsq(a);
    if (t < 0.0f) {
        out[0] = unitmatrix[3][0] - p[0];
        out[1] = 0.0f;
        out[2] = unitmatrix[3][2] - p[2];
    } else if (t > 1.0f) {
        out[0] = a[0] - p[0];
        out[1] = 0.0f;
        out[2] = a[2] - p[2];
    } else {
        out[0] = (a[0] * t) - p[0];
        out[1] = 0.0f;
        out[2] = (a[2] * t) - p[2];
    }
    if (rad2 <= ((out[0] * out[0]) + (out[2] * out[2]))) {
        return 0;
    }
    return 1;
}
/* Converts a surface's orientation, s16 with 1.0 = 0x4000, `in`, to floats in `out`. */
void mats2fcopy(void *in, void *out) {
    f32 *o = (f32 *) out;
    s16 *p = (s16 *) in;
    o[0] = (f32) p[0] * 0.00006103515625f;
    o[1] = (f32) p[1] * 0.00006103515625f;
    o[2] = (f32) p[2] * 0.00006103515625f;
    o[3] = (f32) p[3] * 0.00006103515625f;
    o[4] = (f32) p[4] * 0.00006103515625f;
    o[5] = (f32) p[5] * 0.00006103515625f;
    o[6] = (f32) p[6] * 0.00006103515625f;
    o[7] = (f32) p[7] * 0.00006103515625f;
    o[8] = (f32) p[8] * 0.00006103515625f;
}


/* A vertex's position in feet (sverttyp: whole feet and 32nds). */

#define DECODE(v, e) \
    (v)[0] = (f32) (((e)->pos[0] << 5) + (((e)->overflow & 0x7C00) >> 10)) * 0.03125f; \
    (v)[1] = (f32) (((e)->pos[1] << 5) + (((e)->overflow & 0x3E0) >> 5)) * 0.03125f; \
    (v)[2] = (f32) (((e)->pos[2] << 5) + ((e)->overflow & 0x1F)) * 0.03125f

/* Is point `wp` over surface `poly`? Turns wp, from the surface's first vertex, into the surface's frame (`pt`;
 * `mat` gets the frame), and misses if it is no higher than `zmin` above the surface or higher than *bound. Then
 * walks the edges (the last, the first, the others): a point outside an edge still counts if within the edge's
 * radius of it (VectorToSurfEdge), and gives -1. A wall (kinds 5 and 6) also needs `q`, when given, above the point
 * and at least point_in_surf_min_height above the surface. Returns 0 a miss, 1 over it, -1 within an edge's radius;
 * *bound gets the point's height, *outIdx what follows the surface's vertex list. */
s16 PointInSurf(surftyp *poly, f32 *wp, f32 *q, f32 *pt, f32 *bound, s16 *outIdx, f32 *mat, f32 zmin) {
    s32 res;
    f32 va[3];
    f32 vb[3];
    f32 vc[3];
    f32 vd[3];
    f32 ve[3];
    s32 i;
    sverttyp *e;
    u16 idx[16];
    s32 k;
    u32 n;
    f32 c;

    n = poly->snd_flags & 0xF;
    res = 1;
    *outIdx = DecodeVlist((u8 *) (stree_vlist + poly->vli), n, (s16 *) idx);
    mats2fcopy(poly->uvs, mat);
    i = idx[0];
    e = i + stree_svert;
    DECODE(vb, e);
    va[0] = wp[0] - vb[0];
    va[1] = wp[1] - vb[1];
    va[2] = wp[2] - vb[2];
    BodyVector(va, pt, mat);
    if ((pt[1] <= zmin) || (*bound < pt[1])) {
        return 0;
    }
    i = idx[n - 1];
    e = i + stree_svert;
    DECODE(vb, e);
    c = ((vb[2] - pt[2]) * vb[0]) - ((vb[0] - pt[0]) * vb[2]);
    if (c < 0) {
        if (VectorToSurfEdge(vb, pt, vc, point_in_surf_edge_radius_last) == 0) {
            return 0;
        }
        res = -1;
        goto done;
    }
    i = idx[1];
    e = i + stree_svert;
    DECODE(vb, e);
    c = (pt[2] * vb[0]) - (pt[0] * vb[2]);
    if (c < 0) {
        if (VectorToSurfEdge(vb, pt, vc, point_in_surf_edge_radius_first) == 0) {
            return 0;
        }
        res = -1;
        goto done;
    }
    for (k = 2; k < n; k++) {
        ve[0] = vb[0];
        ve[2] = vb[2];
        i = idx[k];
        e = i + stree_svert;
        DECODE(vb, e);
        va[0] = vb[0] - ve[0];
        va[1] = 0.0f;
        va[2] = vb[2] - ve[2];
        vd[0] = pt[0] - ve[0];
        vd[1] = 0.0f;
        vd[2] = pt[2] - ve[2];
        c = (vd[2] * va[0]) - (vd[0] * va[2]);
        if (c < 0) {
            if (VectorToSurfEdge(va, vd, vc, point_in_surf_edge_radius) == 0) {
                return 0;
            }
            res = -1;
            goto done;
        }
    }
done:
    *bound = pt[1];
    if ((q != NULL) && (((poly->flags & 0xF) == 5) || ((poly->flags & 0xF) == 6))) {
        i = idx[0];
    e = i + stree_svert;
    DECODE(vb, e);
        va[0] = q[0] - vb[0];
        va[1] = q[1] - vb[1];
        va[2] = q[2] - vb[2];
        BodyVector(va, vb, mat);
        if ((vb[1] <= *bound) || (vb[1] < point_in_surf_min_height)) {
            return 0;
        }
    }
    return res;
}

VIEW(streetyp *, stree_nodes);
#define stree_nodes stree_nodes_view

void *downleaf(streetyp *n, s16 x, s16 y, s16 *out);

/* Finds the leaf of the surface tree under point (x, z) = (`x`, `y`), starting from node `n` (NULL the root): up
 * through the parents until a node holds the point (none: NULL), then down to the leaf (downleaf, which sets *out). */
void *updownleaf(streetyp *n, s16 x, s16 y, s16 *out) {
    s16 outside;
    if (n == 0) {
        n = stree_nodes;
    }
    while (1) {
        outside = (x >= n->maxh || x < n->minh || y >= n->maxv || y < n->minv);
        if (!outside) break;
        if (n->parent == -1) {
            return 0;
        }
        n = &stree_nodes[n->parent];
    }
    return downleaf(n, x, y, out);
}

float fabsf(float);

/* A contact with a wall (MAT3 anon_surf_uvs_30; CollideCarBodyAgainstSurface's): host layout. */
typedef struct CCol {
    surftyp *surf;  /* the wall */
    f32 uvs[9];     /* its frame: uvs[3..5] its normal */
    s16 carEdge;    /* the car's corner (tireIndex) */
    s16 index;
    f32 dist;       /* how far the car is into it */
} CCol;

/* The model record (MAT3 tModelData): the fields this reads and sets. */
typedef struct CCar {
    u8 p0[0x40];
    f32 v[3];               /* 0x040 the velocity in the car's frame */
    f32 w[3];               /* 0x04C the turning speed */
    u8 p88[108];
    f32 bodyForce[4][3];    /* 0x0C4 the forces at the corners */
    f32 bodyR[4][3];        /* 0x0F4 the corners from the centre */
    u8 p292[8];
    f32 centerForce2;       /* 0x12C centerForce[2] */
    u8 p304[252];
    f32 rwr[3];             /* 0x22C the position */
    u8 p568[12];
    f32 tireRwr[4][3];      /* 0x244 */
    f32 bodyrwr[4][3];      /* 0x274 */
    u8 p676[72];
    f32 uvs[9];             /* 0x2EC the orientation */
    u8 p784[688];
    f32 mass;               /* 0x5C0 */
    u8 p1476[72];
    s32 roadcode[4];        /* 0x60C each tyre's road (8 ...) */
    u8 p1564[28];
    f32 idt;                /* 0x638 1 / the step */
    u8 p1596[344];
    f32 reckon_rwr[3];      /* 0x794 the dead-reckoned position */
    u8 p1952[44];
    s8 drone_type;          /* 0x7CC 1 a drone */
    u8 p1997[7];
    u32 appearance;         /* 0x7D4 */
} CCar;

/* Pushes model `car` out of the wall of contact `col`: moves it (and its dead reckoning, corners and tyres) back
 * by its depth along the wall's normal, and adds a force at the corner against its velocity there (the turning
 * included), softer off walls of kinds 5, 6 and 0x2000, and a lift that grows with how square to the wall the
 * car's up is (less for a drone). A corner on road code 8 against a soft wall takes only the sideways force and
 * the push along the car. Marks the car (appearance 0x100000) as having hit. */
void CarApplyCollision(CCar *car, CCol *col) {
    f32 v[3];
    f32 w[3];
    s32 i;
    f32 f12;
    f32 k;
    s32 idx;
    surftyp *poly;

    k = col->dist;
    poly = col->surf;
    idx = tireIndex[col->carEdge];
    v[0] = k * col->uvs[3];
    v[1] = k * col->uvs[4];
    v[2] = col->uvs[5] * k;
    car->rwr[0] = v[0] + car->rwr[0];
    car->rwr[1] = v[1] + car->rwr[1];
    car->rwr[2] = v[2] + car->rwr[2];
    car->reckon_rwr[0] = v[0] + car->reckon_rwr[0];
    car->reckon_rwr[1] = v[1] + car->reckon_rwr[1];
    car->reckon_rwr[2] = v[2] + car->reckon_rwr[2];
    for (i = 0; i < 4; i++) {
        car->bodyrwr[i][0] = v[0] + car->bodyrwr[i][0];
        car->bodyrwr[i][1] = v[1] + car->bodyrwr[i][1];
        car->bodyrwr[i][2] = v[2] + car->bodyrwr[i][2];
        car->tireRwr[i][0] = v[0] + car->tireRwr[i][0];
        car->tireRwr[i][1] = v[1] + car->tireRwr[i][1];
        car->tireRwr[i][2] = v[2] + car->tireRwr[i][2];
    }
    v[0] = car->bodyR[idx][1] * car->w[2] - car->bodyR[idx][2] * car->w[1];
    v[1] = car->bodyR[idx][2] * car->w[0] - car->bodyR[idx][0] * car->w[2];
    v[2] = car->bodyR[idx][0] * car->w[1] - car->bodyR[idx][1] * car->w[0];
    v[0] = v[0] + car->v[0];
    v[1] = v[1] + car->v[1];
    v[2] = v[2] + car->v[2];
    WorldVector(v, w, car->uvs);
    BodyVector(w, v, col->uvs);
    car->appearance |= 0x100000;
    i = poly->flags & 0xF;
    f12 = collision_mass_effect / car->mass;
    if (i == 5 || (poly->flags & 0x2000) || i == 6) {
        k = car->idt * f12 * collision_damp_soft;
    } else {
        k = car->idt * f12 * collision_damp_hard;
    }
    w[0] = -v[0] * k;
    w[2] = -v[2] * k;
    if (car->drone_type == 1) {
        k = fabsf((car->uvs[6] * col->uvs[3] + car->uvs[7] * col->uvs[4]) + car->uvs[8] * col->uvs[5]) * collision_lift_slope_drone + collision_lift_base;
    } else {
        k = fabsf((car->uvs[6] * col->uvs[3] + car->uvs[7] * col->uvs[4]) + car->uvs[8] * col->uvs[5]) * collision_lift_slope + collision_force_limit;
    }
    w[1] = -v[1] * f12 * car->idt * k;
    WorldVector(w, v, col->uvs);
    BodyVector(v, w, car->uvs);
    if ((i == 5 || i == 6) && car->roadcode[idx] == 8 && fabsf(w[2]) < 4000.0f && fabsf(w[1]) < 4000.0f) {
        car->centerForce2 += w[2];
        car->bodyForce[idx][0] += w[0];
    } else {
        car->bodyForce[idx][0] += w[0];
        car->bodyForce[idx][1] += w[1];
        car->bodyForce[idx][2] += w[2];
    }
}

