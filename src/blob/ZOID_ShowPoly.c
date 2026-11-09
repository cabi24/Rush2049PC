#include "game/game.h"

void ZOID_ShowPoly(void *arg0) {
    M2C_FIELD(arg0, u16 *, 2) = (u16) (M2C_FIELD(arg0, u16 *, 2) & 0x7FFF);
}
