#include "game/game.h"

VIEW(f32[3], rvector);
#define rvector rvector_view

void LookInDir(f32 *dir, f32 m[3][3]) {
    f32 len;
    f32 inv;

    m[2][0] = dir[0];
    m[2][1] = dir[1];
    m[2][2] = dir[2];
    vector_direction(m[2], m[2]);
    m[0][0] = dir[2];
    m[0][1] = 0.0f;
    m[0][2] = -dir[0];
    len = vector_magnitude(m[0]);
    if (len <= 0.01f) {
        m[0][0] = rvector[0];
        m[0][1] = rvector[1];
        m[0][2] = rvector[2];
    } else {
        inv = 1.0f / len;
        m[0][0] *= inv;
        m[0][1] *= inv;
        m[0][2] *= inv;
    }
    m[1][0] = m[2][1] * m[0][2] - m[2][2] * m[0][1];
    m[1][1] = m[2][2] * m[0][0] - m[2][0] * m[0][2];
    m[1][2] = m[2][0] * m[0][1] - m[2][1] * m[0][0];
    m[0][0] = m[1][1] * m[2][2] - m[1][2] * m[2][1];
    m[0][1] = m[1][2] * m[2][0] - m[1][0] * m[2][2];
    m[0][2] = m[1][0] * m[2][1] - m[1][1] * m[2][0];
}
