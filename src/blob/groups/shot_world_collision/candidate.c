/* ShotToWorldCollision: a shot against the track's surfaces (MAT3 stree.c). */
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

#define FLOORF(v) (((v) < 0.0f && (f32)(s32)(v) != (v)) ? (v) - 1.0f : (v))

/* Does the segment from `start` to `end` hit a surface? Walks the surface tree's leaves along it, testing their
 * surfaces (CylinderInSurf, within `radius`) but removed ones (kind 15) and kind `skipType`, leaf by leaf from the
 * start: `end` moves to the hit point and `mat` gets the surface's frame, with `snap` interpolated on a half-pipe
 * edge (flag 0x2000) or a road path surface (0x1000). Returns the surface hit, or NULL. The weapons' shots. */
surftyp *ShotToWorldCollision(f32 *start, f32 *end, f32 (*mat)[3], f32 radius, s32 snap, s32 skipType) {
    f32 m[3][3];
    u32 i;
    s16 quad;
    s16 endQuad;
    f32 t;
    s16 mx;
    s16 mz;
    u8 n;
    f32 cur[3];
    f32 d[3];
    f32 np[3];
    f32 vc[3];
    s16 state;
    u16 idx;
    u16 bestIdx;
    u16 list[35];
    u32 off;
    s16 minx;
    s16 maxx;
    s16 minz;
    s16 maxz;
    f32 x;
    f32 z;
    streetyp *node;
    streetyp *endNode;
    surftyp *poly;
    surftyp *best;
    u8 *p;

    best = NULL;
    cur[0] = start[0];
    cur[1] = start[1];
    cur[2] = start[2];
    x = FLOORF(cur[0]);
    node = updownleaf(stree_nodes, (s32)x, (s32)(z = FLOORF(cur[2])), &quad);
    if (node == NULL) {
        return NULL;
    }
    if (end[0] < 0.0f && (f32)(s32)end[0] != end[0]) {
        x = end[0] - 1.0f;
    } else {
        x = end[0];
    }
    endNode = updownleaf(stree_nodes, (s32)x, (s32)(z = FLOORF(end[2])), &endQuad);
    for (;;) {
        mx = node->minh + node->maxh;
        if (mx < 0) {
            mx++;
        }
        mz = node->minv + node->maxv;
        if (mz < 0) {
            mz++;
        }
        mx >>= 1;
        mz >>= 1;
        minx = (quad & 1) ? mx : node->minh;
        maxx = (quad & 1) ? node->maxh : mx;
        minz = (quad & 2) ? node->minv : mz;
        maxz = (quad & 2) ? mz : node->maxv;
        off = node->child[quad];
        if (node->flags & (16 << quad)) {
            off |= 0x10000;
        }
        if (off != 0) {
            p = stree_slist + off;
            n = *p++;
            DecodeSlist(p, n, list, -1);
            for (i = 0; i < n; i++) {
                poly = &stree_surf[list[i]];
                if ((poly->flags & 0xF) == 15 || (poly->flags & 0xF) == skipType) {
                    continue;
                }
                state = CylinderInSurf(poly, cur, end, np, vc, (f32 *)m, 0, (s16 *)&idx, radius * radius);
                if (state != 0) {
                    if (np[0] < minx || maxx <= np[0] || np[2] < minz || maxz <= np[2]) {
                        continue;
                    }
                    end[0] = np[0];
                    end[1] = np[1];
                    end[2] = np[2];
                    CopyUV(m, mat);
                    best = poly;
                    bestIdx = idx;
                }
            }
        }
        if (best != NULL) {
            if (snap != 0) {
                if (best->flags & 0x2000) {
                    fHalfPipeInterp((intptr_t)best, bestIdx, end, vc, mat, 250.0f);
                } else if (best->flags & 0x1000) {
                    fPathInterp((intptr_t)best, bestIdx, end, vc, mat);
                    NormalVector(mat[0]);
                    NormalVector(mat[1]);
                    NormalVector(mat[2]);
                }
            }
            return best;
        }
        if (node == endNode && quad == endQuad) {
            return NULL;
        }
        d[0] = end[0] - cur[0];
        d[1] = end[1] - cur[1];
        d[2] = end[2] - cur[2];
        if (d[0] < 0.0f) {
            np[0] = minx;
            t = (np[0] - cur[0]) / d[0];
            np[1] = cur[1] + d[1] * t;
            np[2] = cur[2] + d[2] * t;
            if (np[2] == minz && d[2] < 0.0f) {
                node = updownleaf(stree_nodes, minx - 1, minz - 1, &quad);
                goto moved;
            }
            if (np[2] == maxz && d[2] < 0.0f) {
                node = updownleaf(stree_nodes, minx - 1, maxz - 1, &quad);
                goto moved;
            }
            if (minz <= np[2] && np[2] <= maxz) {
                node = updownleaf(stree_nodes, minx - 1, (s32)FLOORF(np[2]), &quad);
                goto moved;
            }
        } else if (d[0] > 0.0f) {
            np[0] = maxx;
            t = (np[0] - cur[0]) / d[0];
            np[1] = cur[1] + d[1] * t;
            np[2] = cur[2] + d[2] * t;
            if (np[2] == minz && d[2] < 0.0f) {
                node = updownleaf(stree_nodes, maxx, minz - 1, &quad);
                goto moved;
            }
            if (np[2] == maxz && d[2] < 0.0f) {
                node = updownleaf(stree_nodes, maxx, maxz - 1, &quad);
                goto moved;
            }
            if (minz <= np[2] && np[2] <= maxz) {
                node = updownleaf(stree_nodes, maxx, (s32)FLOORF(np[2]), &quad);
                goto moved;
            }
        }
        if (d[2] < 0.0f) {
            np[2] = minz;
            t = (np[2] - cur[2]) / d[2];
            np[1] = cur[1] + d[1] * t;
            np[0] = cur[0] + d[0] * t;
            if (np[0] == minx && d[0] < 0.0f) {
                node = updownleaf(stree_nodes, minx - 1, minz - 1, &quad);
                goto moved;
            }
            if (np[0] == maxx && d[0] < 0.0f) {
                node = updownleaf(stree_nodes, maxx - 1, minz - 1, &quad);
                goto moved;
            }
            if (minx <= np[0] && np[0] <= maxx) {
                node = updownleaf(stree_nodes, (s32)FLOORF(np[0]), minz - 1, &quad);
                goto moved;
            }
        } else if (d[2] > 0.0f) {
            np[2] = maxz;
            t = (np[2] - cur[2]) / d[2];
            np[1] = cur[1] + d[1] * t;
            np[0] = cur[0] + d[0] * t;
            if (np[0] == minx && d[0] < 0.0f) {
                node = updownleaf(stree_nodes, minx - 1, maxz, &quad);
                goto moved;
            }
            if (np[0] == maxx && d[0] < 0.0f) {
                node = updownleaf(stree_nodes, maxx - 1, maxz, &quad);
                goto moved;
            }
            if (minx <= np[0] && np[0] <= maxx) {
                node = updownleaf(stree_nodes, (s32)FLOORF(np[0]), maxz, &quad);
                goto moved;
            }
        }
moved:
        cur[0] = np[0];
        cur[1] = np[1];
        cur[2] = np[2];
        if (node == NULL) {
            return NULL;
        }
    }
}
