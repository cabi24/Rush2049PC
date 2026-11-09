#include "game/game.h"

f32 fatan2(f32 y, f32 x) {
    if (y == y + x) {
        if (y == 0.0f) {
            return 0;
        }
        if (y > 0.0f) {
            return atan2_pio2;
        }
        return atan2_neg_pio2;
    }
    if (x < 0.0f) {
        if (y >= 0.0f) {
            return atan2_pi - fsatan(-y / x);
        }
        return fsatan(y / x) + atan2_neg_pi;
    }
    if (y > 0) {
        return fatan(y / x);
    }
    return -fatan(-y / x);
}
