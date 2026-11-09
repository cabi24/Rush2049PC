/* copy_tire_info.c: a tire's constants from the car's descriptor into the model (MAT3 initiali.c,
 * copy_tire_info()). Part of the car set-up (carsetup). */
#include "game/game.h"

/* TireDesc (include/game/types.h): a tire's constants, which the car's physics descriptor's tires[] point at. */

/* A tire in the model record (MAT3 anon_tradius_springK_5c; tModelData.tires[i]): the same six first. */
typedef struct Tire {
    f32 tradius;
    f32 springK;
    f32 rubdamp;
    f32 cstiff;
    f32 cfmax;
    f32 invmi;
} Tire;

/* Copy the six constants of descriptor tire tp1 into model tire tp2. */
void copy_tire_info(TireDesc *tp1, Tire *tp2) {
    tp2->tradius = tp1->tradius;
    tp2->springK = tp1->springK;
    tp2->rubdamp = tp1->rubdamp;
    tp2->cstiff = tp1->cstiff;
    tp2->cfmax = tp1->cfmax;
    tp2->invmi = tp1->invmi;
}
