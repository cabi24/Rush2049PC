#include "game/game.h"

f32 fatan(f32 arg0) {
    if (arg0 > 0.0f) {
        return fsatan(arg0);
    }
    return -fsatan(-arg0);
}
