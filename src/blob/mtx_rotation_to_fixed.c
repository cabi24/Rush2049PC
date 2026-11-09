#include "game/game.h"

void mtx_rotation_to_fixed(f32 matrix[4][4], u32 *fixed) {
    s32 i, first, second;
    u32 *integer = fixed;

    for (i = 0; i < 3; i++) {
        first = matrix[i][0] * 65536.0f;
        second = matrix[i][1] * 65536.0f;
        *integer++ = (first & 0xffff0000) | ((u32)second >> 16);
        integer[7] = (first << 16) | (second & 0xffff);
        first = matrix[i][2] * 65536.0f;
        *integer++ = first & 0xffff0000;
        integer[7] = first << 16;
    }
    integer[0] = 0;
    integer[8] = 0;
    integer[1] = 1;
    integer[9] = 0;
}
