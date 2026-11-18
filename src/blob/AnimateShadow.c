/* AnimateShadow.c: a car's shadow each frame (MAT3 visuals.c, AnimateShadow()).
 *
 * The N64's shadow is one four-cornered polygon under the tires (StartShadow); MAT3's is built from strips. */
#include "game/game.h"

extern f32 fabsf(f32);
#pragma intrinsic(fabsf)

/* A car visual (tVisual) on the PC: 0x28 bytes, next, data and func pointer-sized (struct pc_visual, docs/port/CARS.md
 * 3). */
typedef struct tVisual {
    /* 0x00 */ void *next;
    /* 0x08 */ s16 index;
    /* 0x0A */ s16 objnum;          /* the shadow's polygon */
    /* 0x0C */ s16 slot;
    /* 0x10 */ uintptr_t data;
    /* 0x18 */ f32 timeStamp;
    /* 0x20 */ void (*func)(struct tVisual *v, s16 op);
} tVisual;

/* A polygon of the scene (PolyInstanceList). */
typedef struct Poly {
    /* 0x00 */ s16 active;
    /* 0x02 */ u16 flags;
    /* 0x04 */ u8 pad[0x54];
} Poly;

#define rng(x, lo, hi) (((x) < (lo)) ? (lo) : (((x) > (hi)) ? (hi) : (x)))

/* Car v->slot's shadow (VIS_SHADOW), each frame (op 1): a polygon whose corners are under the four tires (taken
 * round the car: 0, 1, 3, 2), each lowered by its tire's height off the ground and lifted a little (1 foot, up to
 * 2 as the tire is higher) to stay above the road, then widened at the front and back and lengthened by the body's
 * shadow_scales (the wreck's for a hulk). It fades as the car rises (alpha 255 less 8 times the tires' mean height,
 * from 20 to 255; 192 at most). It is hidden when the tires' heights differ by more than 20 feet (the car over an
 * edge), for a car shown as a mine, while respawning, with the invisible cheat or a hidden car; the car's own window
 * doesn't show it in views 0 and 1. op 0 stops it: the polygon is removed. */
void AnimateShadow(tVisual *v, s16 op) {
    f32 pos[3], mat[3][3], xyz[4][3], dist[4], d1, d2;
    s16 i, whl;
    tCarData *car = &pc_cars[v->slot];
    tModelData *m = &pc_models[v->slot];
    f32 fscale, bscale, len, offset;
    s32 body;

    if (op == 0) {
        if (v->objnum >= 0) {
            ZOID_RemovePoly(&PolyInstanceList[v->objnum]);
        }
        v->func = 0;
        v->objnum = -1;
        return;
    }

    d1 = m->reckon.airdist[3] - m->reckon.airdist[1];
    offset = m->reckon.airdist[2] - m->reckon.airdist[0];
    fscale = m->reckon.airdist[1] - m->reckon.airdist[0];
    bscale = m->reckon.airdist[3] - m->reckon.airdist[2];
    if (fabsf(d1) > 20 || fabsf(offset) > 20 || fabsf(fscale) > 20 || fabsf(bscale) > 20 ||
        car->carMine != 0 || m->resurrect.moving_state > -1 || (car->appearance & APPEAR_INVISIBLE) || gInvisibleCar != 0 || m->hide_car) {
        ZOID_HidePoly(&PolyInstanceList[v->objnum]);
        return;
    }

    for (d1 = i = 0; i < 4; i++) {
        d1 += (dist[i] = m->reckon.airdist[i]);
        if (dist[i] < 0.0f) {
            dist[i] = 0.0f;
        }
    }
    d1 *= 0.25f;

    for (i = 0; i < 4; i++) {
        whl = (i < 2) ? i : 5 - i;
        offset = (dist[whl] > 10.0f) ? 2.0f : dist[whl] * 0.1f + 1.0f;
        xyz[i][0] = car->dr_tirepos[whl][0];
        xyz[i][1] = car->dr_tirepos[whl][1] - dist[whl] + offset;
        xyz[i][2] = car->dr_tirepos[whl][2];
    }

    if (car->appearance & APPEAR_HULK) {
        body = 13;
    } else {
        body = m->body_type;
    }

    fscale = shadow_scales[body][0];
    bscale = shadow_scales[body][1];
    for (i = 0; i < 3; i++) {
        len = xyz[0][i] - xyz[1][i];
        xyz[0][i] += len * fscale;
        xyz[1][i] -= len * fscale;

        len = xyz[3][i] - xyz[2][i];
        xyz[3][i] += len * bscale;
        xyz[2][i] -= len * bscale;
    }

    fscale = shadow_scales[body][2];
    bscale = shadow_scales[body][3];
    for (i = 0; i < 3; i++) {
        len = xyz[0][i] - xyz[3][i];
        xyz[0][i] += len * fscale;
        xyz[3][i] -= len * bscale;

        len = xyz[1][i] - xyz[2][i];
        xyz[1][i] += len * fscale;
        xyz[2][i] -= len * bscale;
    }

    ZOID_ShowPoly(&PolyInstanceList[v->objnum]);
    gShadowColor.c[3] = 255.0f - rng(d1 * 8.0f, 20.0f, 255.0f);
    if (gShadowColor.c[3] > 192) {
        gShadowColor.c[3] = 192;
    }

    if (car->camera >= 0 && (car->view == 0 || car->view == 1))
        ZOID_ClearPolyFlags(&PolyInstanceList[v->objnum], 1 << car->camera);
    else ZOID_SetPolyFlags(&PolyInstanceList[v->objnum], 15);
    ZOID_UpdatePoly(&PolyInstanceList[v->objnum], 4, xyz, 0, gShadowColor.c, 0, 0);
}
