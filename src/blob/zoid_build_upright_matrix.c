/* zoid_build_upright_matrix.c: a camera-facing matrix that stays upright, for the scene draw.
 *
 * zoid_draw builds it once a view (zoid_upright_matrix) for the scene nodes flagged to turn to face the camera
 * about the vertical only (node flag 0x8000; zoid_view_matrix is the one for nodes that face it fully). */
#include "game/game.h"

float sqrtf(float);
#pragma intrinsic (sqrtf)

/* Fill the rotation part of the 4x4 float matrix m (16 floats, row by row; the rest is left as it is) with a yaw
 * that turns an object to face the camera: the camera's view direction from the frame's lookat matrix (its
 * elements [0][2] and [2][2]: the x and z of the view axis) flattened onto the ground and normalised. Should the view point
 * straight up or down (that length under upright_min_length) the direction is taken as (0, 1). */
void zoid_build_upright_matrix(f32 *m) {
    f32 a, b, len;

    a = zoid_lookat_matrix[0][2];
    b = zoid_lookat_matrix[2][2];
    len = sqrtf(a * a + b * b);
    if (len < upright_min_length) {
        a = 0.0f;
        b = 1.0f;
    } else {
        a /= len;
        b /= len;
    }
    m[0] = -b;
    m[4] = 0.0f;
    m[8] = -a;
    m[1] = 0.0f;
    m[9] = 0.0f;
    m[2] = a;
    m[6] = 0.0f;
    m[10] = -b;
    m[5] = 1.0f;
}
