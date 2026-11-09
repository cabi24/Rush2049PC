/* vector_magnitude.c: the length of a 3D vector (MAT3 fmath.c, magnitude()). */
#include "game/game.h"

float sqrtf(float);
#pragma intrinsic (sqrtf)

/* The length of the vector vec. */
f32 vector_magnitude(f32 *vec) {
    return sqrtf(vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2]);
}
