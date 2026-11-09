#include "game/game.h"

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

/* context: strict single match, unchanged */

/* compressed vertex: three s16 with 5 extra fraction bits each packed in w */

#define DECODE(v, e) \
    (v)[0] = (f32) (((e)->x << 5) + (((e)->w & 0x7C00) >> 10)) * 0.03125f; \
    (v)[1] = (f32) (((e)->y << 5) + (((e)->w & 0x3E0) >> 5)) * 0.03125f; \
    (v)[2] = (f32) (((e)->z << 5) + ((e)->w & 0x1F)) * 0.03125f

/* CylinderInSurf (0x800AD734, 363 words).
 * Does the segment p1->p2 cross convex polygon `poly` from above?  Both ends go into the polygon frame
 * (vp, vq); p1 must be above (vp.y >= 0) and p2 below (vq.y <= 0); vp is moved to the plane crossing, then
 * the same edge walk as PointInSurf (with `flag > 0` turning an outside edge into a miss).  On a hit the
 * crossing point is transformed back (WorldVector) and written to out.
 * Quirks: as PointInSurf, plus `out[n] = out[n] + v0[n]` (neither `+=` nor `v0 + out` gives the load
 * order), and the declaration order below (k, n above idx[16]; t, c, i, e between vprev and vd) is the
 * retail frame.  Parameter order is the real one: argument registers are s1,s2,s3,a3,s5,s6,s7,s8,f24.
 * Real caller ShotToWorldCollision is unmatched -> two stand-in call sites -> PROVISIONAL.
 */
s16 CylinderInSurf(Poly *poly, f32 *p1, f32 *p2, f32 *out, f32 *vcOut, f32 *mat, s32 flag, s16 *outIdx, f32 rad2) {
    s32 k;
    u32 n;
    u16 idx[16];
    s32 res;
    f32 vp[3];
    f32 vq[3];
    f32 va[3];
    f32 ve[3];
    f32 v0[3];
    f32 vprev[3];
    f32 t;
    f32 c;
    s32 i;
    PV *e;
    f32 vd[3];

    res = 1;
    mats2fcopy(poly->body, mat);
    n = poly->cnt & 0xF;
    *outIdx = DecodeVlist((u8 *) (stree_vlist + poly->off), n, (s16 *) idx);
    i = idx[0];
    e = i + stree_svert;
    DECODE(v0, e);
    va[0] = p1[0] - v0[0];
    va[1] = p1[1] - v0[1];
    va[2] = p1[2] - v0[2];
    BodyVector(va, vp, mat);
    if (vp[1] < 0.0f) {
        return 0;
    }
    va[0] = p2[0] - v0[0];
    va[1] = p2[1] - v0[1];
    va[2] = p2[2] - v0[2];
    BodyVector(va, vq, mat);
    if (vq[1] > 0.0f) {
        return 0;
    }
    va[0] = vq[0] - vp[0];
    va[1] = vq[1] - vp[1];
    va[2] = vq[2] - vp[2];
    if (va[1] > 0.0f) {
        return 0;
    }
    if (va[1] < 0.0f) {
        t = vp[1] / va[1];
        vp[0] -= va[0] * t;
        vp[1] -= va[1] * t;
        vp[2] -= va[2] * t;
    }
    i = idx[n - 1];
    e = i + stree_svert;
    DECODE(ve, e);
    c = ((ve[2] - vp[2]) * ve[0]) - ((ve[0] - vp[0]) * ve[2]);
    if (c < 0) {
        if (flag > 0) {
            return 0;
        }
        if (VectorToSurfEdge(ve, vp, vcOut, rad2) == 0) {
            return 0;
        }
        res = -1;
        goto done;
    }
    i = idx[1];
    e = i + stree_svert;
    DECODE(ve, e);
    c = (vp[2] * ve[0]) - (vp[0] * ve[2]);
    if (c < 0) {
        if (flag > 0) {
            return 0;
        }
        if (VectorToSurfEdge(ve, vp, vcOut, rad2) == 0) {
            return 0;
        }
        res = -1;
        goto done;
    }
    for (k = 2; k < n; k++) {
        vprev[0] = ve[0];
        vprev[2] = ve[2];
        i = idx[k];
        e = i + stree_svert;
        DECODE(ve, e);
        va[0] = ve[0] - vprev[0];
        va[1] = 0.0f;
        va[2] = ve[2] - vprev[2];
        vd[0] = vp[0] - vprev[0];
        vd[1] = 0.0f;
        vd[2] = vp[2] - vprev[2];
        c = (vd[2] * va[0]) - (vd[0] * va[2]);
        if (c < 0) {
            if (flag > 0) {
                return 0;
            }
            if (VectorToSurfEdge(va, vd, vcOut, rad2) == 0) {
                return 0;
            }
            res = -1;
            goto done;
        }
    }
done:
    WorldVector(vp, out, mat);
    out[0] = out[0] + v0[0];
    out[1] = out[1] + v0[1];
    out[2] = out[2] + v0[2];
    return res;
}

float fabsf(float);
#pragma intrinsic (fabsf)

