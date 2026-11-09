/* vector_direction.c: the unit vector along a 3D vector (MAT3 fmath.c, direction()). */
#include "game/game.h"

/* dir = vec / |vec| (invmagnitude: a zero vector gives a zero dir). vec and dir may be the same vector. */
void vector_direction(f32 *vec, f32 *dir) {
    f32 invmag;

    invmag = invmagnitude(vec);
    dir[0] = vec[0] * invmag;
    dir[1] = vec[1] * invmag;
    dir[2] = vec[2] * invmag;
}
