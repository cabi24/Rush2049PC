#include "game/game.h"

f32 view1_suscomp_offset(void *arg0) {
    f32 t = 0.0f;
    if (view1_suscomp_bias) {}
    t = (*(f32 *) ((u8 *) arg0 + 0x75C) + *(f32 *) ((u8 *) arg0 + 0x760)) * 0.5f;
    if (view1_suscomp_bias < t) {
        return t - view1_suscomp_bias;
    }
    return 0.0f;
}
