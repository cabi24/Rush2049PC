/* distance/group.c: the length of a 3D vector (MAT3 fmath.c, Distance()). */
#include "game/game.h"

f32 sqrtf(f32);
#pragma intrinsic(sqrtf)

/* The length of the vector (x, y, z). */
f32 Distance(f32 x, f32 y, f32 z)
{
    return sqrtf((x * x + y * y) + z * z);
}
