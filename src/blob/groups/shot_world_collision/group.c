/* CylinderInSurf: a segment through a surface of the track (MAT3 stree.c). */
#include "game/game.h"

/* A vertex's position in feet (sverttyp: whole feet and 32nds). */

/* Does the segment from `p1` down to `p2` pass through surface `poly`? Both ends go into the surface's frame (`mat`
 * gets it): p1 must be on or above it and p2 on or below it; the crossing point then walks the edges as in
 * PointInSurf: outside an edge it is a miss when `flag` is positive, else it counts if within sqrt(`rad2`) of the
 * edge (VectorToSurfEdge, which sets `vcOut`) and gives -1. Sets `out` to the crossing point in the world; *outIdx
 * gets what follows the surface's vertex list. Returns 0, 1 or -1 as PointInSurf. The shots' world collision. */
s16 CylinderInSurf(surftyp *poly, f32 *p1, f32 *p2, f32 *out, f32 *vcOut, f32 *mat, s32 flag, s16 *outIdx, f32 rad2) {
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
    sverttyp *e;
    f32 vd[3];

    res = 1;
    mats2fcopy(poly->uvs, mat);
    n = poly->snd_flags & 0xF;
    *outIdx = DecodeVlist(stree_vlist + poly->vli, n, (s16 *) idx);
    i = idx[0];
    e = i + stree_svert;
    DECODE_SVERT(v0, e);
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
    DECODE_SVERT(ve, e);
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
    DECODE_SVERT(ve, e);
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
        DECODE_SVERT(ve, e);
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
