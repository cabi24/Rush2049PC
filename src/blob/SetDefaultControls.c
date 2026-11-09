#include "game/game.h"

void SetDefaultControls(void *arg0) {
    M2C_FIELD(arg0, s8 *, 0) = 1;
    M2C_FIELD(arg0, s8 *, 1) = 2;
    M2C_FIELD(arg0, s8 *, 2) = 0x13;
    M2C_FIELD(arg0, s8 *, 3) = 9;
    M2C_FIELD(arg0, s8 *, 4) = 0xF;
    M2C_FIELD(arg0, s8 *, 5) = 0x10;
    M2C_FIELD(arg0, s8 *, 6) = 0xE;
    M2C_FIELD(arg0, s8 *, 7) = 8;
    M2C_FIELD(arg0, s8 *, 8) = 0xD;
    M2C_FIELD(arg0, s8 *, 9) = 7;
}
