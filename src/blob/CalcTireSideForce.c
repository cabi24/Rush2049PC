#include "game/game.h"

f32 CalcTireSideForce(void *arg0) {
    return gTireSideForce[M2C_FIELD(arg0, s8 *, 0xD)];
}
