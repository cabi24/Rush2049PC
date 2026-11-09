/* ScaleUVXYZ.c: scale the three rows of a 3x3 matrix (MAT3 fmath.c, ScaleUVXYZ()). */
#include "game/game.h"

/* dst = src with row 0 times sx, row 1 times sy and row 2 times sz (nine floats each, row by row): an orientation
 * matrix scaled along the object's own x, y and z axes. src and dst may be the same matrix. */
void ScaleUVXYZ(float *src, float *dst, float sx, float sy, float sz) {
    s32 i;
    for (i = 0; i < 3; i++) {
        dst[i] = src[i] * sx;
        dst[i + 3] = src[i + 3] * sy;
        dst[i + 6] = src[i + 6] * sz;
    }
}
