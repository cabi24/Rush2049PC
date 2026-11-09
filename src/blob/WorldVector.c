#include "game/game.h"

void WorldVector(f32 *arg0, f32 *arg1, f32 *arg2) {
    arg1[0] = (arg0[0] * arg2[0] + arg0[1] * arg2[3]) + arg0[2] * arg2[6];
    arg1[1] = (arg0[0] * arg2[1] + arg0[1] * arg2[4]) + arg0[2] * arg2[7];
    arg1[2] = (arg0[0] * arg2[2] + arg0[1] * arg2[5]) + arg0[2] * arg2[8];
}
