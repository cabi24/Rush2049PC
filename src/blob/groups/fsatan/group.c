#include "game/game.h"

f32 fsatan(f32 arg0) {
    if (arg0 < atan_sq2m1) {
        return fxatan(arg0);
    }
    if (atan_sq2p1 < arg0) {
        return atan_pio2 - fxatan(1.0f / arg0);
    }
    return fxatan((arg0 - 1.0f) / (arg0 + 1.0f)) + atan_pio4;
}
