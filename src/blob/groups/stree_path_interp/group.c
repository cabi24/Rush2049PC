#include "game/game.h"

typedef struct {
    /* 0x00 */ f32 pos[3];
    /* 0x0C */ f32 uvs[3][3];
    /* 0x30 */ f32 right[3];
    /* 0x3C */ f32 fwdL[3];
    /* 0x48 */ f32 fwdR[3];
    /* 0x54 */ f32 halfWidth;
    /* 0x58 */ f32 len0;
    /* 0x5C */ f32 skew0;
    /* 0x60 */ f32 skew1;
    /* 0x64 */ f32 wL0;
    /* 0x68 */ f32 wL1;
    /* 0x6C */ f32 wR0;
    /* 0x70 */ f32 wR1;
    /* 0x74 */ f32 hL;
    /* 0x78 */ f32 hR;
    /* 0x7C */ f32 pad7C[2];
} PathRec; /* 0x84 */
VIEW(PathRec *, gRoadPath);
#define gRoadPath gRoadPath_view

VIEW(u16 *, gRoadHead);
#define gRoadHead gRoadHead_view

f32 fabsf(f32);
void surfpos(f32 *origin, f32 *basis, f32 *position, f32 *out) {
    f32 delta[3];
    delta[0] = position[0] - origin[0];
    delta[1] = position[1] - origin[1];
    delta[2] = position[2] - origin[2];
    BodyVector(delta, out, basis);
}
f32 sqrtf(f32);
#pragma intrinsic (sqrtf)
#pragma intrinsic (fabsf)
/*
 * : fHalfPipeInterp (0x800AD128, 232 words) -- strict MATCH in this
 * real -O3 group (no stand-ins; the four context bodies are the locked
 * fPathInterp sources, unchanged).
 *
 * fHalfPipeInterp(unused, idx, position, outPosition, outMatrix,
 * threshold) is a historical label; no arcade ancestor found (N64-only path
 * code, like fPathInterp).  It places a world position in the cross
 * section of path segment idx (next record wraps to 0):
 *   outMatrix = the segment's basis (CopyUV); rel = position in that
 *   frame (surfpos, register-parameter internal callee);
 *   the forward coordinate rel[2] is clamped to [0, skew1] and gives the
 *   fraction along the segment; width (+0x58) and halfWidth (+0x54) are
 *   lerped between the two records; height = halfWidth - width - 2.5.
 *   If the basis is upside down (outMatrix[1][1] < 0) it is turned over
 *   (froll(0, -1)) and x/y are mirrored (y measured from the other
 *   side, + 5.0).  Inside the flat part (|x| <= width) outPosition[1] is the
 *   height above/below and returns; otherwise the point is in a rounded
 *   corner of radius `height`: dist from the corner centre, outPosition[1] =
 *   height - dist, and below `threshold` the basis is rotated onto the
 *   curved surface (fpitch / froll with sine/cosine pairs).
 *
 * Shaping that the retail bytes require (each one moved the colouring, found
 * with the traced uopt):
 *   - one variable `frac` first holds r0->skew1 and then z / skew1 (gives the
 *     shared $f0 web);
 *   - `z = 0;` int literal (a second zero web: two mtc1 zero);
 *   - `height -= width; height -= 2.5f;` as separate statements (height in
 *     $f20 and the final subtract in the bc1f delay slot);
 *   - `x = rel[0]; x += r0->skew0;` after the lerps (x before width in web
 *     order, loads after the lerp loads);
 *   - `y = rel[1];` is dead on the else path, which re-reads rel[1].
 *   0.01f is the function's own .rodata word at 0x80123BFC (verified).
 */
void fHalfPipeInterp(s32 arg0, u16 idx, f32 *position, f32 *outPosition, f32 (*outMatrix)[3], f32 threshold) {
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
    if (idx + 1 < *gRoadHead) {
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

void fPathInterp(s32 arg0, u16 idx, f32 *pos, f32 *outPos, f32 (*outMat)[3]) {
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

    next = idx + 1 < *gRoadHead ? idx + 1 : 0;
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
