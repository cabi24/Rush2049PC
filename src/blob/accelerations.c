#include "game/game.h"

void accelerations(f32 *arg0) {
    f32 t;

    t = gGravity / arg0[0x5BC / 4];
    arg0[0x28 / 4] = arg0[0x10 / 4] * t;
    arg0[0x2C / 4] = arg0[0x14 / 4] * t;
    arg0[0x30 / 4] = arg0[0x18 / 4] * t;
    calcaa(arg0, arg0 + 7, arg0 + 0xD);
}
