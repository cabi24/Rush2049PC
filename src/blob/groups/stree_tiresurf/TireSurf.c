#include "game/game.h"

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

typedef struct Poly {
    u16 type;
    u16 cnt;
    u8 body[0x12];
    u16 off;
} Poly;
VIEW(Poly *, stree_surf);
#define stree_surf stree_surf_view

enum n64p_Poly { n64p_Poly_ };
enum n64p_QNode { n64p_QNode_ };
typedef struct EUCar {
    u8 pad0[928];
    enum n64p_QNode node[4];
    u8 pad944[1440 - 944];
    u16 hint[4];
    u8 pad1448[1564 - 1448];
    s16 kind[4];
    s16 slope[4];
    u16 attr[4];
    u8 pad1588[1600 - 1588];
    u8 b1600;
    u8 pad1601[7];
    enum n64p_Poly p1608;
    u8 pad1612[1732 - 1612];
    s16 s1732;
    u8 pad1734[1741 - 1734];
    u8 b1741;
    u8 pad1742[1990 - 1742];
    s16 id;
} EUCar;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Poly: (Poly *)PC_N64P(x), enum n64p_QNode: (QNode *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)

VIEW(f32[], unitmatrix);
#define unitmatrix unitmatrix_view

static void eu_nop(void) {}
void TireSurf(EUCar *car, f32 *pos, f32 *out, s32 *surf, f32 (*mat)[3], s32 wheel) {
    f32 m[3][3];
    u32 i;
    u8 *p;
    u16 idx;
    u16 bestIdx;
    s16 quad;
    u8 n;
    QNode *node;
    f32 h;
    u32 off;
    f32 pt[3];
    u16 list[36];
    f32 bestAbs;
    f32 bestH;
    Poly *poly;
    Poly *best;
    f32 x;
    f32 z;

    best = NULL;
    bestAbs = 250.0f;
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
    node = updownleaf(N64P(car->node[wheel]), (s32)x, (s32)z, &quad);
    if (node == NULL) {
        goto miss;
    }
    car->node[wheel] = N64W(node);
    off = node->child[quad];
    if (node->mask & (16 << quad)) {
        off |= 0x10000;
    }
    if (off != 0) {
    p = stree_slist + off;
    n = *p++;
    DecodeSlist(p, n, list, car->hint[wheel]);
    for (i = 0; i < n; i++) {
        poly = &stree_surf[list[i]];
        if ((poly->type & 0xF) == 5 || (poly->type & 0xF) == 6 || (poly->type & 0xF) == 15) {
            continue;
        }
        h = bestAbs;
        if (PointInSurf(poly, pos, NULL, pt, &h, (s16 *)&idx, (f32 *)m, -5.0f) != 0) {
            if (-5.0f < h && h < bestAbs) {
                best = poly;
                bestIdx = idx;
                bestAbs = h;
                out[0] = pt[0];
                out[1] = pt[1];
                out[2] = pt[2];
                CopyUV(m, mat);
                car->hint[wheel] = list[i];
            }
        }
    }
    if (bestAbs != 250.0f) {
    if (out[1] < 1.0f && (best->cnt & 0x30)) {
        car->p1608 = N64W(best);
    }
    if (best->type & 0x2000) {
        fHalfPipeInterp((intptr_t)best, bestIdx, pos, out, mat, 250.0f);
        if (out[1] < -5.0f) {
            goto miss;
        }
    } else if (best->type & 0x1000) {
        fPathInterp((intptr_t)best, bestIdx, pos, out, mat);
    }
    *surf = best->type & 0xF;
    car->kind[wheel] = (best->type & 0xF0) >> 4;
    car->slope[wheel] = (best->type & 0xF00) >> 8;
    if (best->type & 0x8000) {
        car->slope[wheel] = -car->slope[wheel];
    }
    car->attr[wheel] = best->cnt;
    if (best->cnt & 0x30) {
        car->attr[wheel] &= 0x7FF;
    }
    if ((*surf == 3 || *surf == 4 || *surf == 7) && out[1] < 0.25f) {
        if (PC_CAR_STATE(car->id) == 0 && PC_CAR_B358(car->id) == 0 && car->s1732 == -1) {
            if (PC_CAR_SLOT(car->id)) {}
            if (*surf == 4) {
                AddBattlePoint(car->id, car->id, -1);
                car->b1600 = 1;
            } else if (*surf == 7) {
                car->b1741 = 1;
                AbortCar(car->id, 1);
            } else {
                car->b1741 = 1;
                AbortCar(car->id, 1);
                PlayGameSound(21, 0, 1, 2);
                eu_nop();
            }
        }
    }
    return;
    }
    }
miss:
    *surf = 0;
    car->kind[wheel] = 0;
    car->slope[wheel] = 0;
    car->attr[wheel] = 0;
    out[0] = pos[0];
    out[1] = pos[1] - -200.0f;
    out[2] = pos[2];
    CopyUV(unitmatrix, mat);
}
