/* TireSurf: the road surface under a tyre (MAT3 stree.c). */
#include "game/game.h"

float fabsf(float);

/* A surface (MAT3 surftyp, stree_surf[]). */
typedef struct surftyp {
    /* 0x00 */ u16 flags;     /* low nibble: the surface's kind (5 and 6 walls) */
    /* 0x02 */ u16 snd_flags; /* low nibble: its vertices */
    /* 0x04 */ s16 uvs[9];    /* its orientation, 1.0 = 0x4000 (MAT3 s16[3][3]) */
    /* 0x16 */ u16 vli;       /* its vertex list, a byte offset in stree_vlist */
} surftyp;
VIEW(surftyp *, stree_surf);
#define stree_surf stree_surf_view

enum n64p_Poly { n64p_Poly_ };
enum n64p_QNode { n64p_QNode_ };
/* The model record (MAT3 tModelData): the fields this reads and sets. Its pointers are u32 window addresses on the
 * PC too (docs/port/CARS.md 4). */
typedef struct EUCar {
    u8 pad0[928];
    enum n64p_QNode node[4];        /* 0x3A0 each tyre's node of the surface tree */
    u8 pad944[1440 - 944];
    u16 tiresli[4];                 /* 0x5A0 each tyre's surface last found, which DecodeSlist puts first */
    u8 pad1448[1564 - 1448];
    s16 sviscode[4];                /* 0x61C each tyre's surface's visual code (flags bits 4-7) */
    s16 roadboost[4];               /* 0x624 its boost (bits 8-11, negative with 0x8000) */
    u16 sound_flags[4];             /* 0x62C its sound (snd_flags) */
    u8 pad1588[1600 - 1588];
    u8 crashflag;                   /* 0x640 */
    u8 pad1601[7];
    enum n64p_Poly movingSurface;   /* 0x648 a moving surface it is on (snd_flags 0x30) */
    u8 pad1612[1732 - 1612];
    s16 moving_state;               /* 0x6C4 resurrect.moving_state: -1 unless being put back */
    u8 pad1734[1741 - 1734];
    u8 waterAbort;                  /* 0x6CD resurrect.waterAbort */
    u8 pad1742[1990 - 1742];
    s16 slot;                       /* 0x7C6 */
} EUCar;
#define N64P(x) _Generic((x), enum n64p_Poly: (surftyp *)PC_N64P(x), enum n64p_QNode: (streetyp *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)

VIEW(f32[], unitmatrix);
#define unitmatrix unitmatrix_view

/* An empty function the N64 code calls after the kill wall's sound. */
static void eu_nop(void) {}

/* Finds the road surface under tyre `wheel` of model `car` at `pos`, as RoadPosition (starting from the tyre's last
 * tree node and surface, walls and removed surfaces skipped, from 5 feet below): `out` the point on it, `mat` its
 * frame, *surf its kind; the tyre's visual code, boost and sound flags from it, and a moving surface it is on. A
 * tyre on water (7), a kill surface (3) or a battle trap (4) within a quarter foot ends the car as in
 * CollideCarBodyAgainstSurface. With no surface: kind 0, `out` 200 feet below. */
void TireSurf(EUCar *car, f32 *pos, f32 *out, s32 *surf, f32 (*mat)[3], s32 wheel) {
    f32 m[3][3];
    u32 i;
    u8 *p;
    u16 idx;
    u16 bestIdx;
    s16 quad;
    u8 n;
    streetyp *node;
    f32 h;
    u32 off;
    f32 pt[3];
    u16 list[36];
    f32 bestAbs;
    f32 bestH;
    surftyp *poly;
    surftyp *best;
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
    if (node->flags & (16 << quad)) {
        off |= 0x10000;
    }
    if (off != 0) {
    p = stree_slist + off;
    n = *p++;
    DecodeSlist(p, n, list, car->tiresli[wheel]);
    for (i = 0; i < n; i++) {
        poly = &stree_surf[list[i]];
        if ((poly->flags & 0xF) == 5 || (poly->flags & 0xF) == 6 || (poly->flags & 0xF) == 15) {
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
                car->tiresli[wheel] = list[i];
            }
        }
    }
    if (bestAbs != 250.0f) {
    if (out[1] < 1.0f && (best->snd_flags & 0x30)) {
        car->movingSurface = N64W(best);
    }
    if (best->flags & 0x2000) {
        fHalfPipeInterp((intptr_t)best, bestIdx, pos, out, mat, 250.0f);
        if (out[1] < -5.0f) {
            goto miss;
        }
    } else if (best->flags & 0x1000) {
        fPathInterp((intptr_t)best, bestIdx, pos, out, mat);
    }
    *surf = best->flags & 0xF;
    car->sviscode[wheel] = (best->flags & 0xF0) >> 4;
    car->roadboost[wheel] = (best->flags & 0xF00) >> 8;
    if (best->flags & 0x8000) {
        car->roadboost[wheel] = -car->roadboost[wheel];
    }
    car->sound_flags[wheel] = best->snd_flags;
    if (best->snd_flags & 0x30) {
        car->sound_flags[wheel] &= 0x7FF;
    }
    if ((*surf == 3 || *surf == 4 || *surf == 7) && out[1] < 0.25f) {
        if (PC_CAR_STATE(car->slot) == 0 && PC_CAR_B358(car->slot) == 0 && car->moving_state == -1) {
            if (PC_CAR_SLOT(car->slot)) {}
            if (*surf == 4) {
                AddBattlePoint(car->slot, car->slot, -1);
                car->crashflag = 1;
            } else if (*surf == 7) {
                car->waterAbort = 1;
                AbortCar(car->slot, 1);
            } else {
                car->waterAbort = 1;
                AbortCar(car->slot, 1);
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
    car->sviscode[wheel] = 0;
    car->roadboost[wheel] = 0;
    car->sound_flags[wheel] = 0;
    out[0] = pos[0];
    out[1] = pos[1] - -200.0f;
    out[2] = pos[2];
    CopyUV(unitmatrix, mat);
}
