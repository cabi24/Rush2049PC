#include "game/game.h"

void vector_direction(void *arg0, void *arg1) {
    f32 temp_f0;

    temp_f0 = invmagnitude(arg0);
    M2C_FIELD(arg1, f32 *, 0) = (f32) (M2C_FIELD(arg0, f32 *, 0) * temp_f0);
    M2C_FIELD(arg1, f32 *, 4) = (f32) (M2C_FIELD(arg0, f32 *, 4) * temp_f0);
    M2C_FIELD(arg1, f32 *, 8) = (f32) (M2C_FIELD(arg0, f32 *, 8) * temp_f0);
}
