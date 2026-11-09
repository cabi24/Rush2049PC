/* BodyVector.c: a world vector in a body's own frame (MAT3 fmath.c, BodyVector()).
 *
 * The inverse of WorldVector: it multiplies by the orientation matrix where WorldVector multiplies by its
 * transpose. */
#include "game/game.h"

/* dst = uvs * src: the world-frame vector src expressed along the body's axes, which are the rows of the
 * orientation matrix uvs (3 x 3, row by row: uvs[3 * row + column]; each row one of the body's unit vectors in
 * world coordinates). */
void BodyVector(f32 *src, f32 *dst, f32 *uvs) {
    dst[0] = src[0] * uvs[0] + src[1] * uvs[1] + src[2] * uvs[2];
    dst[1] = src[0] * uvs[3] + src[1] * uvs[4] + src[2] * uvs[5];
    dst[2] = src[0] * uvs[6] + src[1] * uvs[7] + src[2] * uvs[8];
}
