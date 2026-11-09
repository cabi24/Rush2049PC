#include "game/game.h"

s32 PointInBody(void *arg0, void *arg1, f32 arg2) {
    f32 temp_f0;
    f32 temp_f0_2;
    f32 temp_f0_3;

    temp_f0 = M2C_FIELD(arg1, f32 *, 0);
    if ((M2C_FIELD(arg0, f32 *, 0xF4) < temp_f0) || (temp_f0 < M2C_FIELD(arg0, f32 *, 0x118))) {
        return 0;
    }
    temp_f0_2 = M2C_FIELD(arg1, f32 *, 4);
    if ((arg2 < temp_f0_2) || (temp_f0_2 < -1.0f)) {
        return 0;
    }
    temp_f0_3 = M2C_FIELD(arg1, f32 *, 8);
    if ((M2C_FIELD(arg0, f32 *, 0xFC) < temp_f0_3) || (temp_f0_3 < M2C_FIELD(arg0, f32 *, 0x120))) {
        return 0;
    }
    return 1;
}
