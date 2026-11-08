typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
#define NULL ((void *)0)
float fabsf(float);
#pragma intrinsic (fabsf)

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
extern QNode *stree_nodes;

typedef struct Poly {
    u16 type;
    u16 cnt;
    u8 body[0x12];
    u16 off;
} Poly;
extern Poly *stree_surf;
extern u8 *stree_slist;

void *updownleaf(QNode *n, s16 x, s16 y, s16 *out);
void DecodeSlist(u8 *input, s32 count, u16 *front, u32 marker);
s16 PointInSurf(Poly *poly, f32 *wp, f32 *q, f32 *pt, f32 *bound, s16 *outIdx, f32 *mat, f32 zmin);
void CopyUV(void *, void *);
void fHalfPipeInterp(s32 arg0, u16 idx, f32 *position, f32 *outPosition, f32 (*outMatrix)[3], f32 threshold);
void fPathInterp(s32 arg0, u16 idx, f32 *pos, f32 *outPos, f32 (*outMat)[3]);
f32 NormalVector(f32 *v);

typedef struct EUCar {
    u8 pad0[928];
    QNode *node[4];
    u8 pad944[1440 - 944];
    u16 hint[4];
    u8 pad1448[1564 - 1448];
    s16 kind[4];
    s16 slope[4];
    u16 attr[4];
    u8 pad1588[1600 - 1588];
    u8 b1600;
    u8 pad1601[7];
    Poly *p1608;
    u8 pad1612[1732 - 1612];
    s16 s1732;
    u8 pad1734[1741 - 1734];
    u8 b1741;
    u8 pad1742[1990 - 1742];
    s16 id;
} EUCar;
typedef struct EUSlot {
    u8 pad0[856];
    s8 b856;
    s8 b857;
    u8 pad858[952 - 858];
} EUSlot;
extern EUSlot gGameCar[];
extern s8 gGameSoundsEnabled;
extern f32 unitmatrix[];
void AddBattlePoint(s8 a, s8 b, s8 c);
void AbortCar(s16 arg0, s32 arg1);
u32 AudPlayEffect(u32 index, u32 other, u32 value, u8 mode);
s16 CylinderInSurf(f32 *p1, f32 *p2, f32 *out, Poly *poly, s16 *outIdx, s32 flag, f32 *vcOut, f32 *mat, f32 rad2);

/*
 * RoadPosition (0x800C4200, 308 words) -- , .
 * Historical label; it is a "find the road surface under a point" query (N64-only, no arcade
 * ancestor found):
 *   floor(pos.x), floor(pos.z) -> quadtree cell (updownleaf on stree_nodes, quadrant in `quad`);
 *   the cell's packed polygon list (stree_slist + child offset, bit 0x10000 from the node mask) is
 *   expanded by DecodeSlist into list[]; every polygon that is not type 5/6/15 is tested with
 *   PointInSurf (point-above-polygon, zmin -20, bound = best so far); the polygon with the
 *   smallest |height| wins (out = hit point, mat = its basis via CopyUV).
 *   Winner flags 0x2000 -> fHalfPipeInterp (rounded edge; fails below -5),
 *   0x1000 -> fPathInterp + NormalVector on the three basis rows.  out = pos - up * h.
 *   On a miss the probe is nudged (+x, then -x+z, then -2z) and retried; returns the polygon or 0.
 * Shaping (each measured in the whole-program unit):
 *   - u8 n (declared right after quad, it fills the s16 pad) and u32 i with `i < n`: n lives in
 *     s1 across DecodeSlist, the entry test is beqz, and n is spilled to a temp at loop entry;
 *     with s32/u32 n it is spilled before the call (one extra word);
 *   - `n = *p++; DecodeSlist(p, n, ...)` keeps p in a0;
 *   - poly = &stree_surf[list[i]] (no pointer local): the strength-reduced pointer takes s1;
 *   - bestH = 250 (int literal) and bestAbs = 250.0f are separate constant webs (two mtc1);
 *   - fabsf(h) written twice (no named local) gives abs in $f2 and keeps h in $f4;
 *   - best/bestIdx/bestAbs/bestH assigned before the out[] copies;
 *   - mat[1][k] * -out[1] operand order; out[k] = pt[k] + pos[k];
 *   - PointInSurf's real parameter order is (poly, wp, q, pt, bound, outIdx, mat, zmin):
 *     callers set up s4, s2, s6, s0, s8, s5, s7, f20 in that order (TireSurf agrees).
 */
