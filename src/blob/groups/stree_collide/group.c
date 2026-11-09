#include "game/game.h"

VIEW(f32[][3], unitmatrix);
#define unitmatrix unitmatrix_view

typedef struct PV {
    s16 x;
    s16 y;
    s16 z;
    u16 w;
} PV;
VIEW(PV *, stree_svert);
#define stree_svert stree_svert_view

VIEW(intptr_t, stree_vlist);
#define stree_vlist stree_vlist_view

/* ---- unit: VectorToSurfEdge + its two callers (agent) ----
 * Collision helpers for the track-surface polygon tests.  All three are internal (-O3 whole-program):
 *   VectorToSurfEdge            closest point on a 2D (x,z) segment; radius-squared arrives in $f18
 *   PointInSurf            point-above-convex-polygon test (IPA params s0,s2,s4,s5,s6,s7,s8,f20)
 *   CylinderInSurf segment-through-convex-polygon test (IPA params s1,s2,s3,a3,s5,s6,s7,s8,f24)
 * (names are historical labels; nothing here touches controllers).
 * mats2fcopy / DecodeVlist are context (already matched elsewhere), reused; note the parameter order
 * of mats2fcopy below.
 */
typedef struct Poly {
    /* 0x00 */ u16 type;      /* low nibble: surface type (5 and 6 get the extra `q` test) */
    /* 0x02 */ u16 cnt;       /* low nibble: vertex count */
    /* 0x04 */ s16 body[9];   /* 3x3 rotation, s16 * 2^-14 */
    /* 0x16 */ u16 off;       /* offset of the packed vertex-index list in stree_vlist */
} Poly;

/* VectorToSurfEdge (0x800AD4C8, 66 words).
 * Closest point on the segment origin->a to p, in the x/z plane.  t = dot(a,p)/|a|^2 clamped to [0,1];
 * writes (closest - p) to out (out.y = 0) and returns 1 when its squared length is below rad2.
 * For t < 0 the "closest" point is unitmatrix[3] (a zero vector in the image).
 * Quirks: the squared length is a tiny static function that umerge inlines (that is what puts the
 * denominator in $f0 and evaluates it before the numerator); operand order inside it matters.
 * rad2 is an IPA register parameter ($f18): the body only reproduces with its real callers in the unit.
 */
f32 lsq(f32 *v) { return (v[0] * v[0]) + (v[2] * v[2]); }
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
/* context: strict single match, unchanged */

