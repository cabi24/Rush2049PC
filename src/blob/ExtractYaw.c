/* ExtractYaw.c: the heading of an orientation matrix (MAT3 fmath.c, ExtractYaw()). */
#include "game/game.h"

extern f32 fabsf(f32);
#pragma intrinsic (fabsf)

/* The yaw (the heading about the vertical, radians) of the 3x3 matrix mat (nine floats, row by row), into *yaw:
 * the angle of the forward row (2) in the horizontal plane. When the forward axis points nearly straight up or
 * down (its x and z under extract_yaw_epsilon, MAT3's 0.000001) the right row (0) gives it instead. */
void ExtractYaw(f32 *mat, f32 *yaw) {
 f32 x=mat[6];
 if (fabsf(x)<extract_yaw_epsilon && fabsf(mat[8])<extract_yaw_epsilon) {
  *yaw=fatan2(mat[2],mat[0]);
 } else {
  *yaw=fatan2(-x,mat[8]);
 }
}
