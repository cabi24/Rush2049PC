#include "game/game.h"

void RotateUV(void *arg0, void *arg1) {
    YawUV(M2C_FIELD(arg0, f32 *, 4), arg1);
    PitchUV(M2C_FIELD(arg0, f32 *, 0), arg1);
    RollUV(M2C_FIELD(arg0, f32 *, 8), arg1);
}
