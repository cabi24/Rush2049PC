/* TransposeUV.c: transpose a 3x3 matrix in place (MAT3 fmath.c, TransposeUV()). */
#include "game/game.h"

/* Transpose the matrix uv (nine floats, row by row) in place; for an orientation matrix that is its inverse. */
void TransposeUV(f32 *uv) {
    f32 tmp;
    tmp = uv[1]; uv[1] = uv[3]; uv[3] = tmp;
    tmp = uv[2]; uv[2] = uv[6]; uv[6] = tmp;
    tmp = uv[5]; uv[5] = uv[7]; uv[7] = tmp;
}
