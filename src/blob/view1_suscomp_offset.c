/* view1_suscomp_offset: the driver's eye rides with the front suspension (MAT3 camera.c). */
#include "game/game.h"

/* The model record (MAT3 tModelData): the front tyres' suspension compression. */
struct tModelData {
    _tCar *car;
    Tune44 *tune;
    u8 pad8[0x398];
    streetyp *lasttp[4];
    u8 pad3B0[0x5C];
    const s16 *torquecurve;
    u8 pad410[0x238];
    surftyp *movingSurface;
    u8 pad64C[0x110];
    f32 suscomp[2]; /* 0x75C reckon.suscomp: the front tyres' suspension compression, feet */
};

/* Returns how far the driver's eye of model `m` rises: the front tyres' mean compression beyond
 * view1_suscomp_bias, 0 if less. The empty test is the N64 code's. */
f32 view1_suscomp_offset(struct tModelData *m) {
    f32 t = 0.0f;
    if (view1_suscomp_bias) {}
    t = (m->suscomp[0] + m->suscomp[1]) * 0.5f;
    if (view1_suscomp_bias < t) {
        return t - view1_suscomp_bias;
    }
    return 0.0f;
}