Poly *RoadPosition(f32 *pos, f32 *out, f32 (*mat)[3]) {
    f32 m[3][3];
    u32 i;
    u16 idx;
    u16 bestIdx;
    s16 quad;
    u8 n;
    QNode *node;
    f32 h;
    u32 off;
    u8 *p;
    f32 pt[3];
    u16 list[36];
    f32 bestAbs;
    f32 bestH;
    Poly *poly;
    Poly *best;
    s32 tries;
    f32 x;
    f32 z;

    best = NULL;
    tries = 0;
again:
    if (pos[0] < 0.0f && (f32)(s32)pos[0] != pos[0]) {
        x = pos[0] - 1.0f;
    } else {
        x = pos[0];
    }
    if (pos[2] < 0.0f && (f32)(s32)pos[2] != pos[2]) {
        z = pos[2] - 1.0f;
    } else {
        z = pos[2];
    }
    node = updownleaf(stree_nodes, (s32)x, (s32)z, &quad);
    if (node != NULL) {
        off = node->child[quad];
        if (node->mask & (16 << quad)) {
            off |= 0x10000;
        }
        if (off != 0) {
            p = stree_slist + off;
            bestH = 250;
            n = *p++;
            bestAbs = 250.0f;
            DecodeSlist(p, n, list, -1);
            for (i = 0; i < n; i++) {
                poly = &stree_surf[list[i]];
                if ((poly->type & 0xF) == 5 || (poly->type & 0xF) == 6 || (poly->type & 0xF) == 15) {
                    continue;
                }
                h = bestAbs;
                if (PointInSurf(poly, pos, NULL, pt, &h, (s16 *)&idx, (f32 *)m, -20.0f) != 0) {
                    if (fabsf(h) < bestAbs) {
                        best = poly;
                        bestIdx = idx;
                        bestAbs = fabsf(h);
                        bestH = h;
                        out[0] = pt[0];
                        out[1] = pt[1];
                        out[2] = pt[2];
                        CopyUV(m, mat);
                    }
                }
            }
            if (bestAbs != 250.0f) {
                if (best->type & 0x2000) {
                    fHalfPipeInterp((intptr_t)best, bestIdx, pos, out, mat, 250.0f);
                    if (out[1] < -5.0f) {
                        goto fail;
                    }
                    pt[0] = mat[1][0] * -out[1];
                    pt[1] = mat[1][1] * -out[1];
                    pt[2] = mat[1][2] * -out[1];
                } else {
                    if (best->type & 0x1000) {
                        fPathInterp((intptr_t)best, bestIdx, pos, out, mat);
                        NormalVector(mat[0]);
                        NormalVector(mat[1]);
                        NormalVector(mat[2]);
                    }
                    pt[0] = mat[1][0] * -bestH;
                    pt[1] = mat[1][1] * -bestH;
                    pt[2] = mat[1][2] * -bestH;
                }
                out[0] = pos[0] + pt[0];
                out[1] = pos[1] + pt[1];
                out[2] = pos[2] + pt[2];
                return best;
            }
        }
    }
fail:
    if (tries == 0) {
        tries = 1;
        pos[0] += 1.0f;
        goto again;
    }
    if (tries == 1) {
        tries = 2;
        pos[0] -= 1.0f;
        pos[2] += 1.0f;
        goto again;
    }
    if (tries == 2) {
        tries = 3;
        pos[2] -= 2.0f;
        goto again;
    }
    return NULL;
}