/* parameter order (out): callers set up a1 (out) before a0 -- proven by RotateCollisionObject. */
void mats2fcopy(void *arg0, void *arg1) {
    f32 *o = (f32 *) arg1;
    s16 *p = (s16 *) arg0;
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


/* compressed vertex: three s16 with 5 extra fraction bits each packed in w */

#define DECODE(v, e) \
    (v)[0] = (f32) (((e)->x << 5) + (((e)->w & 0x7C00) >> 10)) * 0.03125f; \
    (v)[1] = (f32) (((e)->y << 5) + (((e)->w & 0x3E0) >> 5)) * 0.03125f; \
    (v)[2] = (f32) (((e)->z << 5) + ((e)->w & 0x1F)) * 0.03125f

/* PointInSurf (0x800C3AD0, 362 words).
 * Is point wp above convex polygon `poly`?  Transforms (wp - v0) into the polygon frame (pt), rejects on
 * height (pt.y <= zmin or pt.y > *bound), then walks the edges; when the point is outside an edge it must
 * still be within a per-edge radius (VectorToSurfEdge) and the result becomes -1.  Types 5/6 also test q.
 * Returns 0 (miss), 1 (inside) or -1 (within the edge radius); *bound receives pt.y; *outIdx the list tail.
 * Quirks (all needed for the bytes):
 *  - both the vertex index `i` and the vertex pointer `e` are named locals (index lives in $a3; naming the
 *    pointer removes one uopt temp word from the frame);
 *  - the cross product goes through a named local `c` and is compared with INT 0 (a separate zero
 *    register from the 0.0f stores; unnamed, IDO folds `a - b < 0` into `a < b`);
 *  - `i + stree_svert` (not &stree_svert[i]) and `stree_vlist + poly->off` give the addu operand order;
 *  - `s32 k` against `u32 n` keeps `sltu; bnez` instead of `bne`;
 *  - operand order of every product in the three cross tests is as written here (found by search);
 *  - idx[16] (count is 4 bits) with `i, e` declared just above it and `k, n, c` below reproduces the frame.
 * IPA: all eight parameters arrive in registers chosen by the callee (s0,s2,s4,s5,s6,s7,s8,f20); its real
 * callers (RoadPosition, CollideCarBody, TireSurf) are unmatched, so two stand-in call sites
 * keep it out of line here -> PROVISIONAL.
 */
s16 PointInSurf(Poly *poly, f32 *wp, f32 *q, f32 *pt, f32 *bound, s16 *outIdx, f32 *mat, f32 zmin) {
    s32 res;
    f32 va[3];
    f32 vb[3];
    f32 vc[3];
    f32 vd[3];
    f32 ve[3];
    s32 i;
    PV *e;
    u16 idx[16];
    s32 k;
    u32 n;
    f32 c;

    n = poly->cnt & 0xF;
    res = 1;
    *outIdx = DecodeVlist((u8 *) (stree_vlist + poly->off), n, (s16 *) idx);
    mats2fcopy(poly->body, mat);
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
    if ((q != NULL) && (((poly->type & 0xF) == 5) || ((poly->type & 0xF) == 6))) {
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

/* stand-in caller: keeps VectorToSurfEdge out of line under -O3 */

/* stand-in callers for CylinderInSurf, whose real caller (ShotToWorldCollision) is outside
 * the unit: two call sites keep it out of line without `keep`, so it gets IPA registers like the ROM's.
 * (the former PointInSurf stand-ins are removed -- its real callers RoadPosition and
 * CollideCarBody are members of this group, and TireSurf is locked in TireSurf.) */

/* quadtree lookup (updownleaf is the ROM name, it is a point-in-region search) */
typedef struct QNode {
    s16 next;
    u8 pad2;
    u8 mask;
    s16 x0;
    s16 x1;
    s16 y0;
    s16 y1;
    u16 child[4];
} QNode;
VIEW(QNode *, stree_nodes);
#define stree_nodes stree_nodes_view


void *downleaf(QNode *n, s16 x, s16 y, s16 *out);
void *updownleaf(QNode *n, s16 x, s16 y, s16 *out) {
    s16 outside;
    if (n == 0) {
        n = stree_nodes;
    }
    while (1) {
        outside = (x >= n->x1 || x < n->x0 || y >= n->y1 || y < n->y0);
        if (!outside) break;
        if (n->next == -1) {
            return 0;
        }
        n = &stree_nodes[n->next];
    }
    return downleaf(n, x, y, out);
}

float fabsf(float);
#pragma intrinsic (fabsf)

typedef struct CPoly { u16 type; } CPoly;
typedef struct CCol {
    CPoly *poly;
    f32 m[9];
    s16 idx;
    s16 pad42;
    f32 k;
} CCol;
typedef struct CCar {
    u8 p0[64];
    f32 v64[3];
    f32 v76[3];
    u8 p88[108];
    f32 A196[4][3];
    f32 A244[4][3];
    u8 p292[8];
    f32 f300;
    u8 p304[252];
    f32 f556[3];
    u8 p568[12];
    f32 B580[4][3];
    f32 B628[4][3];
    u8 p676[72];
    f32 mat[9];
    u8 p784[688];
    f32 f1472;
    u8 p1476[72];
    s32 w1548[4];
    u8 p1564[28];
    f32 f1592;
    u8 p1596[344];
    f32 f1940[3];
    u8 p1952[44];
    s8 b1996;
    u8 p1997[7];
    u32 flags2004;
} CCar;

void CarApplyCollision(CCar *car, CCol *col) {
    f32 v[3];
    f32 w[3];
    s32 i;
    f32 f12;
    f32 k;
    s32 idx;
    CPoly *poly;

    k = col->k;
    poly = col->poly;
    idx = tireIndex[col->idx];
    v[0] = k * col->m[3];
    v[1] = k * col->m[4];
    v[2] = col->m[5] * k;
    car->f556[0] = v[0] + car->f556[0];
    car->f556[1] = v[1] + car->f556[1];
    car->f556[2] = v[2] + car->f556[2];
    car->f1940[0] = v[0] + car->f1940[0];
    car->f1940[1] = v[1] + car->f1940[1];
    car->f1940[2] = v[2] + car->f1940[2];
    for (i = 0; i < 4; i++) {
        car->B628[i][0] = v[0] + car->B628[i][0];
        car->B628[i][1] = v[1] + car->B628[i][1];
        car->B628[i][2] = v[2] + car->B628[i][2];
        car->B580[i][0] = v[0] + car->B580[i][0];
        car->B580[i][1] = v[1] + car->B580[i][1];
        car->B580[i][2] = v[2] + car->B580[i][2];
    }
    v[0] = car->A244[idx][1] * car->v76[2] - car->A244[idx][2] * car->v76[1];
    v[1] = car->A244[idx][2] * car->v76[0] - car->A244[idx][0] * car->v76[2];
    v[2] = car->A244[idx][0] * car->v76[1] - car->A244[idx][1] * car->v76[0];
    v[0] = v[0] + car->v64[0];
    v[1] = v[1] + car->v64[1];
    v[2] = v[2] + car->v64[2];
    WorldVector(v, w, car->mat);
    BodyVector(w, v, col->m);
    car->flags2004 |= 0x100000;
    i = poly->type & 0xF;
    f12 = collision_mass_effect / car->f1472;
    if (i == 5 || (poly->type & 0x2000) || i == 6) {
        k = car->f1592 * f12 * collision_damp_soft;
    } else {
        k = car->f1592 * f12 * collision_damp_hard;
    }
    w[0] = -v[0] * k;
    w[2] = -v[2] * k;
    if (car->b1996 == 1) {
        k = fabsf((car->mat[6] * col->m[3] + car->mat[7] * col->m[4]) + car->mat[8] * col->m[5]) * collision_lift_slope_drone + collision_lift_base;
    } else {
        k = fabsf((car->mat[6] * col->m[3] + car->mat[7] * col->m[4]) + car->mat[8] * col->m[5]) * collision_lift_slope + collision_force_limit;
    }
    w[1] = -v[1] * f12 * car->f1592 * k;
    WorldVector(w, v, col->m);
    BodyVector(v, w, car->mat);
    if ((i == 5 || i == 6) && car->w1548[idx] == 8 && fabsf(w[2]) < 4000.0f && fabsf(w[1]) < 4000.0f) {
        car->f300 += w[2];
        car->A196[idx][0] += w[0];
    } else {
        car->A196[idx][0] += w[0];
        car->A196[idx][1] += w[1];
        car->A196[idx][2] += w[2];
    }
}

