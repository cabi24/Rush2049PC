#include "game/game.h"

void ZOID_SetPolyZ(void *arg0, s16 arg1) {
    M2C_FIELD(arg0, s16 *, 6) = arg1;
}
