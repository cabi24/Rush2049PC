/* normalize_vector_or_x (MAT3 fmath.c's vector normalize, the N64 form): a vector made unit length. */
#include "game/game.h"

extern f32 sqrtf(f32);
#pragma intrinsic(sqrtf)

/* Scales the 3-vector v to length 1 and returns its old length; a zero vector becomes (1, 0, 0). */
f32 normalize_vector_or_x(f32 *v) {
    f32 length;
    f32 x;
    f32 y;
    f32 scale;
    f32 z;

    z = v[2];
    x = v[0];
    y = v[1];
    length = sqrtf((z * z) + ((x * x) + (y * y)));
    if (length != 0.0f) {
        scale = 1.0f / length;
        v[0] = (f32) (x * scale);
        v[1] = (f32) (y * scale);
        v[2] = (f32) (z * scale);
    } else {
    v[1] = 0.0f;
    v[2] = 0.0f;
    v[0] = 1.0f;
    }
    return length;
}
