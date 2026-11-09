/* The road path's surfaces (MAT3 stree.c): the frame of a point on a surface built along the road path (flag
 * 0x1000, fPathInterp) and on a half-pipe's rounded edge (0x2000, fHalfPipeInterp). */
#include "game/game.h"

/* A point of the road path (MAT3 pathtyp, gRoadPath[gRoadHead->pnum], the collision WAD's): a cross-section of the
 * road. The names say what the code does with each; MAT3's are in the comments. */
typedef struct pathtyp {
    /* 0x00 */ f32 pos[3];      /* pos: its centre */
    /* 0x0C */ f32 uvs[3][3];   /* uvs: its frame */
    /* 0x30 */ f32 right[3];    /* ve: across the road */
    /* 0x3C */ f32 fwdL[3];     /* vl: along the road at its left edge */
    /* 0x48 */ f32 fwdR[3];     /* vr: at its right edge */
    /* 0x54 */ f32 halfWidth;   /* width */
    /* 0x58 */ f32 len0;        /* w */
    /* 0x5C */ f32 skew0;       /* m0 */
    /* 0x60 */ f32 skew1;       /* m1 */
    /* 0x64 */ f32 wL0;         /* wl */
    /* 0x68 */ f32 wL1;         /* ml */
    /* 0x6C */ f32 wR0;         /* wr */
    /* 0x70 */ f32 wR1;         /* mr */
    /* 0x74 */ f32 hL;          /* m0l: the height curve's slope, left */
    /* 0x78 */ f32 hR;          /* m0r: right */
    /* 0x7C */ f32 pad7C[2];    /* m1l, m1r */
} PathRec; /* 0x84 */

f32 fabsf(f32);

/* Sets `out` to `position` from `origin` in the frame `basis`. */
void surfpos(f32 *origin, f32 *basis, f32 *position, f32 *out) {
    f32 delta[3];
    delta[0] = position[0] - origin[0];
    delta[1] = position[1] - origin[1];
    delta[2] = position[2] - origin[2];
    BodyVector(delta, out, basis);
}
f32 sqrtf(f32);
/* Places `position` on the half-pipe edge of road path point `idx`'s stretch (to the next point, round to 0):
 * `outMatrix` the stretch's frame, the position in it lerped along the stretch; on the flat part `outPosition[1]` is
 * its height above the surface, on a rounded corner its height above the curve, and under `threshold` the frame
 * turns to lie on the curve (fpitch, froll). An upside-down frame is turned over first. `unused` is the surface. */
void fHalfPipeInterp(s32 unused, u16 idx, f32 *position, f32 *outPosition, f32 (*outMatrix)[3], f32 threshold) {
    PathRec *r0;
    PathRec *r1;
    f32 height;
    f32 frac;
    f32 dist;
    f32 width;
    f32 x;
    f32 y;
    f32 z;
    f32 len;
    f32 rel[3];

    r0 = &gRoadPath[idx];
    if (idx + 1 < gRoadHead->pnum) {
        r1 = r0 + 1;
    } else {
        r1 = gRoadPath;
    }
    CopyUV(r0->uvs, outMatrix);
    surfpos(r0->pos, r0->uvs[0], position, rel);
    frac = r0->skew1;
    z = rel[2];
    if (rel[2] < 0.0f) {
        z = 0;
    } else if (frac < rel[2]) {
        z = frac;
    }
    frac = z / frac;
    width = r0->len0 + (r1->len0 - r0->len0) * frac;
    height = r0->halfWidth + (r1->halfWidth - r0->halfWidth) * frac;
    height -= width;
    height -= 2.5f;
    x = rel[0];
    x += r0->skew0;
    y = rel[1];
    if (outMatrix[1][1] < 0.0f) {
        froll(0.0f, -1.0f, outMatrix);
        x = -x;
        y = height - y + 5.0f;
    } else {
        y = rel[1] - height;
    }
    if (x < -width) {
        x += width;
    } else if (width < x) {
        x -= width;
    } else {
        if (0.0f < y) {
            outPosition[1] = height - y;
            froll(0.0f, -1.0f, outMatrix);
        } else {
            outPosition[1] = height + y;
        }
        return;
    }
    dist = sqrtf(x * x + y * y);
    if (dist < 0.01f) {
        outPosition[1] += height;
        return;
    }
    outPosition[1] = height - dist;
    if (outPosition[1] < threshold) {
        width = r1->len0 - r0->len0;
        height = r1->halfWidth - r1->len0 - r0->halfWidth + r0->len0;
        if (fabsf(height) > 1.0f) {
            z = sqrtf(height * height + r0->skew1 * r0->skew1);
            fpitch(-height / z, r0->skew1 / z, outMatrix);
        }
        froll(-x / dist, -y / dist, outMatrix);
        if (fabsf(width) > 1.0f) {
            width = fabsf(x) * width / dist;
            len = sqrtf(width * width + r0->skew1 * r0->skew1);
            fpitch(-width / len, r0->skew1 / len, outMatrix);
        }
        if (fabsf(height) > 1.0f) {
            fpitch(height / z, r0->skew1 / z, outMatrix);
        }
    }
}

