#include "game/game.h"

extern f32 cosf(f32);
extern f32 sinf(f32);
void CreateYPRMatrix(f32 matrix[3][3], f32 angle[3]) {
    f32 cx, sx, cy, sy, cz, sz, product;
    cx = cosf(angle[0]);
    sx = sinf(angle[0]);
    cy = cosf(angle[1]);
    sy = sinf(angle[1]);
    cz = cosf(angle[2]);
    sz = sinf(angle[2]);
    product = sy * sx;
    matrix[2][1] = sx;
    matrix[0][0] = cy * cz - product * sz;
    matrix[1][0] = cy * sz + product * cz;
    matrix[2][0] = -sy * cx;
    matrix[0][1] = -cx * sz;
    matrix[1][1] = cx * cz;
    product = cy * sx;
    matrix[0][2] = sy * cz + product * sz;
    matrix[1][2] = sy * sz - product * cz;
    matrix[2][2] = cy * cx;
}
