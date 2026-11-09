/* PointInBody.c: is a point inside a car's body box? (MAT3 collision.c, PointInBody()).
 *
 * The car-to-car collision (collision.c's checks, setFBCollisionForce) tests the other car's body corners against
 * this car's box, in this car's frame. */
#include "game/game.h"

/* The model record (tModelData), the body corners it reads. */
typedef struct tModelData {
    _tCar *car;
    Tune44 *tune;
    u8 pad8[0xEC];
    f32 bodyR[4][3];            /* +0x0F4 the body's corners in the car's frame: [0] front right, [3] rear left */
} tModelData;

/* 1 if pt (in model m's frame, feet) lies within m's body: x between the corners 3 and 0, y between 1 ft below the
 * car's origin and `top`, z between the rear and the front corners; else 0. */
s32 PointInBody(tModelData *m, f32 *pt, f32 top) {
    f32 x;
    f32 y;
    f32 z;

    x = pt[0];
    if ((m->bodyR[0][0] < x) || (x < m->bodyR[3][0])) {
        return 0;
    }
    y = pt[1];
    if ((top < y) || (y < -1.0f)) {
        return 0;
    }
    z = pt[2];
    if ((m->bodyR[0][2] < z) || (z < m->bodyR[3][2])) {
        return 0;
    }
    return 1;
}
