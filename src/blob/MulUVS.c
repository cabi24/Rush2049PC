#include "game/game.h"

void MulUVS(f32 m[3][3], f32 in[3][3], f32 out[3][3]) {
    s32 i;

    for (i = 0; i < 3; i++) {
        out[i][0] = in[i][0] * m[0][0] + in[i][1] * m[1][0] + in[i][2] * m[2][0];
        out[i][1] = in[i][0] * m[0][1] + in[i][1] * m[1][1] + in[i][2] * m[2][1];
        out[i][2] = in[i][0] * m[0][2] + in[i][1] * m[1][2] + in[i][2] * m[2][2];
    }
}
