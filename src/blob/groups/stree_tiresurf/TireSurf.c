/* TireSurf: the road surface under a tyre (MAT3 stree.c). */
#include "game/game.h"

float fabsf(float);

/* An empty function the N64 code calls after the kill wall's sound. */
static void eu_nop(void) {}

/* Finds the road surface under tyre `wheel` of model `car` at `pos`, as RoadPosition (starting from the tyre's last
 * tree node and surface, walls and removed surfaces skipped, from 5 feet below): `out` the point on it, `mat` its
 * frame, *surf its kind; the tyre's visual code, boost and sound flags from it, and a moving surface it is on. A
 * tyre on water (7), a kill surface (3) or a battle trap (4) within a quarter foot ends the car as in
 * CollideCarBodyAgainstSurface. With no surface: kind 0, `out` 200 feet below. */
void TireSurf(tModelData *car, f32 *pos, f32 *out, s32 *surf, f32 (*mat)[3], s32 wheel) {
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
    node = updownleaf(car->lasttp[wheel], (s32)x, (s32)z, &quad);
    if (node == NULL) {
        goto miss;
    }
    car->lasttp[wheel] = node;
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
        car->movingSurface = best;
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
        if (PC_CAR_DEAD(car->slot) == 0 && PC_CAR_CRASH_STATE(car->slot) == 0 && car->resurrect.moving_state == -1) {
            if (PC_CAR_SLOT(car->slot)) {}
            if (*surf == 4) {
                AddBattlePoint(car->slot, car->slot, -1);
                car->crashflag = 1;
            } else if (*surf == 7) {
                car->resurrect.waterAbort = 1;
                AbortCar(car->slot, 1);
            } else {
                car->resurrect.waterAbort = 1;
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
