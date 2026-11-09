/* normal_vector/group.c: make a 3D vector unit length (MAT3 fmath.c, NormalVector()). */
#include "game/game.h"

f32 sqrtf(f32);
#pragma intrinsic(sqrtf)

/* Scale the vector v to length 1 in place and return its old length; a vector no longer than normalvector_floor
 * (MAT3 0.00001) is left as it is and gives 0. */
f32 NormalVector(f32 *v)
{
    f32 length, inverse_length;

    length = Distance(v[0], v[1], v[2]);
    if (length <= normalvector_floor) {
        return 0.0f;
    }
    inverse_length = 1.0f / length;
    v[0] *= inverse_length;
    v[1] *= inverse_length;
    v[2] *= inverse_length;
    return length;
}
