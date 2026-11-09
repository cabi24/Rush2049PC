#include "game/game.h"

void InitSoundState(void *arg0) {
    M2C_FIELD(arg0, s32 *, 0) = -1;
    M2C_FIELD(arg0, f32 *, 4) = -2.0f;
    M2C_FIELD(arg0, f32 *, 8) = -2.0f;
    M2C_FIELD(arg0, f32 *, 0xC) = -2.0f;
    M2C_FIELD(arg0, f32 *, 0x10) = -2.0f;
}
