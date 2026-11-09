/* Distance2D.c: the length of a 2D vector (MAT3 fmath.c, Distance2D()). */
#include "game/game.h"

f32 sqrtf(f32);
#pragma intrinsic (sqrtf)

/* The length of (x, z): the game calls it with the two horizontal components of a distance (MAT3 names them x and
 * z). */
f32 Distance2D(f32 x,f32 z) {return sqrtf(x*x+z*z);}
