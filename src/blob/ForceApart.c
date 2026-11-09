#include "game/game.h"

typedef struct Car {
    char pad0[0x124];
    f32 unk124[3];
    char pad130[0x510];
    s8 unk640;
    char pad641[0x15F];
    f32 unk7A0[3][3];
} Car;

extern f32 sqrtf(f32);
#pragma intrinsic(sqrtf)

void ForceApart(Car *car, Car *a, Car *b, f32 *dir) {
    Car *other;
    f32 len;
    f32 mag;
    f32 force[3];
    f32 out[3];
    s32 i;

    if (car == a) {
        other = b;
    } else {
        other = a;
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
    BodyVector(force, out, car->unk7A0[0]);
    car->unk124[0] = car->unk124[0] + out[0];
    car->unk124[1] = car->unk124[1] + out[1];
    car->unk124[2] = car->unk124[2] + out[2];
    BodyVector(force, out, other->unk7A0[0]);
    other->unk124[0] = other->unk124[0] - out[0];
    other->unk124[1] = other->unk124[1] - out[1];
    other->unk124[2] = other->unk124[2] - out[2];
}
