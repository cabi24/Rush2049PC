#include "game/game.h"

void copy_tire_info(void *arg0, void *arg1) {
    M2C_FIELD(arg1, f32 *, 0) = (f32) M2C_FIELD(arg0, f32 *, 0);
    M2C_FIELD(arg1, f32 *, 4) = (f32) M2C_FIELD(arg0, f32 *, 4);
    M2C_FIELD(arg1, f32 *, 8) = (f32) M2C_FIELD(arg0, f32 *, 8);
    M2C_FIELD(arg1, f32 *, 0xC) = (f32) M2C_FIELD(arg0, f32 *, 0xC);
    M2C_FIELD(arg1, f32 *, 0x10) = (f32) M2C_FIELD(arg0, f32 *, 0x10);
    M2C_FIELD(arg1, f32 *, 0x14) = (f32) M2C_FIELD(arg0, f32 *, 0x14);
}
