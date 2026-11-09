/* ForceApart.c: push two overlapping cars apart (MAT3 collision.c, ForceApart()).
 *
 * The car-to-car collision (collision.c, setFBCollisionForce) calls it when two cars' bodies have sunk into each
 * other: an equal and opposite force along the line between them moves them apart over the next steps. */
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
    s8 crashflag;                               /* +0x640 */
    char pad641[0x7];
    surftyp *movingSurface;
    char pad64C[0x154];
    f32 reckon_uvs[3][3];                       /* +0x7A0 reckon.uvs: the orientation the collisions use */
} tModelData;

extern f32 sqrtf(f32);

/* Push the pair m1, m2 apart; `m` is the one of them this step is for, `dir` the world direction to push it in
 * (the other goes the opposite way). The force is force_apart_force (MAT3 40,000) along `dir`, or along world x
 * when the cars are on top of each other (|dir|^2 below force_apart_min_dist_sq). It goes into each car's
 * centerForce in its own frame (BodyVector by its orientation): added to m's, taken from the other's. */
void ForceApart(tModelData *m, tModelData *m1, tModelData *m2, f32 *dir) {
    tModelData *other;
    f32 len;
    f32 mag;       /* unused */
    f32 force[3];
    f32 cent[3];
    s32 i;

    if (m == m1) {
        other = m2;
    } else {
        other = m1;
    }
    len = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
    if (len < force_apart_min_dist_sq) {
        force[0] = force_apart_force;
        force[1] = 0.0f;
        force[2] = 0.0f;
    } else {
        len = force_apart_force_over_dist / sqrtf(len);
        for (i = 0; i < 3; i++) {
            force[i] = dir[i] * len;
        }
    }
    BodyVector(force, cent, m->reckon_uvs[0]);
    m->centerForce[0] = m->centerForce[0] + cent[0];
    m->centerForce[1] = m->centerForce[1] + cent[1];
    m->centerForce[2] = m->centerForce[2] + cent[2];
    BodyVector(force, cent, other->reckon_uvs[0]);
    other->centerForce[0] = other->centerForce[0] - cent[0];
    other->centerForce[1] = other->centerForce[1] - cent[1];
    other->centerForce[2] = other->centerForce[2] - cent[2];
}
