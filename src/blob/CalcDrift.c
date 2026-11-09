#include "game/game.h"

VIEW(f32[5][3], gTireDrift);
#define gTireDrift gTireDrift_view[0][0]

VIEW(s8, gCarSuspensions);
#define gCarSuspensions gCarSuspensions_view

f32 CalcDrift(void *arg0) {
    f32 var_f2;
    s8 temp_v0;

    var_f2 = ((f32 (*)[3]) &gTireDrift)[M2C_FIELD(arg0, s8 *, 0xD)][M2C_FIELD(arg0, s8 *, 0xB)];
    temp_v0 = (&gCarSuspensions)[M2C_FIELD(arg0, u8 *, 8)];
    if (temp_v0 == 0) {
        var_f2 += drift_soft_suspension_bonus;
    } else if (temp_v0 == 2) {
        var_f2 -= drift_stiff_suspension_penalty;
    }
    return var_f2;
}
