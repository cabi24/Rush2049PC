/* invmagnitude.c: one over the length of a 3D vector (MAT3 fmath.c, invmagnitude()). */
#include "game/game.h"

extern f32 sqrtf(f32);
#pragma intrinsic (sqrtf)

/* 1 / |vec|, with the squared length held at invmagnitude_floor or more (MAT3's invsqr: 0.0001) so a zero vector
 * gives a large finite number rather than a division by zero. */
f32 invmagnitude(f32 *vec) {
 f32 sum=vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2];
 if(sum<invmagnitude_floor) sum=invmagnitude_floor;
 return 1.0f/sqrtf(sum);
}
