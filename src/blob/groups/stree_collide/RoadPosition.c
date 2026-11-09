/* RoadPosition: the road surface under a point (MAT3 stree.c). */
#include "game/game.h"

float fabsf(float);

VIEW(streetyp *, stree_nodes);
#define stree_nodes stree_nodes_view

/* A surface (MAT3 surftyp, stree_surf[]). */
typedef struct surftyp {
    /* 0x00 */ u16 flags;     /* low nibble: the surface's kind (5 and 6 walls) */
    /* 0x02 */ u16 snd_flags; /* low nibble: its vertices */
    /* 0x04 */ s16 uvs[9];    /* its orientation, 1.0 = 0x4000 (MAT3 s16[3][3]) */
    /* 0x16 */ u16 vli;       /* its vertex list, a byte offset in stree_vlist */
} surftyp;
VIEW(surftyp *, stree_surf);
#define stree_surf stree_surf_view

/* Finds the road surface under `pos` (MAT3 RoadPosition): the surfaces of the tree's leaf under the point but walls
 * (kinds 5 and 6) and removed ones (15) are tested (PointInSurf, from 20 feet below to the best height so far), and
 * the one nearest in height wins: `out` gets the point on it below `pos`, `mat` its frame (a half-pipe's rounded
 * edge, flag 0x2000, from fHalfPipeInterp, failing more than 5 feet below; a road path surface, 0x1000, from
 * fPathInterp). With nothing found the probe moves a foot (+x, then -x +z, then -2z) and tries again. Returns the
 * surface, or NULL. */
surftyp *RoadPosition(f32 *pos, f32 *out, f32 (*mat)[3]) {
    f32 m[3][3];
    u32 i;
    u16 idx;
    u16 bestIdx;
    s16 quad;
    u8 n;
    streetyp *node;
    f32 h;
    u32 off;
    u8 *p;
    f32 pt[3];
    u16 list[36];
    f32 bestAbs;
    f32 bestH;
    surftyp *poly;
    surftyp *best;
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
        if (node->flags & (16 << quad)) {
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
                if ((poly->flags & 0xF) == 5 || (poly->flags & 0xF) == 6 || (poly->flags & 0xF) == 15) {
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
                if (best->flags & 0x2000) {
                    fHalfPipeInterp((intptr_t)best, bestIdx, pos, out, mat, 250.0f);
                    if (out[1] < -5.0f) {
                        goto fail;
                    }
                    pt[0] = mat[1][0] * -out[1];
                    pt[1] = mat[1][1] * -out[1];
                    pt[2] = mat[1][2] * -out[1];
                } else {
                    if (best->flags & 0x1000) {
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
