#include "game/game.h"

void froll(f32 sine, f32 cosine, f32 m[3][3]) {
    f32 result;
    result = m[0][0] * cosine - m[1][0] * sine;
    m[1][0] = m[1][0] * cosine + m[0][0] * sine;
    m[0][0] = result;
    result = m[0][1] * cosine - m[1][1] * sine;
    m[1][1] = m[1][1] * cosine + m[0][1] * sine;
    m[0][1] = result;
    result = m[0][2] * cosine - m[1][2] * sine;
    m[1][2] = m[1][2] * cosine + m[0][2] * sine;
    m[0][2] = result;
}
