#include "game/game.h"

void InitBST(void *arg0, s32 arg1) {
    M2C_FIELD(arg0, s32 *, 8) = 0;
    M2C_FIELD(arg0, s32 *, 4) = 0;
    M2C_FIELD(arg0, s32 *, 0) = arg1;
}
