/* WorldVector.c: a vector from an object's frame into world coordinates (MAT3 fmath.c, WorldVector()). */
#include "game/game.h"

/* dst = src (in the frame of the orientation uvs, nine floats row by row) in world coordinates: the sum of the rows
 * of uvs (the object's axes) weighted by src's components. BodyVector is the inverse. */
void WorldVector(f32 *src, f32 *dst, f32 *uvs) {
    dst[0] = (src[0] * uvs[0] + src[1] * uvs[3]) + src[2] * uvs[6];
    dst[1] = (src[0] * uvs[1] + src[1] * uvs[4]) + src[2] * uvs[7];
    dst[2] = (src[0] * uvs[2] + src[1] * uvs[5]) + src[2] * uvs[8];
}
