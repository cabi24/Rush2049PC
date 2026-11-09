/* CreateYPRMatrix.c: an orientation matrix from pitch, yaw and roll angles (MAT3 fmath.c, CreateYPRMatrix()). */
#include "game/game.h"

extern f32 cosf(f32);
extern f32 sinf(f32);

/* Fill the 3x3 orientation matrix `matrix` from the angles pyr (radians): pyr[0] the pitch (about x), pyr[1] the
 * yaw (about y, the vertical), pyr[2] the roll (about z). `product` holds sin(yaw) sin(pitch), then
 * cos(yaw) sin(pitch). */
void CreateYPRMatrix(f32 matrix[3][3], f32 pyr[3]) {
    f32 cos_pitch, sin_pitch, cos_yaw, sin_yaw, cos_roll, sin_roll, product;
    cos_pitch = cosf(pyr[0]);
    sin_pitch = sinf(pyr[0]);
    cos_yaw = cosf(pyr[1]);
    sin_yaw = sinf(pyr[1]);
    cos_roll = cosf(pyr[2]);
    sin_roll = sinf(pyr[2]);
    product = sin_yaw * sin_pitch;
    matrix[2][1] = sin_pitch;
    matrix[0][0] = cos_yaw * cos_roll - product * sin_roll;
    matrix[1][0] = cos_yaw * sin_roll + product * cos_roll;
    matrix[2][0] = -sin_yaw * cos_pitch;
    matrix[0][1] = -cos_pitch * sin_roll;
    matrix[1][1] = cos_pitch * cos_roll;
    product = cos_yaw * sin_pitch;
    matrix[0][2] = sin_yaw * cos_roll + product * sin_roll;
    matrix[1][2] = sin_yaw * sin_roll - product * cos_roll;
    matrix[2][2] = cos_yaw * cos_pitch;
}
