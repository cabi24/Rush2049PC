#include "game/game.h"

float sqrtf(float);
#pragma intrinsic (sqrtf)

void zoid_build_upright_matrix(f32 *m) {
    f32 a, b, len;

    a = zoid_lookat_matrix[2];
    b = zoid_lookat_matrix[10];
    len = sqrtf(a * a + b * b);
    if (len < upright_min_length) {
        a = 0.0f;
        b = 1.0f;
    } else {
        a /= len;
        b /= len;
    }
    m[0] = -b;
    m[4] = 0.0f;
    m[8] = -a;
    m[1] = 0.0f;
    m[9] = 0.0f;
    m[2] = a;
    m[6] = 0.0f;
    m[10] = -b;
    m[5] = 1.0f;
}
