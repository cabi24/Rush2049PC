#include "game/game.h"

void upshift(void *arg0) {
    f32 temp_f0;
    s16 temp_v0;

    temp_v0 = M2C_FIELD(arg0, s16 *, 0x3F4);
    if (temp_v0 < 4) {
        temp_f0 = M2C_FIELD(arg0, f32 *, 0x714);
        if (M2C_FIELD(arg0, f32 *, 0x658) < temp_f0) {
            M2C_FIELD(arg0, s16 *, 0x3F4) = (s16) (temp_v0 + 1);
            M2C_FIELD(arg0, f32 *, 0x658) = (f32) (temp_f0 + 1.0f);
        }
    }
}
