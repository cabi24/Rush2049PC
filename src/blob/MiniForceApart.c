/* MiniForceApart.c: push a car out of a small collision object (MAT3 collision.c, MiniForceApart()).
 *
 * The one-sided form of ForceApart: setMiniFBCollisionForce calls it when a car has sunk into a "mini" collision
 * body (a track object), which doesn't move. */
#include "game/game.h"

/* The model record (tModelData, docs/port/CARS.md 4), the fields used here. */
typedef struct tModelData {
    _tCar *car;
    Tune44 *tune;
    char pad8[0x11C];
    f32 centerForce[3];                         /* +0x124 the force on the body's centre this step, car frame */
    char pad130[0x270];
    streetyp *lasttp[4];
    char pad3B0[0x5C];
    const s16 *torquecurve;
    char pad410[0x230];
    s8 crashflag;                               /* +0x640 crashed */
    char pad641[0x7];
    surftyp *movingSurface;
    char pad64C[0x154];
    f32 reckon_uvs[3][3];                       /* +0x7A0 reckon.uvs: the orientation the collisions use */
} tModelData;

extern f32 sqrtf(f32);

/* Push model `m` along `dir`, a world direction (away from the object): mini_force_normal (MAT3 250,000), or
 * mini_force_crashed (MAT3 100,000) for a crashed car, along world x if the car is at the object's centre
 * (|dir|^2 below mini_force_min_dist2); added to its centerForce in its own frame. */
void MiniForceApart(tModelData *m, f32 *dir) {
    f32 mag;
    f32 len;
    f32 force[3];
    f32 cent[3];
    s32 i;

    if (m->crashflag == 0) {
        mag = mini_force_normal;
    } else {
        mag = mini_force_crashed;
    }
    len = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
    if (len < mini_force_min_dist2) {
        force[0] = mag;
        force[1] = 0.0f;
        force[2] = 0.0f;
    } else {
        len = mag / sqrtf(len);
        for (i = 0; i < 3; i++) {
            force[i] = dir[i] * len;
        }
    }
    BodyVector(force, cent, m->reckon_uvs[0]);
    m->centerForce[0] = m->centerForce[0] + cent[0];
    m->centerForce[1] = m->centerForce[1] + cent[1];
    m->centerForce[2] = m->centerForce[2] + cent[2];
}
