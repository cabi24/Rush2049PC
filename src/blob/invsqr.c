/* invsqr.c: one over a square root (MAT3 fmath.c, invsqr()). */
#include "game/game.h"

f32 sqrtf(f32);
#pragma intrinsic (sqrtf)

/* 1 / sqrt(val), with val held at 0.0001 or more so that zero and negative values give 100 rather than a division
 * by zero or a NaN. */
f32 invsqr(f32 val) {
    if (val < 0.0001f) {
        val = 0.0001f;
    }
    return 1.0f / sqrtf(val);
}
