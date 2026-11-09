#include "game/game.h"

f32 sqrtf(f32);
#pragma intrinsic (sqrtf)

f32 invsqr(f32 value) {
    if (value < 0.0001f) {
        value = 0.0001f;
    }
    return 1.0f / sqrtf(value);
}
