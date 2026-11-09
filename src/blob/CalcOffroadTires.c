#include "game/game.h"

f32 CalcOffroadTires(void *arg0) {
    s8 temp_v0;

    temp_v0 = M2C_FIELD(arg0, s8 *, 0xD);
    if (temp_v0 == 4) {
        return 0.75f;
    }
    if (temp_v0 == 3) {
        return 0.5f;
    }
    if (temp_v0 == 1) {
        return -0.25f;
    }
    if (temp_v0 == 2) {
        return -0.5f;
    }
    return 0.0f;
}