/* Places `pos` on the road surface of road path point `idx`'s stretch: `outMat` the frame there, from the left and
 * right edges' directions and the cross direction lerped across and along, and `outPos` the position in it from
 * the surface, whose height follows a cubic between the two points' slopes. `unused` is the surface. */
void fPathInterp(s32 unused, u16 idx, f32 *pos, f32 *outPos, f32 (*outMat)[3]) {
    s32 next;
    PathRec *r0;
    PathRec *r1;
    f32 u;
    f32 v;
    s32 i;
    f32 fwd0[3];
    f32 fwd1[3];
    f32 right[3];
    f32 up[3];
    f32 fwd[3];
    f32 p0[3];
    f32 p1[3];
    f32 org[3];
    f32 h0;
    f32 h1;
    f32 h;
    f32 len;
    f32 uu;
    f32 off;
    f32 rel[3];

    next = idx + 1 < gRoadHead->pnum ? idx + 1 : 0;
    r0 = &gRoadPath[idx];
    r1 = &gRoadPath[next];
    surfpos(r0->pos, r0->uvs[0], pos, rel);
    len = rel[0] * (r0->skew1 - r0->skew0) + r0->len0;
    u = (rel[2] - rel[0] * r0->skew0) / len;
    if (rel[0] > 0.0f) {
        v = rel[0] / (rel[2] * r0->wR1 + r0->wR0) * 0.5f + 0.5f;
    } else {
        v = rel[0] / (rel[2] * r0->wL1 + r0->wL0) * -0.5f + 0.5f;
    }
    FVecInterp(r0->fwdL, r0->fwdR, v, fwd0);
    FVecInterp(r1->fwdL, r1->fwdR, v, fwd1);
    FVecInterp(fwd0, fwd1, u, fwd);
    FVecInterp(r0->right, r1->right, u, right);
    up[0] = fwd[1] * right[2] - fwd[2] * right[1];
    up[1] = fwd[2] * right[0] - fwd[0] * right[2];
    up[2] = fwd[0] * right[1] - fwd[1] * right[0];
    for (i = 0; i < 3; i++) {
        outMat[0][i] = right[i];
        outMat[1][i] = up[i];
        outMat[2][i] = fwd[i];
    }
    for (i = 0; i < 3; i++) {
        p0[i] = r0->pos[i] + r0->right[i] * ((v - 0.5f) * r0->halfWidth);
    }
    for (i = 0; i < 3; i++) {
        p1[i] = r1->pos[i] + r1->right[i] * ((v - 0.5f) * r1->halfWidth);
    }
    FVecInterp(p0, p1, u, org);
    h0 = r0->hL + (r0->hR - r0->hL) * v;
    h1 = -(r1->hL + (r1->hR - r1->hL) * v);
    uu = u * u;
    off = (u * uu * (h0 + h1) - uu * (h0 + h0 + h1) + u * h0) * len;
    for (i = 0; i < 3; i++) {
        org[i] -= off * outMat[1][i];
    }
    surfpos(org, outMat[0], pos, outPos);
}
