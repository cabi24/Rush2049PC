/* TB_Vector2Normalize.c: make a 2D vector unit length (MAT3 TB_Vector.c, TB_Vector2Normalize()). */
#include "game/game.h"

extern f32 sqrtf(f32);
#pragma intrinsic(sqrtf)

/* Scale the 2D vector v (v[0], v[1]) to length 1 and return its old length. A zero vector becomes (1, 0). */
f32 TB_Vector2Normalize(f32 *v) {
    f32 len;
    f32 x;
    f32 invlen;
    f32 y;

    y = v[1];
    x = v[0];
    len = sqrtf((y * y) + (x * x));
    if (len != 0.0f) {
        invlen = 1.0f / len;
        v[0] = x * invlen;
        v[1] = y * invlen;
    } else {
        v[1] = 0.0f;
        v[0] = 1.0f;
    }
    return len;
}
