#include "game/game.h"

void fpitch(f32 sine, f32 cosine, f32 m[3][3]) {
    f32 result;
    result = m[1][0] * cosine - m[2][0] * sine;
    m[2][0] = m[2][0] * cosine + m[1][0] * sine;
    m[1][0] = result;
    result = m[1][1] * cosine - m[2][1] * sine;
    m[2][1] = m[2][1] * cosine + m[1][1] * sine;
    m[1][1] = result;
    result = m[1][2] * cosine - m[2][2] * sine;
    m[2][2] = m[2][2] * cosine + m[1][2] * sine;
    m[1][2] = result;
}
