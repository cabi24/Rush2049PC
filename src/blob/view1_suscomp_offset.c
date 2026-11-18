/* view1_suscomp_offset: the driver's eye rides with the front suspension (MAT3 camera.c). */
#include "game/game.h"

/* Returns how far the driver's eye of model `m` rises: the front tyres' mean compression beyond
 * view1_suscomp_bias, 0 if less. The empty test is the N64 code's. */
f32 view1_suscomp_offset(struct tModelData *m) {
    f32 t = 0.0f;
    if (view1_suscomp_bias) {}
    t = (m->reckon.suscomp[0] + m->reckon.suscomp[1]) * 0.5f;
    if (view1_suscomp_bias < t) {
        return t - view1_suscomp_bias;
    }
    return 0.0f;
}
